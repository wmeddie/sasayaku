#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace sasayaku {

// Audio format constants
constexpr int WHISPER_SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;  // Mono

// Audio buffer type
using AudioBuffer = std::vector<float>;

// Transcription result
struct TranscriptionSegment {
    int64_t start_ms;
    int64_t end_ms;
    std::string text;
    float confidence;
};

struct TranscriptionResult {
    std::vector<TranscriptionSegment> segments;
    std::string full_text;
    int64_t duration_ms;
    bool success;
    std::string error_message;
};

// Callbacks
using AudioDataCallback = std::function<void(const float* samples, size_t count)>;
using TranscriptionCallback = std::function<void(const TranscriptionResult&)>;
using ProgressCallback = std::function<void(int progress_percent)>;

// Recording state
enum class RecordingState {
    STOPPED,
    RECORDING,
    PROCESSING,
    ERROR
};

} // namespace sasayaku
