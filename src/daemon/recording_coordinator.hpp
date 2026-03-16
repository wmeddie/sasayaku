#pragma once

#include "../core/transcription.hpp"
#include "../utils/config_manager.hpp"
#include "mode_manager.hpp"
#include <functional>
#include <memory>
#include <string>

namespace sasayaku {

// Callback for when processing is complete
using ProcessingCompleteCallback = std::function<void(const std::string& final_text, bool success)>;

class RecordingCoordinator {
public:
    RecordingCoordinator();
    ~RecordingCoordinator();

    // Initialize
    bool initialize(ConfigManager* config_manager, ModeManager* mode_manager);

    // Start recording
    bool start_recording();

    // Stop recording and process
    void stop_recording(ProcessingCompleteCallback callback);

    // Toggle recording
    void toggle_recording(ProcessingCompleteCallback callback);

    // Get current state
    RecordingState get_state() const;

    // Get recording duration
    float get_recording_duration() const;

    // Get audio level
    float get_audio_level() const;

private:
    std::unique_ptr<TranscriptionEngine> engine_;
    ConfigManager* config_manager_ = nullptr;
    ModeManager* mode_manager_ = nullptr;

    void on_transcription_complete(
        const TranscriptionResult& result,
        ProcessingCompleteCallback callback
    );
};

} // namespace sasayaku
