#pragma once

#include "common.hpp"
#include <atomic>
#include <mutex>
#include <memory>
#include <string>

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

    // Initialize (stores config, lazy platform init on first recording)
    bool initialize(const AudioCaptureConfig& config);

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

    // Called by platform implementation to deliver audio data
    void on_process(const float* samples, size_t count);

private:
    // Platform-specific implementation (defined in audio_capture_pipewire.cpp or audio_capture_macos.mm)
    struct PlatformImpl;
    std::unique_ptr<PlatformImpl> platform_;

    AudioCaptureConfig config_;
    std::atomic<bool> is_recording_{false};
    std::atomic<bool> should_stop_{false};

    AudioBuffer buffer_;
    mutable std::mutex buffer_mutex_;

    AudioDataCallback data_callback_;
    std::string last_error_;

    bool initialize_platform();
    void cleanup_platform();
};

} // namespace sasayaku
