#import "SasayakuBridge.h"
#import <CoreAudio/CoreAudio.h>

#include "../../src/utils/config_manager.hpp"
#include "../../src/daemon/mode_manager.hpp"
#include "../../src/daemon/recording_coordinator.hpp"
#include "../../src/daemon/window_tracker.hpp"
#include <memory>
#include <iostream>

// MARK: - SasayakuModeInfo

@implementation SasayakuModeInfo
@end

// MARK: - SasayakuAudioDevice

@implementation SasayakuAudioDevice
@end

// MARK: - SasayakuSettings

@implementation SasayakuSettings
- (instancetype)init {
    self = [super init];
    if (self) {
        _apiBaseURL = @"";
        _apiKey = @"";
        _apiModel = @"";
        _whisperModelPath = @"";
        _useGPU = YES;
    }
    return self;
}
@end

// MARK: - SasayakuEngine

@interface SasayakuEngine () {
    std::unique_ptr<sasayaku::ConfigManager> _configManager;
    std::unique_ptr<sasayaku::ModeManager> _modeManager;
    std::unique_ptr<sasayaku::RecordingCoordinator> _coordinator;
    std::unique_ptr<sasayaku::WindowTracker> _windowTracker;
    BOOL _isEngineReady;
    BOOL _configLoaded;
}
@end

@implementation SasayakuEngine

- (instancetype)init {
    self = [super init];
    if (self) {
        _isEngineReady = NO;
        _configLoaded = NO;
    }
    return self;
}

- (void)loadConfig {
    if (_configLoaded) return;

    _configManager = std::make_unique<sasayaku::ConfigManager>();
    if (!_configManager->load()) {
        std::cout << "Creating default configuration..." << std::endl;
        _configManager->initialize_defaults();

        auto& config = _configManager->get_mutable_config();
        config.whisper.model_path = _configManager->get_data_dir() + "/models/ggml-large-v3-turbo.bin";

        // Ensure data directory exists
        NSString* dataDir = [NSString stringWithUTF8String:_configManager->get_data_dir().c_str()];
        NSString* modelsDir = [dataDir stringByAppendingPathComponent:@"models"];
        [[NSFileManager defaultManager] createDirectoryAtPath:modelsDir
                                  withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil];

        _configManager->save();
    }

    std::cout << "Configuration loaded from: " << _configManager->get_config_path() << std::endl;

    // Initialize mode manager (doesn't depend on whisper)
    _modeManager = std::make_unique<sasayaku::ModeManager>();
    _modeManager->initialize(_configManager.get());

    // Initialize window tracker
    _windowTracker = std::make_unique<sasayaku::WindowTracker>();
    _windowTracker->initialize();

    _configLoaded = YES;
}

- (BOOL)isEngineReady {
    return _isEngineReady;
}

- (BOOL)initializeWithError:(NSError * _Nullable *)error {
    // Ensure config is loaded first
    [self loadConfig];

    // Check whisper model
    const auto& whisperConfig = _configManager->get_config().whisper;
    if (whisperConfig.model_path.empty()) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.sasayaku"
                                         code:1
                                     userInfo:@{NSLocalizedDescriptionKey: @"Whisper model path not configured. Set it in Settings."}];
        }
        return NO;
    }

    // Check if model file exists
    NSString* modelPath = [NSString stringWithUTF8String:whisperConfig.model_path.c_str()];
    if (![[NSFileManager defaultManager] fileExistsAtPath:modelPath]) {
        if (error) {
            NSString* msg = [NSString stringWithFormat:@"Whisper model not found at: %@\nDownload a model and set the path in Settings.", modelPath];
            *error = [NSError errorWithDomain:@"com.sasayaku"
                                         code:1
                                     userInfo:@{NSLocalizedDescriptionKey: msg}];
        }
        return NO;
    }

    // Initialize recording coordinator
    _coordinator = std::make_unique<sasayaku::RecordingCoordinator>();
    if (!_coordinator->initialize(_configManager.get(), _modeManager.get())) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.sasayaku"
                                         code:2
                                     userInfo:@{NSLocalizedDescriptionKey: @"Failed to initialize transcription engine"}];
        }
        return NO;
    }

    _isEngineReady = YES;
    std::cout << "Sasayaku engine initialized successfully" << std::endl;
    return YES;
}

- (BOOL)startRecording {
    if (!_isEngineReady || !_coordinator) return NO;

    // Auto-switch mode based on active app
    std::string activeApp = _windowTracker->get_active_app_id();
    if (!activeApp.empty()) {
        std::string modeForApp = _modeManager->get_mode_for_app(activeApp);
        if (modeForApp != _modeManager->get_current_mode()) {
            std::cout << "Auto-switching to mode: " << modeForApp
                      << " for app: " << activeApp << std::endl;
            _modeManager->set_current_mode(modeForApp);
        }
    }

    return _coordinator->start_recording();
}

- (void)stopRecordingWithCompletion:(void (^)(NSString * _Nullable text, BOOL success))completion {
    if (!_isEngineReady || !_coordinator) {
        if (completion) completion(nil, NO);
        return;
    }

    _coordinator->stop_recording([completion](const std::string& text, bool success) {
        @autoreleasepool {
            NSString* nsText = success ? [NSString stringWithUTF8String:text.c_str()] : nil;
            if (completion) {
                completion(nsText, success);
            }
        }
    });
}

- (void)toggleRecordingWithCompletion:(void (^)(NSString * _Nullable text, BOOL success))completion {
    if (!_isEngineReady || !_coordinator) {
        if (completion) completion(nil, NO);
        return;
    }

    auto state = _coordinator->get_state();
    if (state == sasayaku::RecordingState::RECORDING) {
        [self stopRecordingWithCompletion:completion];
    } else if (state == sasayaku::RecordingState::STOPPED) {
        BOOL started = [self startRecording];
        if (completion) {
            completion(nil, started);
        }
    }
}

- (SasayakuRecordingState)recordingState {
    if (!_coordinator) return SasayakuRecordingStateStopped;

    switch (_coordinator->get_state()) {
        case sasayaku::RecordingState::STOPPED:    return SasayakuRecordingStateStopped;
        case sasayaku::RecordingState::RECORDING:   return SasayakuRecordingStateRecording;
        case sasayaku::RecordingState::PROCESSING:  return SasayakuRecordingStateProcessing;
        case sasayaku::RecordingState::ERROR:        return SasayakuRecordingStateError;
    }
    return SasayakuRecordingStateStopped;
}

- (float)audioLevel {
    if (!_coordinator) return 0.0f;
    return _coordinator->get_audio_level();
}

- (float)recordingDuration {
    if (!_coordinator) return 0.0f;
    return _coordinator->get_recording_duration();
}

- (NSString *)currentMode {
    if (!_modeManager) return @"voice_to_text";
    return [NSString stringWithUTF8String:_modeManager->get_current_mode().c_str()];
}

- (void)setCurrentMode:(NSString *)currentMode {
    if (_modeManager) {
        _modeManager->set_current_mode(std::string([currentMode UTF8String]));
    }
}

- (NSArray<SasayakuModeInfo *> *)availableModes {
    NSMutableArray* modes = [NSMutableArray array];
    if (!_configManager) return modes;

    const auto& configModes = _configManager->get_config().modes;
    for (const auto& [id, mode] : configModes) {
        SasayakuModeInfo* info = [[SasayakuModeInfo alloc] init];
        info.modeId = [NSString stringWithUTF8String:id.c_str()];
        info.name = [NSString stringWithUTF8String:mode.name.c_str()];
        info.modeDescription = [NSString stringWithUTF8String:mode.description.c_str()];
        info.useAI = mode.use_ai;
        info.prompt = [NSString stringWithUTF8String:mode.prompt.c_str()];
        info.userTemplate = [NSString stringWithUTF8String:mode.user_template.c_str()];
        info.requiresClipboard = mode.requires_clipboard;
        [modes addObject:info];
    }

    return modes;
}

- (void)saveMode:(SasayakuModeInfo *)mode {
    if (!_configManager) return;

    sasayaku::ModeConfig cfg;
    cfg.name = std::string([mode.name UTF8String]);
    cfg.description = std::string([mode.modeDescription UTF8String]);
    cfg.use_ai = mode.useAI;
    cfg.prompt = std::string([mode.prompt UTF8String]);
    cfg.user_template = std::string([mode.userTemplate UTF8String]);
    cfg.requires_clipboard = mode.requiresClipboard;

    _configManager->set_mode(std::string([mode.modeId UTF8String]), cfg);
    _configManager->save();

    // Reinitialize mode manager to pick up changes
    if (_modeManager) {
        _modeManager->initialize(_configManager.get());
    }
}

- (void)deleteMode:(NSString *)modeId {
    if (!_configManager) return;

    _configManager->delete_mode(std::string([modeId UTF8String]));
    _configManager->save();

    if (_modeManager) {
        _modeManager->initialize(_configManager.get());
    }
}

- (SasayakuSettings *)currentSettings {
    SasayakuSettings* settings = [[SasayakuSettings alloc] init];
    if (!_configManager) return settings;

    const auto& config = _configManager->get_config();
    settings.apiBaseURL = [NSString stringWithUTF8String:config.api.base_url.c_str()];
    settings.apiKey = [NSString stringWithUTF8String:config.api.api_key.c_str()];
    settings.apiModel = [NSString stringWithUTF8String:config.api.model.c_str()];
    settings.whisperModelPath = [NSString stringWithUTF8String:config.whisper.model_path.c_str()];
    settings.useGPU = config.whisper.use_gpu;

    return settings;
}

- (BOOL)saveSettings:(SasayakuSettings *)settings error:(NSError * _Nullable *)error {
    if (!_configManager) {
        [self loadConfig];
    }

    auto& config = _configManager->get_mutable_config();
    config.api.base_url = std::string([settings.apiBaseURL UTF8String]);
    config.api.api_key = std::string([settings.apiKey UTF8String]);
    config.api.model = std::string([settings.apiModel UTF8String]);
    config.whisper.model_path = std::string([settings.whisperModelPath UTF8String]);
    config.whisper.use_gpu = settings.useGPU;

    if (!_configManager->save()) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.sasayaku"
                                         code:3
                                     userInfo:@{NSLocalizedDescriptionKey: @"Failed to save settings"}];
        }
        return NO;
    }

    return YES;
}

- (BOOL)reinitializeWithError:(NSError * _Nullable *)error {
    if (!_configManager) return NO;

    // Reinitialize mode manager
    _modeManager->initialize(_configManager.get());

    // Try to initialize the recording engine
    _coordinator = std::make_unique<sasayaku::RecordingCoordinator>();
    if (!_coordinator->initialize(_configManager.get(), _modeManager.get())) {
        _isEngineReady = NO;
        if (error) {
            *error = [NSError errorWithDomain:@"com.sasayaku"
                                         code:4
                                     userInfo:@{NSLocalizedDescriptionKey: @"Failed to reinitialize engine"}];
        }
        return NO;
    }

    _isEngineReady = YES;
    return YES;
}

- (NSArray<SasayakuAudioDevice *> *)availableInputDevices {
    NSMutableArray* result = [NSMutableArray array];

    AudioObjectPropertyAddress devicesAddr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddr, 0, NULL, &dataSize);
    int count = dataSize / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> devices(count);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddr, 0, NULL, &dataSize, devices.data());

    for (AudioDeviceID dev : devices) {
        // Only include devices with input streams
        AudioObjectPropertyAddress streamsAddr = {
            kAudioDevicePropertyStreams,
            kAudioObjectPropertyScopeInput,
            kAudioObjectPropertyElementMain
        };
        UInt32 streamSize = 0;
        AudioObjectGetPropertyDataSize(dev, &streamsAddr, 0, NULL, &streamSize);
        if (streamSize == 0) continue;

        // Get name
        CFStringRef cfName = NULL;
        UInt32 nameSize = sizeof(cfName);
        AudioObjectPropertyAddress nameAddr = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        AudioObjectGetPropertyData(dev, &nameAddr, 0, NULL, &nameSize, &cfName);

        // Get transport type
        UInt32 transportType = 0;
        UInt32 ttSize = sizeof(transportType);
        AudioObjectPropertyAddress ttAddr = {
            kAudioDevicePropertyTransportType,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        AudioObjectGetPropertyData(dev, &ttAddr, 0, NULL, &ttSize, &transportType);

        SasayakuAudioDevice* device = [[SasayakuAudioDevice alloc] init];
        device.deviceId = dev;
        device.isBuiltIn = (transportType == kAudioDeviceTransportTypeBuiltIn);
        if (cfName) {
            device.name = (__bridge_transfer NSString*)cfName;
        } else {
            device.name = @"Unknown";
        }

        [result addObject:device];
    }

    return result;
}

- (NSString *)inputDeviceName {
    if (!_configManager) return @"";
    return [NSString stringWithUTF8String:_configManager->get_config().audio.device_name.c_str()];
}

- (void)setInputDeviceName:(NSString *)inputDeviceName {
    if (!_configManager) return;
    _configManager->get_mutable_config().audio.device_name = std::string([inputDeviceName UTF8String]);
    _configManager->save();
    std::cout << "Input device set to: " << [inputDeviceName UTF8String] << std::endl;
}

@end
