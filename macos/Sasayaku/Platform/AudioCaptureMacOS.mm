#include "../../src/core/audio_capture.hpp"

#ifdef __APPLE__

#import <AVFoundation/AVFoundation.h>
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <vector>
#include <cmath>

namespace sasayaku {

struct AudioCapture::PlatformImpl {
    AVAudioEngine* engine = nil;
    bool is_initialized = false;
};

AudioCapture::AudioCapture()
    : platform_(std::make_unique<PlatformImpl>()) {
}

AudioCapture::~AudioCapture() {
    cleanup_platform();
}

void AudioCapture::cleanup_platform() {
    if (is_recording_) {
        stop_recording();
    }

    if (platform_->engine) {
        [platform_->engine stop];
        [[platform_->engine inputNode] removeTapOnBus:0];
        platform_->engine = nil;
    }

    platform_->is_initialized = false;
}

bool AudioCapture::initialize(const AudioCaptureConfig& config) {
    config_ = config;
    return true;
}

bool AudioCapture::initialize_platform() {
    if (platform_->is_initialized) {
        return true;
    }

    @autoreleasepool {
        platform_->engine = [[AVAudioEngine alloc] init];
        if (!platform_->engine) {
            last_error_ = "Failed to create AVAudioEngine";
            return false;
        }

        platform_->is_initialized = true;
        std::cout << "AVAudioEngine initialized successfully" << std::endl;
    }

    return true;
}

bool AudioCapture::start_recording(AudioDataCallback callback) {
    if (is_recording_) {
        return true;
    }

    // Always create a fresh engine so the device format is never stale
    cleanup_platform();
    platform_ = std::make_unique<PlatformImpl>();
    platform_->engine = [[AVAudioEngine alloc] init];
    platform_->is_initialized = true;

    data_callback_ = callback;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.clear();
    }

    // Set flags BEFORE starting the engine
    should_stop_ = false;
    is_recording_ = true;

    @autoreleasepool {
        // Find the correct input device FIRST, before accessing inputNode
        AudioDeviceID chosenDevice = 0;
        {
            // Get all audio devices
            AudioObjectPropertyAddress devicesAddr = {
                kAudioHardwarePropertyDevices,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
            UInt32 dataSize = 0;
            AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddr, 0, NULL, &dataSize);
            int deviceCount = dataSize / sizeof(AudioDeviceID);
            std::vector<AudioDeviceID> devices(deviceCount);
            AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddr, 0, NULL, &dataSize, devices.data());

            AudioDeviceID chosenDevice = 0;
            AudioDeviceID builtinDevice = 0;
            AudioDeviceID defaultDevice = 0;

            // Get system default
            {
                UInt32 sz = sizeof(defaultDevice);
                AudioObjectPropertyAddress addr = {
                    kAudioHardwarePropertyDefaultInputDevice,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &sz, &defaultDevice);
            }

            // Find devices with input channels
            std::cout << "Available input devices:" << std::endl;
            for (AudioDeviceID dev : devices) {
                // Check if device has input channels
                AudioObjectPropertyAddress streamsAddr = {
                    kAudioDevicePropertyStreams,
                    kAudioObjectPropertyScopeInput,
                    kAudioObjectPropertyElementMain
                };
                UInt32 streamSize = 0;
                AudioObjectGetPropertyDataSize(dev, &streamsAddr, 0, NULL, &streamSize);
                if (streamSize == 0) continue;  // No input streams

                // Get device name
                CFStringRef cfName = NULL;
                UInt32 nameSize = sizeof(cfName);
                AudioObjectPropertyAddress nameAddr = {
                    kAudioObjectPropertyName,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                AudioObjectGetPropertyData(dev, &nameAddr, 0, NULL, &nameSize, &cfName);
                char name[256] = "";
                if (cfName) {
                    CFStringGetCString(cfName, name, sizeof(name), kCFStringEncodingUTF8);
                    CFRelease(cfName);
                }

                // Check if it's a built-in device
                UInt32 transportType = 0;
                UInt32 ttSize = sizeof(transportType);
                AudioObjectPropertyAddress ttAddr = {
                    kAudioDevicePropertyTransportType,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                AudioObjectGetPropertyData(dev, &ttAddr, 0, NULL, &ttSize, &transportType);

                bool isBuiltin = (transportType == kAudioDeviceTransportTypeBuiltIn);
                bool isDefault = (dev == defaultDevice);
                std::cout << "  " << (isDefault ? "* " : "  ") << name
                          << " (id=" << dev << (isBuiltin ? ", built-in" : "") << ")" << std::endl;

                if (isBuiltin && builtinDevice == 0) {
                    builtinDevice = dev;
                }

                // If config has a device_name, match it
                if (!config_.device_name.empty() &&
                    std::string(name).find(config_.device_name) != std::string::npos) {
                    chosenDevice = dev;
                }
            }

            // Priority: configured device > built-in > default
            if (chosenDevice == 0) {
                chosenDevice = builtinDevice ? builtinDevice : defaultDevice;
            }

            // Log chosen device
            if (chosenDevice != 0) {
                CFStringRef cfName = NULL;
                UInt32 nameSize = sizeof(cfName);
                AudioObjectPropertyAddress nameAddr = {
                    kAudioObjectPropertyName,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                AudioObjectGetPropertyData(chosenDevice, &nameAddr, 0, NULL, &nameSize, &cfName);
                if (cfName) {
                    char nameBuf[256];
                    CFStringGetCString(cfName, nameBuf, sizeof(nameBuf), kCFStringEncodingUTF8);
                    std::cout << "Using input: " << nameBuf << std::endl;
                    CFRelease(cfName);
                }
            }
        }

        // Set the input device on the engine's audio unit BEFORE accessing inputNode's format
        if (chosenDevice != 0) {
            AVAudioInputNode* tempInput = [platform_->engine inputNode];
            AudioUnit inputAU = [tempInput audioUnit];
            if (inputAU) {
                AudioUnitSetProperty(inputAU,
                    kAudioOutputUnitProperty_CurrentDevice,
                    kAudioUnitScope_Global, 0,
                    &chosenDevice, sizeof(chosenDevice));
            }
        }

        // Now access inputNode — format will reflect the chosen device
        AVAudioInputNode* inputNode = [platform_->engine inputNode];

        // Get the hardware's native format
        AVAudioFormat* hwFormat = [inputNode outputFormatForBus:0];
        double hwRate = [hwFormat sampleRate];
        uint32_t hwChannels = (uint32_t)[hwFormat channelCount];
        std::cout << "Hardware format: " << hwRate << " Hz, "
                  << hwChannels << " ch" << std::endl;

        // Prepare the engine
        [platform_->engine prepare];

        // Capture what we need for the block
        AudioCapture* capturePtr = this;
        int targetRate = config_.sample_rate;

        // Install tap — pass nil for format to let AVAudioEngine choose
        __block int tapCallCount = 0;
        [inputNode installTapOnBus:0
                        bufferSize:4096
                            format:nil
                             block:^(AVAudioPCMBuffer* _Nonnull buffer, AVAudioTime* _Nonnull when) {

            AVAudioFrameCount rawCount = buffer.frameLength;
            uint32_t bufChannels = (uint32_t)buffer.format.channelCount;
            double bufRate = buffer.format.sampleRate;

            if (!buffer.floatChannelData || rawCount == 0) return;
            const float* rawSamples = buffer.floatChannelData[0];

            tapCallCount++;
            if (tapCallCount == 1) {
                std::cout << "Audio tap active: " << bufRate << " Hz, "
                          << bufChannels << " ch" << std::endl;
            }

            // If multi-channel, mix down to mono
            std::vector<float> mono;
            if (bufChannels > 1) {
                mono.resize(rawCount);
                for (AVAudioFrameCount i = 0; i < rawCount; i++) {
                    float sum = 0;
                    for (uint32_t ch = 0; ch < bufChannels; ch++) {
                        sum += buffer.floatChannelData[ch][i];
                    }
                    mono[i] = sum / bufChannels;
                }
                rawSamples = mono.data();
            }

            // Resample from buffer rate to targetRate using linear interpolation
            double ratio = bufRate / (double)targetRate;
            size_t outputCount = (size_t)(rawCount / ratio);
            if (outputCount == 0) return;

            std::vector<float> resampled(outputCount);
            for (size_t i = 0; i < outputCount; i++) {
                double srcIdx = i * ratio;
                size_t idx0 = (size_t)srcIdx;
                size_t idx1 = std::min(idx0 + 1, (size_t)(rawCount - 1));
                double frac = srcIdx - idx0;
                resampled[i] = (float)(rawSamples[idx0] * (1.0 - frac) + rawSamples[idx1] * frac);
            }

            capturePtr->on_process(resampled.data(), resampled.size());
        }];

        NSError* error = nil;
        if (![platform_->engine startAndReturnError:&error]) {
            last_error_ = "Failed to start AVAudioEngine: ";
            if (error) {
                last_error_ += [[error localizedDescription] UTF8String];
            }
            [inputNode removeTapOnBus:0];
            is_recording_ = false;
            return false;
        }
    }

    std::cout << "Recording started (resampling " << config_.sample_rate << " Hz)" << std::endl;
    return true;
}

bool AudioCapture::stop_recording() {
    if (!is_recording_) {
        return true;
    }

    should_stop_ = true;
    is_recording_ = false;

    @autoreleasepool {
        if (platform_->engine) {
            [[platform_->engine inputNode] removeTapOnBus:0];
            [platform_->engine stop];
        }
    }

    // Diagnostic: report buffer size
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        float duration = static_cast<float>(buffer_.size()) / config_.sample_rate;
        std::cout << "Recording stopped — captured "
                  << buffer_.size() << " samples ("
                  << duration << "s)" << std::endl;
    }

    return true;
}

AudioBuffer AudioCapture::get_buffer() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return buffer_;
}

void AudioCapture::clear_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_.clear();
}

void AudioCapture::on_process(const float* samples, size_t count) {
    if (!is_recording_ || should_stop_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), samples, samples + count);
    }

    if (data_callback_) {
        data_callback_(samples, count);
    }
}

} // namespace sasayaku

#endif // __APPLE__
