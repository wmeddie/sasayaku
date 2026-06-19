#include "recording_coordinator.hpp"
#include "../utils/clipboard.hpp"
#include <iostream>

namespace sasayaku {

RecordingCoordinator::RecordingCoordinator() {
    engine_ = std::make_unique<TranscriptionEngine>();
}

RecordingCoordinator::~RecordingCoordinator() {
}

bool RecordingCoordinator::initialize(ConfigManager* config_manager, ModeManager* mode_manager) {
    config_manager_ = config_manager;
    mode_manager_ = mode_manager;

    // Build transcription config from app config
    TranscriptionConfig trans_config;
    trans_config.whisper = config_manager->get_config().whisper;
    trans_config.audio = config_manager->get_config().audio;
    trans_config.auto_trim_silence = true;
    trans_config.normalize_audio = true;
    trans_config.apply_filters = true;

    if (!engine_->initialize(trans_config)) {
        std::cerr << "Failed to initialize transcription engine: "
                  << engine_->get_last_error() << std::endl;
        return false;
    }

    return true;
}

bool RecordingCoordinator::start_recording() {
    return engine_->start_recording();
}

void RecordingCoordinator::stop_recording(ProcessingCompleteCallback callback) {
    // Get clipboard content if needed for super mode
    std::string clipboard_content;
    auto mode_opt = config_manager_->get_mode(mode_manager_->get_current_mode());
    if (mode_opt.has_value() && mode_opt->requires_clipboard) {
        clipboard_content = Clipboard::get_text();
    }

    engine_->stop_recording([this, callback, clipboard_content](const TranscriptionResult& result) {
        this->on_transcription_complete(result, callback);
    });
}

void RecordingCoordinator::toggle_recording(ProcessingCompleteCallback callback) {
    if (engine_->get_state() == RecordingState::RECORDING) {
        stop_recording(callback);
    } else if (engine_->get_state() == RecordingState::STOPPED) {
        start_recording();
        if (callback) {
            callback("", true);  // Signal that recording started successfully
        }
    }
}

RecordingState RecordingCoordinator::get_state() const {
    return engine_->get_state();
}

float RecordingCoordinator::get_recording_duration() const {
    return engine_->get_recording_duration();
}

float RecordingCoordinator::get_audio_level() const {
    return engine_->get_audio_level();
}

void RecordingCoordinator::on_transcription_complete(
    const TranscriptionResult& result,
    ProcessingCompleteCallback callback
) {
    if (!result.success) {
        std::cerr << "Transcription failed: " << result.error_message << std::endl;
        if (callback) {
            callback("", false);
        }
        return;
    }

    std::cout << "Transcription: " << result.full_text << std::endl;

    // Get clipboard content for super mode
    std::string clipboard_content;
    auto mode_opt = config_manager_->get_mode(mode_manager_->get_current_mode());
    if (mode_opt.has_value() && mode_opt->requires_clipboard) {
        clipboard_content = Clipboard::get_text();
    }

    // Process with mode manager
    std::string processed_text = mode_manager_->process_transcript(
        result.full_text,
        clipboard_content
    );

    std::cout << "Processed text: " << processed_text << std::endl;

    // Clipboard + paste are handled by the GNOME Shell extension (St.Clipboard);
    // a headless daemon has no GDK display to write the system clipboard itself.
    std::cout << "Transcription ready (" << processed_text.length() << " chars)" << std::endl;

    if (callback) {
        callback(processed_text, true);
    }
}

} // namespace sasayaku
