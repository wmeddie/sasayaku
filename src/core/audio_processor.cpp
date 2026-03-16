#include "audio_processor.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sasayaku {

AudioBuffer AudioProcessor::resample(
    const AudioBuffer& input,
    int input_rate,
    int output_rate
) {
    if (input_rate == output_rate) {
        return input;
    }

    float ratio = static_cast<float>(output_rate) / input_rate;
    size_t output_size = static_cast<size_t>(input.size() * ratio);
    AudioBuffer output(output_size);

    // Simple linear interpolation resampling
    for (size_t i = 0; i < output_size; ++i) {
        float src_pos = i / ratio;
        size_t src_idx = static_cast<size_t>(src_pos);
        float frac = src_pos - src_idx;

        if (src_idx + 1 < input.size()) {
            output[i] = input[src_idx] * (1.0f - frac) + input[src_idx + 1] * frac;
        } else if (src_idx < input.size()) {
            output[i] = input[src_idx];
        }
    }

    return output;
}

AudioBuffer AudioProcessor::stereo_to_mono(const AudioBuffer& stereo) {
    if (stereo.size() % 2 != 0) {
        // Not valid stereo, return as is
        return stereo;
    }

    AudioBuffer mono(stereo.size() / 2);

    for (size_t i = 0; i < mono.size(); ++i) {
        // Average left and right channels
        mono[i] = (stereo[i * 2] + stereo[i * 2 + 1]) * 0.5f;
    }

    return mono;
}

void AudioProcessor::normalize(AudioBuffer& audio) {
    if (audio.empty()) {
        return;
    }

    // Find max absolute value
    float max_val = 0.0f;
    for (float sample : audio) {
        max_val = std::max(max_val, std::abs(sample));
    }

    if (max_val > 0.0f && max_val != 1.0f) {
        float scale = 0.95f / max_val;  // Leave some headroom
        for (float& sample : audio) {
            sample *= scale;
        }
    }
}

void AudioProcessor::high_pass_filter(AudioBuffer& audio, float cutoff_hz) {
    if (audio.size() < 2) {
        return;
    }

    // Simple first-order high-pass filter
    float rc = 1.0f / (2.0f * M_PI * cutoff_hz);
    float dt = 1.0f / WHISPER_SAMPLE_RATE;
    float alpha = rc / (rc + dt);

    float prev_input = audio[0];
    float prev_output = 0.0f;

    for (size_t i = 1; i < audio.size(); ++i) {
        float output = alpha * (prev_output + audio[i] - prev_input);
        prev_input = audio[i];
        prev_output = output;
        audio[i] = output;
    }
}

float AudioProcessor::calculate_rms(const AudioBuffer& audio) {
    if (audio.empty()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (float sample : audio) {
        sum += sample * sample;
    }

    return std::sqrt(sum / audio.size());
}

bool AudioProcessor::detect_voice_activity(
    const AudioBuffer& audio,
    float energy_threshold
) {
    float rms = calculate_rms(audio);
    return rms > energy_threshold;
}

AudioBuffer AudioProcessor::trim_silence(
    const AudioBuffer& audio,
    float energy_threshold,
    int window_samples
) {
    if (audio.size() < static_cast<size_t>(window_samples * 2)) {
        return audio;
    }

    // Find start of speech
    size_t start = 0;
    for (size_t i = 0; i + window_samples < audio.size(); i += window_samples / 2) {
        AudioBuffer window(audio.begin() + i, audio.begin() + i + window_samples);
        if (calculate_rms(window) > energy_threshold) {
            start = i;
            break;
        }
    }

    // Find end of speech
    size_t end = audio.size();
    for (size_t i = audio.size() - window_samples; i > start; i -= window_samples / 2) {
        if (i < start || i + window_samples > audio.size()) {
            break;
        }
        AudioBuffer window(audio.begin() + i, audio.begin() + i + window_samples);
        if (calculate_rms(window) > energy_threshold) {
            end = i + window_samples;
            break;
        }
    }

    // Return trimmed audio
    if (end > start) {
        return AudioBuffer(audio.begin() + start, audio.begin() + end);
    }

    return audio;
}

} // namespace sasayaku
