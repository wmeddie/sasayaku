import AppKit
import SwiftUI

/// A floating panel window that stays on top and has no title bar.
/// Used for the recording/processing/done overlay.
class OverlayPanel: NSPanel {
    private weak var viewModel: RecordingViewModel?

    init(viewModel: RecordingViewModel) {
        self.viewModel = nil  // Set after super.init
        super.init(
            contentRect: NSRect(x: 0, y: 0, width: 580, height: 140),
            styleMask: [.nonactivatingPanel, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )

        self.viewModel = viewModel

        isFloatingPanel = true
        level = .floating
        isOpaque = false
        backgroundColor = .clear
        hasShadow = true
        titleVisibility = .hidden
        titlebarAppearsTransparent = true
        isMovableByWindowBackground = true
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        animationBehavior = .utilityWindow

        // SwiftUI content
        let overlayView = OverlayView(viewModel: viewModel)
            .environment(\.colorScheme, .dark)
        contentView = NSHostingView(rootView: overlayView)

        // Position at bottom center of main screen
        positionAtBottomCenter()
    }

    func positionAtBottomCenter() {
        guard let screen = NSScreen.main else { return }
        let screenFrame = screen.visibleFrame
        let x = screenFrame.midX - frame.width / 2
        let y = screenFrame.minY + 80
        setFrameOrigin(NSPoint(x: x, y: y))
    }

    // Allow the panel to become key so the text editor can receive input
    override var canBecomeKey: Bool { true }

    // But don't force-activate the app when showing
    override var canBecomeMain: Bool { false }

    override func keyDown(with event: NSEvent) {
        let optionPressed = event.modifierFlags.contains(.option)

        // Option+Space — toggle recording / confirm
        if event.keyCode == 49 && optionPressed {
            Task { @MainActor in
                self.viewModel?.toggleRecording()
            }
            return
        }

        // Esc — cancel/close
        if event.keyCode == 53 {
            Task { @MainActor in
                self.viewModel?.cancelRecording()
            }
            return
        }

        // Option+K — cycle mode
        if event.keyCode == 40 && optionPressed {
            Task { @MainActor in
                self.viewModel?.cycleMode()
            }
            return
        }

        super.keyDown(with: event)
    }

    // Also handle via performKeyEquivalent for when panel isn't key
    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        let optionPressed = event.modifierFlags.contains(.option)

        if event.keyCode == 49 && optionPressed {
            Task { @MainActor in
                self.viewModel?.toggleRecording()
            }
            return true
        }

        if event.keyCode == 53 {
            Task { @MainActor in
                self.viewModel?.cancelRecording()
            }
            return true
        }

        return super.performKeyEquivalent(with: event)
    }
}

/// Manages showing/hiding the overlay panel
@MainActor
class OverlayPanelController {
    static let shared = OverlayPanelController()

    private var panel: OverlayPanel?
    private weak var viewModel: RecordingViewModel?

    func setup(viewModel: RecordingViewModel) {
        self.viewModel = viewModel
    }

    func show() {
        guard let viewModel else { return }

        if panel == nil {
            panel = OverlayPanel(viewModel: viewModel)
        }

        panel?.positionAtBottomCenter()
        panel?.orderFrontRegardless()

        // If in done state, make key so text editor works
        if viewModel.overlayState == .done {
            panel?.makeKey()
        }
    }

    func showAndMakeKey() {
        show()
        panel?.makeKey()
    }

    func hide() {
        panel?.orderOut(nil)
    }

    var isVisible: Bool {
        panel?.isVisible ?? false
    }
}
