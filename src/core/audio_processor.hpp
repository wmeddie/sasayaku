#pragma once

#include "common.hpp"
#include <vector>

namespace sasayaku {

class AudioProcessor {
public:
    // Resample audio to target sample rate
    static AudioBuffer resample(
        const AudioBuffer& input,
        int input_rate,
        int output_rate
    );

    // Convert stereo to mono
    static AudioBuffer stereo_to_mono(const AudioBuffer& stereo);

    // Normalize audio levels
    static void normalize(AudioBuffer& audio);

    // Apply simple high-pass filter to remove low-frequency noise
    static void high_pass_filter(AudioBuffer& audio, float cutoff_hz = 80.0f);

    // Simple voice activity detection (VAD)
    // Returns true if speech is detected in the buffer
    static bool detect_voice_activity(
        const AudioBuffer& audio,
        float energy_threshold = 0.001f
    );

    // Trim silence from beginning and end
    static AudioBuffer trim_silence(
        const AudioBuffer& audio,
        float energy_threshold = 0.001f,
        int window_samples = 160  // 10ms at 16kHz
    );

    // Calculate RMS energy of audio
    static float calculate_rms(const AudioBuffer& audio);
};

} // namespace sasayaku
