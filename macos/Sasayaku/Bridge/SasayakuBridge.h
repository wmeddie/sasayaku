#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Represents a dictation mode (full config)
@interface SasayakuModeInfo : NSObject
@property (nonatomic, copy) NSString *modeId;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *modeDescription;
@property (nonatomic, assign) BOOL useAI;
@property (nonatomic, copy) NSString *prompt;          // System message
@property (nonatomic, copy) NSString *userTemplate;    // User message template (default: {transcript})
@property (nonatomic, assign) BOOL requiresClipboard;
@end

/// Represents an audio input device
@interface SasayakuAudioDevice : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) uint32_t deviceId;
@property (nonatomic, assign) BOOL isBuiltIn;
@end

/// Settings data passed between Swift and the C++ core
@interface SasayakuSettings : NSObject
@property (nonatomic, copy) NSString *apiBaseURL;
@property (nonatomic, copy) NSString *apiKey;
@property (nonatomic, copy) NSString *apiModel;
@property (nonatomic, copy) NSString *whisperModelPath;
@property (nonatomic, assign) BOOL useGPU;
@end

/// Recording state enum exposed to Swift
typedef NS_ENUM(NSInteger, SasayakuRecordingState) {
    SasayakuRecordingStateStopped = 0,
    SasayakuRecordingStateRecording,
    SasayakuRecordingStateProcessing,
    SasayakuRecordingStateError
};

/// Main bridge between Swift/SwiftUI and the C++ core engine.
/// All methods are safe to call from any thread unless noted otherwise.
@interface SasayakuEngine : NSObject

/// Load configuration (always succeeds). Call this first.
- (void)loadConfig;

/// Initialize the transcription engine. Returns NO if whisper model can't load.
/// Config and modes are still available even if this fails.
- (BOOL)initializeWithError:(NSError * _Nullable *)error;

/// Whether the transcription engine is ready to record
@property (nonatomic, readonly) BOOL isEngineReady;

/// Start recording audio
- (BOOL)startRecording;

/// Stop recording and transcribe. Completion called on background thread.
- (void)stopRecordingWithCompletion:(void (^)(NSString * _Nullable text, BOOL success))completion;

/// Toggle recording on/off. Completion called when transcription finishes (if stopping).
- (void)toggleRecordingWithCompletion:(void (^)(NSString * _Nullable text, BOOL success))completion;

/// Current recording state
@property (nonatomic, readonly) SasayakuRecordingState recordingState;

/// Current audio level (0.0 - 1.0), updated during recording
@property (nonatomic, readonly) float audioLevel;

/// Recording duration in seconds
@property (nonatomic, readonly) float recordingDuration;

/// Current mode ID
@property (nonatomic, copy) NSString *currentMode;

/// Available modes
- (NSArray<SasayakuModeInfo *> *)availableModes;

/// Current settings
- (SasayakuSettings *)currentSettings;

/// Save settings. Returns NO on failure.
- (BOOL)saveSettings:(SasayakuSettings *)settings error:(NSError * _Nullable *)error;

/// Reinitialize after settings change
- (BOOL)reinitializeWithError:(NSError * _Nullable *)error;

/// Get available audio input devices
- (NSArray<SasayakuAudioDevice *> *)availableInputDevices;

/// Get the configured input device name (empty = auto/built-in)
@property (nonatomic, copy) NSString *inputDeviceName;

/// Save a mode (creates or updates). Saves config to disk.
- (void)saveMode:(SasayakuModeInfo *)mode;

/// Delete a mode by ID. Saves config to disk.
- (void)deleteMode:(NSString *)modeId;

@end

NS_ASSUME_NONNULL_END
