#pragma once

#include "common.hpp"
#include "whisper_wrapper.hpp"
#include "audio_capture.hpp"
#include "audio_processor.hpp"
#include <memory>
#include <string>

namespace sasayaku {

struct TranscriptionConfig {
    WhisperConfig whisper;
    AudioCaptureConfig audio;
    bool auto_trim_silence = true;
    bool normalize_audio = true;
    bool apply_filters = true;
};

// High-level transcription coordinator
class TranscriptionEngine {
public:
    TranscriptionEngine();
    ~TranscriptionEngine();

    // Initialize engine
    bool initialize(const TranscriptionConfig& config);

    // Start recording
    bool start_recording();

    // Stop recording and transcribe
    void stop_recording(TranscriptionCallback callback);

    // Transcribe pre-recorded audio file
    TranscriptionResult transcribe_file(const std::string& file_path);

    // Get current recording state
    RecordingState get_state() const { return state_; }

    // Get current recording duration in seconds
    float get_recording_duration() const;

    // Get current audio level (for UI visualization)
    float get_audio_level() const;

    // Is initialized?
    bool is_initialized() const { return whisper_ && audio_capture_; }

    // Get last error
    std::string get_last_error() const { return last_error_; }

private:
    std::unique_ptr<WhisperWrapper> whisper_;
    std::unique_ptr<AudioCapture> audio_capture_;
    TranscriptionConfig config_;

    RecordingState state_ = RecordingState::STOPPED;
    std::string last_error_;

    std::atomic<float> current_audio_level_{0.0f};
    size_t samples_recorded_ = 0;

    void on_audio_data(const float* samples, size_t count);
    AudioBuffer process_audio(const AudioBuffer& raw_audio);
};

} // namespace sasayaku
