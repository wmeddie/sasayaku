#include "dbus_helpers.hpp"
#include "../core/common.hpp"
#include <cassert>
#include <iostream>

using namespace sasayaku;

int main() {
    assert(recording_state_to_string(RecordingState::STOPPED) == "stopped");
    assert(recording_state_to_string(RecordingState::RECORDING) == "recording");
    assert(recording_state_to_string(RecordingState::PROCESSING) == "processing");
    assert(recording_state_to_string(RecordingState::ERROR) == "error");
    std::cout << "dbus_helpers tests passed" << std::endl;
    return 0;
}
