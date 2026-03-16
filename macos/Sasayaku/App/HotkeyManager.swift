import Carbon.HIToolbox
import Cocoa

/// Manages a global keyboard shortcut for toggling recording.
/// Uses the Carbon RegisterEventHotKey API (works without accessibility permission).
final class HotkeyManager {
    static let shared = HotkeyManager()

    private var hotkeyRef: EventHotKeyRef?
    private var eventHandler: EventHandlerRef?

    // Callback invoked when hotkey is pressed
    var onHotkeyPressed: (() -> Void)?

    private init() {}

    /// Register the global hotkey (Option+Space)
    func register() {
        // Define hotkey ID
        var hotkeyID = EventHotKeyID()
        hotkeyID.signature = OSType(0x5361_7361) // "Sasa"
        hotkeyID.id = 1

        // Set up event type spec
        var eventType = EventTypeSpec(
            eventClass: OSType(kEventClassKeyboard),
            eventKind: UInt32(kEventHotKeyPressed)
        )

        // Install handler
        let status = InstallEventHandler(
            GetApplicationEventTarget(),
            { (_, event, _) -> OSStatus in
                HotkeyManager.shared.onHotkeyPressed?()
                return noErr
            },
            1,
            &eventType,
            nil,
            &eventHandler
        )

        guard status == noErr else {
            print("Failed to install hotkey handler: \(status)")
            return
        }

        // Register Option+Space
        // kVK_Space = 0x31, optionKey modifier
        let registerStatus = RegisterEventHotKey(
            UInt32(kVK_Space),
            UInt32(optionKey),
            hotkeyID,
            GetApplicationEventTarget(),
            0,
            &hotkeyRef
        )

        if registerStatus == noErr {
            print("Global hotkey registered: Option+Space")
        } else {
            print("Failed to register hotkey: \(registerStatus)")
        }
    }

    /// Unregister the global hotkey
    func unregister() {
        if let ref = hotkeyRef {
            UnregisterEventHotKey(ref)
            hotkeyRef = nil
        }
        if let handler = eventHandler {
            RemoveEventHandler(handler)
            eventHandler = nil
        }
    }
}
