import SwiftUI
import Combine

enum OverlayState {
    case recording
    case processing
    case done
}

@MainActor
final class RecordingViewModel: ObservableObject {
    @Published var isRecording = false
    @Published var isProcessing = false
    @Published var audioLevel: Float = 0
    @Published var recordingDuration: Float = 0
    @Published var statusText = "Ready"
    @Published var lastTranscription = ""
    @Published var currentModeName = "Voice to Text"
    @Published var modes: [SasayakuModeInfo] = []
    @Published var engineError: String? = nil
    @Published var inputDevices: [SasayakuAudioDevice] = []
    @Published var selectedInputDeviceName: String = ""
    @Published var micGain: Double = UserDefaults.standard.double(forKey: "micGain") == 0
        ? 1.0 : UserDefaults.standard.double(forKey: "micGain") {
        didSet { UserDefaults.standard.set(micGain, forKey: "micGain") }
    }

    // Overlay state
    @Published var overlayState: OverlayState = .recording
    @Published var waveformLevels: [Float] = []
    @Published var editableText = ""

    let engine = SasayakuEngine()

    private var audioLevelTimer: Timer?
    private var isEngineReady = false
    private let maxWaveformSamples = 20  // ~1 second at 50ms interval

    init() {
        // Always load config first — this never fails
        engine.loadConfig()
        loadModes()
        loadInputDevices()

        // Try to init the transcription engine (may fail if model missing)
        initializeEngine()

        // Connect global hotkey
        HotkeyManager.shared.onHotkeyPressed = { [weak self] in
            Task { @MainActor in
                self?.toggleRecording()
            }
        }

        // Set up overlay panel
        OverlayPanelController.shared.setup(viewModel: self)
    }

    private func initializeEngine() {
        do {
            try engine.initialize()
            isEngineReady = true
            engineError = nil
            statusText = "Ready"
        } catch {
            isEngineReady = false
            engineError = error.localizedDescription
            statusText = "Model not loaded — open Settings to configure"
            print("Engine init: \(error.localizedDescription)")
        }
    }

    func loadModes() {
        modes = engine.availableModes()
        if let current = modes.first(where: { $0.modeId == engine.currentMode }) {
            currentModeName = current.name
        }
    }

    func setMode(_ modeId: String) {
        engine.currentMode = modeId
        if let mode = modes.first(where: { $0.modeId == modeId }) {
            currentModeName = mode.name
        }
    }

    func loadInputDevices() {
        inputDevices = engine.availableInputDevices()
        selectedInputDeviceName = engine.inputDeviceName ?? ""
    }

    func setInputDevice(_ name: String) {
        engine.inputDeviceName = name
        selectedInputDeviceName = name
    }

    func cycleMode() {
        guard !modes.isEmpty else { return }
        let currentId = engine.currentMode ?? "voice_to_text"
        let currentIndex = modes.firstIndex(where: { $0.modeId == currentId }) ?? 0
        let nextIndex = (currentIndex + 1) % modes.count
        setMode(modes[nextIndex].modeId)
    }

    func toggleRecording() {
        if !isEngineReady {
            initializeEngine()
            guard isEngineReady else { return }
        }

        if isRecording {
            stopRecording()
        } else if overlayState == .done && OverlayPanelController.shared.isVisible {
            // Confirm edited text and copy to clipboard
            confirmAndCopy()
        } else {
            startRecording()
        }
    }

    func startRecording() {
        guard isEngineReady, !isRecording else { return }

        if engine.startRecording() {
            isRecording = true
            isProcessing = false
            statusText = "Recording... Speak now"
            lastTranscription = ""
            editableText = ""
            waveformLevels = []
            overlayState = .recording

            startAudioLevelUpdates()
            OverlayPanelController.shared.show()
        } else {
            statusText = "Failed to start recording"
        }
    }

    func stopRecording() {
        guard isRecording else { return }

        isRecording = false
        isProcessing = true
        statusText = "Processing transcription..."
        overlayState = .processing
        stopAudioLevelUpdates()

        engine.stopRecording { [weak self] text, success in
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.isProcessing = false

                if success, let text, !text.isEmpty {
                    self.lastTranscription = text
                    self.editableText = text
                    self.statusText = "Transcription complete — Copied to clipboard"
                    self.overlayState = .done
                    // Make panel key so text editor is focused
                    OverlayPanelController.shared.showAndMakeKey()
                } else {
                    self.statusText = "Transcription failed"
                    self.dismissOverlay()
                }
            }
        }
    }

    func confirmAndCopy() {
        // Copy the (possibly edited) text to clipboard
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(editableText, forType: .string)
        lastTranscription = editableText
        statusText = "Copied to clipboard"
        dismissOverlay()
    }

    func cancelRecording() {
        if isRecording {
            // Stop recording without transcribing
            engine.stopRecording { _, _ in }
            isRecording = false
            stopAudioLevelUpdates()
        }
        isProcessing = false
        statusText = "Cancelled"
        dismissOverlay()
    }

    func dismissOverlay() {
        OverlayPanelController.shared.hide()
        overlayState = .recording
        waveformLevels = []
    }

    /// Called after settings are saved and engine reinitialized successfully
    func retryEngineInit() {
        isEngineReady = engine.isEngineReady
        if isEngineReady {
            engineError = nil
            statusText = "Ready"
        }
        loadModes()
    }

    private func startAudioLevelUpdates() {
        audioLevelTimer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in
                guard let self, self.isRecording else { return }
                let level = self.engine.audioLevel
                self.audioLevel = level
                self.recordingDuration = self.engine.recordingDuration

                // Append to waveform
                self.waveformLevels.append(level)
                if self.waveformLevels.count > self.maxWaveformSamples {
                    self.waveformLevels.removeFirst()
                }
            }
        }
    }

    private func stopAudioLevelUpdates() {
        audioLevelTimer?.invalidate()
        audioLevelTimer = nil
        audioLevel = 0
    }
}
