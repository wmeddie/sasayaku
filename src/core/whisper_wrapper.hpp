#pragma once

#include "common.hpp"
#include <string>
#include <memory>

// Forward declare whisper.cpp types
struct whisper_context;
struct whisper_state;

namespace sasayaku {

struct WhisperConfig {
    std::string model_path;
    bool use_gpu = true;
    int gpu_device = 0;
    int n_threads = 4;
    bool translate = false;
    std::string language = "en";
    float temperature = 0.0f;
    bool print_progress = false;
};

class WhisperWrapper {
public:
    WhisperWrapper();
    ~WhisperWrapper();

    // Initialize whisper.cpp with model
    bool initialize(const WhisperConfig& config);

    // Check if initialized
    bool is_initialized() const { return ctx_ != nullptr; }

    // Transcribe audio buffer
    TranscriptionResult transcribe(const AudioBuffer& audio);

    // Transcribe with progress callback
    TranscriptionResult transcribe(const AudioBuffer& audio, ProgressCallback progress_cb);

    // Get last error
    std::string get_last_error() const { return last_error_; }

private:
    whisper_context* ctx_ = nullptr;
    WhisperConfig config_;
    std::string last_error_;

    void cleanup();
};

} // namespace sasayaku
