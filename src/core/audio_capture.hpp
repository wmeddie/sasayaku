#pragma once

#include "common.hpp"
#include <atomic>
#include <thread>
#include <mutex>

// Forward declare PipeWire types
struct pw_thread_loop;
struct pw_stream;
struct pw_context;

namespace sasayaku {

enum class AudioSource {
    MICROPHONE,
    SYSTEM_AUDIO,
    BOTH
};

struct AudioCaptureConfig {
    AudioSource source = AudioSource::MICROPHONE;
    int sample_rate = WHISPER_SAMPLE_RATE;
    int channels = CHANNELS;
    std::string device_name;  // Empty for default
};

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // Initialize (just stores config)
    bool initialize(const AudioCaptureConfig& config);

    // Initialize PipeWire (called lazily on first recording)
    bool initialize_pipewire();

    // Start/stop recording
    bool start_recording(AudioDataCallback callback);
    bool stop_recording();

    // Check if recording
    bool is_recording() const { return is_recording_; }

    // Get accumulated audio buffer
    AudioBuffer get_buffer() const;
    void clear_buffer();

    // Get last error
    std::string get_last_error() const { return last_error_; }

private:
    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_stream* stream_ = nullptr;

    AudioCaptureConfig config_;
    std::atomic<bool> is_recording_{false};
    std::atomic<bool> should_stop_{false};

    AudioBuffer buffer_;
    mutable std::mutex buffer_mutex_;

    AudioDataCallback data_callback_;
    std::string last_error_;

    void cleanup();

public:
    void on_process(const float* samples, size_t count);
    pw_stream* get_stream() { return stream_; }
};

} // namespace sasayaku
