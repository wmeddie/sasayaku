#include "audio_capture.hpp"

#if defined(__APPLE__)
// macOS implementation is in AudioCaptureMacOS.mm
#elif defined(_WIN32)
// Windows implementation is in AudioCaptureWindows.cpp
#else

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <cstring>
#include <iostream>

namespace sasayaku {

// Platform-specific PipeWire implementation
struct AudioCapture::PlatformImpl {
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_stream* stream = nullptr;
};

// We need a wrapper to pass both capture and stream
struct PipeWireCallbackData {
    AudioCapture* capture;
    pw_stream* stream;
};

static void on_process_callback(void* userdata) {
    auto* data = static_cast<PipeWireCallbackData*>(userdata);

    pw_buffer* buf = pw_stream_dequeue_buffer(data->stream);
    if (!buf) {
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    spa_data* spa_d = &spa_buf->datas[0];

    if (spa_d->data) {
        const float* samples = static_cast<const float*>(spa_d->data);
        uint32_t n_samples = spa_d->chunk->size / sizeof(float);

        if (n_samples > 0) {
            data->capture->on_process(samples, n_samples);
        }
    }

    pw_stream_queue_buffer(data->stream, buf);
}

static const pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = on_process_callback,
};

// Store callback data globally per-capture (only one stream per AudioCapture)
static PipeWireCallbackData g_callback_data;

AudioCapture::AudioCapture()
    : platform_(std::make_unique<PlatformImpl>()) {
}

AudioCapture::~AudioCapture() {
    cleanup_platform();
}

void AudioCapture::cleanup_platform() {
    if (is_recording_) {
        stop_recording();
    }

    if (platform_->loop) {
        pw_thread_loop_lock(platform_->loop);
    }

    if (platform_->stream) {
        pw_stream_destroy(platform_->stream);
        platform_->stream = nullptr;
    }

    if (platform_->loop) {
        pw_thread_loop_unlock(platform_->loop);
        pw_thread_loop_stop(platform_->loop);
        pw_thread_loop_destroy(platform_->loop);
        platform_->loop = nullptr;
    }

    if (platform_->context) {
        pw_context_destroy(platform_->context);
        platform_->context = nullptr;
    }
}

bool AudioCapture::initialize(const AudioCaptureConfig& config) {
    config_ = config;
    return true;
}

bool AudioCapture::initialize_platform() {
    if (platform_->loop && platform_->context) {
        return true;  // Already initialized
    }

    pw_init(nullptr, nullptr);

    platform_->loop = pw_thread_loop_new("audio-capture", nullptr);
    if (!platform_->loop) {
        last_error_ = "Failed to create PipeWire thread loop";
        return false;
    }

    platform_->context = pw_context_new(
        pw_thread_loop_get_loop(platform_->loop),
        nullptr,
        0
    );

    if (!platform_->context) {
        last_error_ = "Failed to create PipeWire context";
        cleanup_platform();
        return false;
    }

    if (pw_thread_loop_start(platform_->loop) < 0) {
        last_error_ = "Failed to start PipeWire thread loop";
        cleanup_platform();
        return false;
    }

    std::cout << "PipeWire initialized successfully" << std::endl;
    return true;
}

bool AudioCapture::start_recording(AudioDataCallback callback) {
    if (is_recording_) {
        return true;
    }

    if (!initialize_platform()) {
        return false;
    }

    data_callback_ = callback;
    buffer_.clear();

    pw_thread_loop_lock(platform_->loop);

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Voice",
        nullptr
    );

    if (config_.source == AudioSource::SYSTEM_AUDIO) {
        pw_properties_set(props, PW_KEY_MEDIA_CATEGORY, "Playback");
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }

    // Set up callback data
    g_callback_data.capture = this;

    platform_->stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(platform_->loop),
        "sasayaku-capture",
        props,
        &stream_events,
        &g_callback_data
    );

    if (!platform_->stream) {
        pw_thread_loop_unlock(platform_->loop);
        last_error_ = "Failed to create PipeWire stream";
        return false;
    }

    g_callback_data.stream = platform_->stream;

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const spa_pod* params[1];
    spa_audio_info_raw audio_info = {};
    audio_info.format = SPA_AUDIO_FORMAT_F32;
    audio_info.channels = config_.channels;
    audio_info.rate = config_.sample_rate;

    params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

    enum pw_stream_flags flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS
    );

    int result = pw_stream_connect(
        platform_->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        flags,
        params,
        1
    );

    pw_thread_loop_unlock(platform_->loop);

    if (result < 0) {
        last_error_ = "Failed to connect PipeWire stream: error code " + std::to_string(result);
        cleanup_platform();
        return false;
    }

    is_recording_ = true;
    should_stop_ = false;

    return true;
}

bool AudioCapture::stop_recording() {
    if (!is_recording_) {
        return true;
    }

    should_stop_ = true;
    is_recording_ = false;

    if (platform_->stream && platform_->loop) {
        pw_thread_loop_lock(platform_->loop);
        pw_stream_set_active(platform_->stream, false);
        pw_thread_loop_unlock(platform_->loop);
    }

    return true;
}

AudioBuffer AudioCapture::get_buffer() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return buffer_;
}

void AudioCapture::clear_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_.clear();
}

void AudioCapture::on_process(const float* samples, size_t count) {
    if (!is_recording_ || should_stop_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), samples, samples + count);
    }

    if (data_callback_) {
        data_callback_(samples, count);
    }
}

} // namespace sasayaku

#endif // platform selection
