#include "dbus_helpers.hpp"

namespace sasayaku {

std::string recording_state_to_string(RecordingState state) {
    switch (state) {
        case RecordingState::RECORDING:  return "recording";
        case RecordingState::PROCESSING: return "processing";
        case RecordingState::STOPPED:    return "stopped";
        case RecordingState::ERROR:      return "error";
    }
    return "unknown";
}

} // namespace sasayaku
