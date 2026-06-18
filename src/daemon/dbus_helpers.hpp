#pragma once

#include "../core/common.hpp"
#include <string>

namespace sasayaku {

// Map the coordinator's recording state to the GetStatus D-Bus string.
std::string recording_state_to_string(RecordingState state);

} // namespace sasayaku
