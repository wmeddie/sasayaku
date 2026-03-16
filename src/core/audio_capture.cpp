#include "audio_capture.hpp"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <cstring>
#include <iostream>

namespace sasayaku {

// PipeWire stream callback
static void on_process(void* userdata) {
    auto* capture = static_cast<AudioCapture*>(userdata);

    pw_buffer* buf = pw_stream_dequeue_buffer(capture->get_stream());
    if (!buf) {
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    spa_data* data = &spa_buf->datas[0];

    if (data->data) {
        const float* samples = static_cast<const float*>(data->data);
        uint32_t n_samples = data->chunk->size / sizeof(float);

        if (n_samples > 0) {
            capture->on_process(samples, n_samples);
        }
    }

    pw_stream_queue_buffer(capture->get_stream(), buf);
}

static const pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = ::sasayaku::on_process,
};

AudioCapture::AudioCapture() {
}

AudioCapture::~AudioCapture() {
    cleanup();
}

void AudioCapture::cleanup() {
    if (is_recording_) {
        stop_recording();
    }

    if (loop_) {
        pw_thread_loop_lock(loop_);
    }

    if (stream_) {
        pw_stream_destroy(stream_);
        stream_ = nullptr;
    }

    if (loop_) {
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }

    if (context_) {
        pw_context_destroy(context_);
        context_ = nullptr;
    }
}

bool AudioCapture::initialize(const AudioCaptureConfig& config) {
    config_ = config;

    // Just store config, we'll initialize PipeWire lazily on first recording
    return true;
}

bool AudioCapture::initialize_pipewire() {
    if (loop_ && context_) {
        return true;  // Already initialized
    }

    // Initialize PipeWire
    pw_init(nullptr, nullptr);

    // Create thread loop
    loop_ = pw_thread_loop_new("audio-capture", nullptr);
    if (!loop_) {
        last_error_ = "Failed to create PipeWire thread loop";
        return false;
    }

    // Create context
    context_ = pw_context_new(
        pw_thread_loop_get_loop(loop_),
        nullptr,
        0
    );

    if (!context_) {
        last_error_ = "Failed to create PipeWire context";
        cleanup();
        return false;
    }

    // Start the thread loop
    if (pw_thread_loop_start(loop_) < 0) {
        last_error_ = "Failed to start PipeWire thread loop";
        cleanup();
        return false;
    }

    std::cout << "PipeWire initialized successfully" << std::endl;
    return true;
}

bool AudioCapture::start_recording(AudioDataCallback callback) {
    if (is_recording_) {
        return true;
    }

    // Initialize PipeWire on first use
    if (!initialize_pipewire()) {
        return false;
    }

    data_callback_ = callback;
    buffer_.clear();

    pw_thread_loop_lock(loop_);

    // Create stream properties
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

    // Create stream
    stream_ = pw_stream_new_simple(
        pw_thread_loop_get_loop(loop_),
        "sasayaku-capture",
        props,
        &stream_events,
        this
    );

    if (!stream_) {
        pw_thread_loop_unlock(loop_);
        last_error_ = "Failed to create PipeWire stream";
        return false;
    }

    // Set audio format
    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const spa_pod* params[1];
    spa_audio_info_raw audio_info = {};
    audio_info.format = SPA_AUDIO_FORMAT_F32;
    audio_info.channels = config_.channels;
    audio_info.rate = config_.sample_rate;

    params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

    // Connect stream
    enum pw_stream_flags flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS
    );

    enum pw_direction direction = config_.source == AudioSource::SYSTEM_AUDIO ?
        PW_DIRECTION_INPUT : PW_DIRECTION_INPUT;

    int result = pw_stream_connect(
        stream_,
        direction,
        PW_ID_ANY,
        flags,
        params,
        1
    );

    pw_thread_loop_unlock(loop_);

    if (result < 0) {
        last_error_ = "Failed to connect PipeWire stream: error code " + std::to_string(result);
        cleanup();
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

    if (stream_ && loop_) {
        pw_thread_loop_lock(loop_);
        pw_stream_set_active(stream_, false);
        pw_thread_loop_unlock(loop_);
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

    // Add to buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), samples, samples + count);
    }

    // Call user callback if provided
    if (data_callback_) {
        data_callback_(samples, count);
    }
}

} // namespace sasayaku
