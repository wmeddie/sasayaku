import Cocoa
import AVFoundation
import Carbon.HIToolbox

class AppDelegate: NSObject, NSApplicationDelegate {
    private var hotkeyManager = HotkeyManager.shared

    func applicationDidFinishLaunching(_ notification: Notification) {
        requestMicrophonePermission()

        // Just check accessibility silently — don't prompt on every launch.
        // The user can grant it manually if they want auto-paste.
        if !AXIsProcessTrusted() {
            print("Accessibility not granted — auto-paste disabled. Enable in System Settings > Privacy & Security > Accessibility")
        }

        // Register global hotkey (Option+Space by default)
        hotkeyManager.register()
    }

    func applicationWillTerminate(_ notification: Notification) {
        hotkeyManager.unregister()
    }

    private func requestMicrophonePermission() {
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .audio) { granted in
                print(granted ? "Microphone permission granted" : "Microphone permission denied")
            }
        case .denied, .restricted:
            print("Microphone permission denied — enable in System Settings > Privacy & Security > Microphone")
        case .authorized:
            print("Microphone permission granted")
        @unknown default:
            break
        }
    }
}
