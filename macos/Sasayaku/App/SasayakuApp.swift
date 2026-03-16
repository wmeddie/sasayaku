import SwiftUI

@main
struct SasayakuApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var recordingVM = RecordingViewModel()

    var body: some Scene {
        MenuBarExtra {
            MenuBarView(viewModel: recordingVM)
        } label: {
            Image(systemName: recordingVM.isRecording ? "mic.fill" : "mic")
                .symbolRenderingMode(.hierarchical)
                .foregroundStyle(recordingVM.isRecording ? .red : .primary)
        }

        Window("Sasayaku", id: "recording") {
            RecordingView(viewModel: recordingVM)
                .frame(minWidth: 400, minHeight: 300)
        }
        .defaultSize(width: 500, height: 400)

        Window("Sasayaku Settings", id: "settings") {
            TabView {
                SettingsView(engine: recordingVM.engine, onSaved: { [weak recordingVM] in
                    recordingVM?.retryEngineInit()
                }, micGain: $recordingVM.micGain)
                .tabItem { Label("General", systemImage: "gear") }

                ModesView(engine: recordingVM.engine, onModesChanged: { [weak recordingVM] in
                    recordingVM?.loadModes()
                })
                .tabItem { Label("Modes", systemImage: "text.bubble") }
            }
            .frame(minWidth: 600, minHeight: 420)
        }
        .windowResizability(.contentSize)
    }
}

struct MenuBarView: View {
    @ObservedObject var viewModel: RecordingViewModel
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        // Status
        if let error = viewModel.engineError {
            Label("Model not loaded", systemImage: "exclamationmark.triangle")
            Text(error)
                .font(.caption)
                .lineLimit(2)
            Divider()
        }

        Button(viewModel.isRecording ? "Stop Recording" : "Start Recording") {
            viewModel.toggleRecording()
        }
        .disabled(viewModel.engineError != nil)
        .keyboardShortcut("r", modifiers: [.command, .shift])

        Divider()

        Text("Mode: \(viewModel.currentModeName)")

        if !viewModel.modes.isEmpty {
            Menu("Switch Mode") {
                ForEach(viewModel.modes, id: \.modeId) { mode in
                    Button(mode.name) {
                        viewModel.setMode(mode.modeId)
                    }
                }
            }
        }

        Menu("Input Device") {
            // "Auto" option — uses built-in mic
            Button {
                viewModel.setInputDevice("")
            } label: {
                HStack {
                    Text("Auto (Built-in)")
                    if viewModel.selectedInputDeviceName.isEmpty {
                        Text("  \u{2713}")
                    }
                }
            }

            Divider()

            ForEach(viewModel.inputDevices, id: \.deviceId) { device in
                Button {
                    viewModel.setInputDevice(device.name)
                } label: {
                    HStack {
                        Text(device.name)
                        if viewModel.selectedInputDeviceName == device.name {
                            Text("  \u{2713}")
                        }
                    }
                }
            }
        }

        Divider()

        if !viewModel.lastTranscription.isEmpty {
            Text(viewModel.lastTranscription.count > 30
                 ? String(viewModel.lastTranscription.prefix(30)) + "..."
                 : viewModel.lastTranscription)
            Button("Copy to Clipboard") {
                NSPasteboard.general.clearContents()
                NSPasteboard.general.setString(viewModel.lastTranscription, forType: .string)
            }
            Divider()
        }

        Button("Settings...") {
            NSApp.activate(ignoringOtherApps: true)
            openWindow(id: "settings")
        }
        .keyboardShortcut(",", modifiers: .command)

        Divider()

        Button("Quit") {
            NSApplication.shared.terminate(nil)
        }
        .keyboardShortcut("q")
    }
}
