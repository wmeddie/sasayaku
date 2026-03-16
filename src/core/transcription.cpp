#include "transcription.hpp"
#include <thread>
#include <iostream>
#include <cmath>

namespace sasayaku {

TranscriptionEngine::TranscriptionEngine() {
}

TranscriptionEngine::~TranscriptionEngine() {
}

bool TranscriptionEngine::initialize(const TranscriptionConfig& config) {
    config_ = config;

    // Initialize whisper
    whisper_ = std::make_unique<WhisperWrapper>();
    if (!whisper_->initialize(config.whisper)) {
        last_error_ = "Failed to initialize whisper: " + whisper_->get_last_error();
        return false;
    }

    // Initialize audio capture
    audio_capture_ = std::make_unique<AudioCapture>();
    if (!audio_capture_->initialize(config.audio)) {
        last_error_ = "Failed to initialize audio capture: " + audio_capture_->get_last_error();
        return false;
    }

    state_ = RecordingState::STOPPED;
    return true;
}

bool TranscriptionEngine::start_recording() {
    if (state_ != RecordingState::STOPPED) {
        last_error_ = "Already recording or processing";
        return false;
    }

    // Clear previous recording
    audio_capture_->clear_buffer();
    samples_recorded_ = 0;
    current_audio_level_ = 0.0f;

    // Start capture with callback to update audio level
    auto callback = [this](const float* samples, size_t count) {
        this->on_audio_data(samples, count);
    };

    if (!audio_capture_->start_recording(callback)) {
        last_error_ = "Failed to start recording: " + audio_capture_->get_last_error();
        return false;
    }

    state_ = RecordingState::RECORDING;
    return true;
}

void TranscriptionEngine::stop_recording(TranscriptionCallback callback) {
    if (state_ != RecordingState::RECORDING) {
        if (callback) {
            TranscriptionResult result;
            result.success = false;
            result.error_message = "Not currently recording";
            callback(result);
        }
        return;
    }

    state_ = RecordingState::PROCESSING;

    // Stop audio capture
    audio_capture_->stop_recording();

    // Get recorded audio
    AudioBuffer raw_audio = audio_capture_->get_buffer();

    // Process in background thread
    std::thread([this, raw_audio, callback]() mutable {
        // Process audio
        AudioBuffer processed = this->process_audio(raw_audio);

        // Transcribe
        TranscriptionResult result = whisper_->transcribe(processed);

        // Update state
        this->state_ = RecordingState::STOPPED;

        // Call callback
        if (callback) {
            callback(result);
        }
    }).detach();
}

float TranscriptionEngine::get_recording_duration() const {
    if (!audio_capture_) {
        return 0.0f;
    }

    return static_cast<float>(samples_recorded_) / config_.audio.sample_rate;
}

float TranscriptionEngine::get_audio_level() const {
    return current_audio_level_.load();
}

void TranscriptionEngine::on_audio_data(const float* samples, size_t count) {
    samples_recorded_ += count;

    // Calculate RMS for audio level visualization
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum += samples[i] * samples[i];
    }
    float rms = std::sqrt(sum / count);

    // Smooth audio level with exponential moving average
    float alpha = 0.3f;
    float prev_level = current_audio_level_.load();
    current_audio_level_.store(prev_level * (1.0f - alpha) + rms * alpha);
}

AudioBuffer TranscriptionEngine::process_audio(const AudioBuffer& raw_audio) {
    AudioBuffer processed = raw_audio;

    // Apply high-pass filter to remove low-frequency noise
    if (config_.apply_filters) {
        AudioProcessor::high_pass_filter(processed);
    }

    // Trim silence from beginning and end
    if (config_.auto_trim_silence) {
        processed = AudioProcessor::trim_silence(processed);
    }

    // Normalize audio levels
    if (config_.normalize_audio) {
        AudioProcessor::normalize(processed);
    }

    return processed;
}

TranscriptionResult TranscriptionEngine::transcribe_file(const std::string& file_path) {
    // TODO: Implement file loading using miniaudio or similar
    // For now, return an error
    TranscriptionResult result;
    result.success = false;
    result.error_message = "File transcription not yet implemented";
    return result;
}

} // namespace sasayaku
