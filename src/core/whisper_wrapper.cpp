#include "whisper_wrapper.hpp"
#include <whisper.h>
#include <iostream>
#include <cstring>

namespace sasayaku {

WhisperWrapper::WhisperWrapper() {
}

WhisperWrapper::~WhisperWrapper() {
    cleanup();
}

void WhisperWrapper::cleanup() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool WhisperWrapper::initialize(const WhisperConfig& config) {
    cleanup();
    config_ = config;

    // Create context params
    whisper_context_params ctx_params = whisper_context_default_params();
    ctx_params.use_gpu = config.use_gpu;
    ctx_params.gpu_device = config.gpu_device;
    ctx_params.flash_attn = config.use_gpu;  // Enable flash attention if GPU is enabled

    // Load model
    ctx_ = whisper_init_from_file_with_params(config.model_path.c_str(), ctx_params);

    if (!ctx_) {
        last_error_ = "Failed to load whisper model from: " + config.model_path;
        return false;
    }

    // Verify GPU if requested
    if (config.use_gpu) {
        std::cout << "Whisper initialized with GPU support on device "
                  << config.gpu_device << std::endl;
    }

    return true;
}

TranscriptionResult WhisperWrapper::transcribe(const AudioBuffer& audio) {
    return transcribe(audio, nullptr);
}

TranscriptionResult WhisperWrapper::transcribe(const AudioBuffer& audio, ProgressCallback progress_cb) {
    TranscriptionResult result;
    result.success = false;

    if (!ctx_) {
        result.error_message = "Whisper not initialized";
        return result;
    }

    if (audio.empty()) {
        result.error_message = "Empty audio buffer";
        return result;
    }

    // Create full params
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.n_threads = config_.n_threads;
    params.translate = config_.translate;
    params.language = config_.language.empty() ? nullptr : config_.language.c_str();
    params.temperature = config_.temperature;
    params.print_progress = config_.print_progress;
    params.print_timestamps = false;
    params.print_realtime = false;
    params.print_special = false;
    params.no_timestamps = false;  // We want timestamps for segments

    // Set progress callback if provided
    if (progress_cb) {
        params.progress_callback = [](struct whisper_context*, struct whisper_state*, int progress, void* user_data) {
            auto* cb = static_cast<ProgressCallback*>(user_data);
            (*cb)(progress);
        };
        params.progress_callback_user_data = &progress_cb;
    }

    // Run transcription
    int ret = whisper_full(ctx_, params, audio.data(), audio.size());

    if (ret != 0) {
        result.error_message = "Transcription failed with code: " + std::to_string(ret);
        return result;
    }

    // Extract segments
    int n_segments = whisper_full_n_segments(ctx_);

    for (int i = 0; i < n_segments; ++i) {
        TranscriptionSegment segment;
        segment.start_ms = whisper_full_get_segment_t0(ctx_, i) * 10;  // Convert to ms
        segment.end_ms = whisper_full_get_segment_t1(ctx_, i) * 10;
        segment.text = whisper_full_get_segment_text(ctx_, i);

        // Skip empty segments
        if (segment.text.empty() || segment.text == " ") {
            continue;
        }

        // Trim leading/trailing whitespace
        size_t start = segment.text.find_first_not_of(" \t\n\r");
        size_t end = segment.text.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            segment.text = segment.text.substr(start, end - start + 1);
        }

        result.segments.push_back(segment);
    }

    // Build full text
    for (const auto& segment : result.segments) {
        if (!result.full_text.empty()) {
            result.full_text += " ";
        }
        result.full_text += segment.text;
    }

    result.duration_ms = audio.size() * 1000 / WHISPER_SAMPLE_RATE;
    result.success = true;

    return result;
}

} // namespace sasayaku
