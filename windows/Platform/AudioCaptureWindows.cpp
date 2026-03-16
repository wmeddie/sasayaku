#ifdef _WIN32

#include "../../src/core/audio_capture.hpp"
#include "WinUtils.hpp"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>

#pragma comment(lib, "ole32.lib")

namespace sasayaku {

struct AudioCapture::PlatformImpl {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    HANDLE stopEvent = nullptr;
    std::thread captureThread;
    WAVEFORMATEX* mixFormat = nullptr;
    bool comInitialized = false;
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

    if (platform_->captureClient) {
        platform_->captureClient->Release();
        platform_->captureClient = nullptr;
    }
    if (platform_->audioClient) {
        platform_->audioClient->Release();
        platform_->audioClient = nullptr;
    }
    if (platform_->device) {
        platform_->device->Release();
        platform_->device = nullptr;
    }
    if (platform_->enumerator) {
        platform_->enumerator->Release();
        platform_->enumerator = nullptr;
    }
    if (platform_->mixFormat) {
        CoTaskMemFree(platform_->mixFormat);
        platform_->mixFormat = nullptr;
    }
    if (platform_->stopEvent) {
        CloseHandle(platform_->stopEvent);
        platform_->stopEvent = nullptr;
    }
    if (platform_->comInitialized) {
        CoUninitialize();
        platform_->comInitialized = false;
    }
}

bool AudioCapture::initialize(const AudioCaptureConfig& config) {
    config_ = config;
    return true;
}

bool AudioCapture::initialize_platform() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE) {
        platform_->comInitialized = true;
    }

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&platform_->enumerator
    );
    if (FAILED(hr)) {
        last_error_ = "Failed to create device enumerator";
        return false;
    }

    // If device_name is specified, find it by name
    if (!config_.device_name.empty()) {
        IMMDeviceCollection* collection = nullptr;
        hr = platform_->enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr) && collection) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; i++) {
                IMMDevice* dev = nullptr;
                collection->Item(i, &dev);
                if (!dev) continue;

                IPropertyStore* props = nullptr;
                dev->OpenPropertyStore(STGM_READ, &props);
                if (props) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    props->GetValue(PKEY_Device_FriendlyName, &name);
                    if (name.vt == VT_LPWSTR) {
                        std::string devName = wide_to_utf8(name.pwszVal);
                        if (devName.find(config_.device_name) != std::string::npos) {
                            platform_->device = dev;
                            dev = nullptr;  // Don't release, we're keeping it
                            std::cout << "Using input: " << devName << std::endl;
                        }
                    }
                    PropVariantClear(&name);
                    props->Release();
                }
                if (dev) dev->Release();
                if (platform_->device) break;
            }
            collection->Release();
        }
    }

    // Fall back to default input device
    if (!platform_->device) {
        hr = platform_->enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &platform_->device);
        if (FAILED(hr)) {
            last_error_ = "No audio input device found";
            return false;
        }
        std::cout << "Using default input device" << std::endl;
    }

    return true;
}

bool AudioCapture::start_recording(AudioDataCallback callback) {
    if (is_recording_) return true;

    cleanup_platform();
    platform_ = std::make_unique<PlatformImpl>();

    if (!initialize_platform()) return false;

    data_callback_ = callback;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.clear();
    }

    should_stop_ = false;
    is_recording_ = true;

    // Activate audio client
    HRESULT hr = platform_->device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        (void**)&platform_->audioClient
    );
    if (FAILED(hr)) {
        last_error_ = "Failed to activate audio client";
        is_recording_ = false;
        return false;
    }

    // Get mix format
    hr = platform_->audioClient->GetMixFormat(&platform_->mixFormat);
    if (FAILED(hr)) {
        last_error_ = "Failed to get mix format";
        is_recording_ = false;
        return false;
    }

    WAVEFORMATEX* fmt = platform_->mixFormat;
    std::cout << "Hardware format: " << fmt->nSamplesPerSec << " Hz, "
              << fmt->nChannels << " ch, " << fmt->wBitsPerSample << " bit" << std::endl;

    // Initialize audio client in shared mode
    REFERENCE_TIME bufferDuration = 200000; // 20ms in 100ns units
    hr = platform_->audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0, bufferDuration, 0,
        platform_->mixFormat, nullptr
    );
    if (FAILED(hr)) {
        last_error_ = "Failed to initialize audio client";
        is_recording_ = false;
        return false;
    }

    hr = platform_->audioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&platform_->captureClient
    );
    if (FAILED(hr)) {
        last_error_ = "Failed to get capture client";
        is_recording_ = false;
        return false;
    }

    platform_->stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Start capture
    hr = platform_->audioClient->Start();
    if (FAILED(hr)) {
        last_error_ = "Failed to start audio capture";
        is_recording_ = false;
        return false;
    }

    // Capture thread
    int targetRate = config_.sample_rate;
    int hwRate = fmt->nSamplesPerSec;
    int hwChannels = fmt->nChannels;
    int hwBits = fmt->wBitsPerSample;
    bool isFloat = (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
        (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    platform_->captureThread = std::thread([this, targetRate, hwRate, hwChannels, hwBits, isFloat]() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        while (WaitForSingleObject(platform_->stopEvent, 10) == WAIT_TIMEOUT) {
            UINT32 packetLength = 0;
            platform_->captureClient->GetNextPacketSize(&packetLength);

            while (packetLength > 0) {
                BYTE* data = nullptr;
                UINT32 framesAvailable = 0;
                DWORD flags = 0;

                HRESULT hr = platform_->captureClient->GetBuffer(
                    &data, &framesAvailable, &flags, nullptr, nullptr
                );
                if (FAILED(hr)) break;

                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && framesAvailable > 0) {
                    // Convert to mono float
                    std::vector<float> mono(framesAvailable);

                    if (isFloat && hwBits == 32) {
                        const float* fdata = reinterpret_cast<const float*>(data);
                        for (UINT32 i = 0; i < framesAvailable; i++) {
                            float sum = 0;
                            for (int ch = 0; ch < hwChannels; ch++) {
                                sum += fdata[i * hwChannels + ch];
                            }
                            mono[i] = sum / hwChannels;
                        }
                    } else if (hwBits == 16) {
                        const int16_t* idata = reinterpret_cast<const int16_t*>(data);
                        for (UINT32 i = 0; i < framesAvailable; i++) {
                            float sum = 0;
                            for (int ch = 0; ch < hwChannels; ch++) {
                                sum += idata[i * hwChannels + ch] / 32768.0f;
                            }
                            mono[i] = sum / hwChannels;
                        }
                    }

                    // Resample to target rate
                    double ratio = (double)hwRate / targetRate;
                    size_t outputCount = (size_t)(framesAvailable / ratio);
                    if (outputCount > 0) {
                        std::vector<float> resampled(outputCount);
                        for (size_t i = 0; i < outputCount; i++) {
                            double srcIdx = i * ratio;
                            size_t idx0 = (size_t)srcIdx;
                            size_t idx1 = std::min(idx0 + 1, (size_t)(framesAvailable - 1));
                            double frac = srcIdx - idx0;
                            resampled[i] = (float)(mono[idx0] * (1.0 - frac) + mono[idx1] * frac);
                        }
                        this->on_process(resampled.data(), resampled.size());
                    }
                }

                platform_->captureClient->ReleaseBuffer(framesAvailable);
                platform_->captureClient->GetNextPacketSize(&packetLength);
            }
        }

        CoUninitialize();
    });

    std::cout << "Recording started (WASAPI, resampling to " << targetRate << " Hz)" << std::endl;
    return true;
}

bool AudioCapture::stop_recording() {
    if (!is_recording_) return true;

    should_stop_ = true;
    is_recording_ = false;

    if (platform_->stopEvent) {
        SetEvent(platform_->stopEvent);
    }
    if (platform_->captureThread.joinable()) {
        platform_->captureThread.join();
    }
    if (platform_->audioClient) {
        platform_->audioClient->Stop();
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        float duration = static_cast<float>(buffer_.size()) / config_.sample_rate;
        std::cout << "Recording stopped -- captured "
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
    if (!is_recording_ || should_stop_) return;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.insert(buffer_.end(), samples, samples + count);
    }

    if (data_callback_) {
        data_callback_(samples, count);
    }
}

} // namespace sasayaku

#endif // _WIN32
