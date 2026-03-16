import SwiftUI

struct RecordingView: View {
    @ObservedObject var viewModel: RecordingViewModel

    var body: some View {
        VStack(spacing: 16) {
            // Mode picker
            HStack {
                Text("Mode:")
                    .foregroundStyle(.secondary)
                Picker("", selection: Binding(
                    get: { viewModel.engine.currentMode ?? "voice_to_text" },
                    set: { viewModel.setMode($0) }
                )) {
                    ForEach(viewModel.modes, id: \.modeId) { mode in
                        Text(mode.name).tag(mode.modeId)
                    }
                }
                .labelsHidden()
                .frame(width: 200)

                Spacer()

                if viewModel.isRecording {
                    Text(formatDuration(viewModel.recordingDuration))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
            }

            // Status
            HStack {
                Circle()
                    .fill(statusColor)
                    .frame(width: 8, height: 8)
                Text(viewModel.statusText)
                    .foregroundStyle(.secondary)
                Spacer()
            }

            // Audio level bar
            if viewModel.isRecording {
                AudioLevelView(level: viewModel.audioLevel)
                    .frame(height: 8)
                    .animation(.linear(duration: 0.1), value: viewModel.audioLevel)
            }

            // Transcription text
            if !viewModel.lastTranscription.isEmpty {
                ScrollView {
                    Text(viewModel.lastTranscription)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(8)
                }
                .background(Color(nsColor: .textBackgroundColor))
                .clipShape(RoundedRectangle(cornerRadius: 6))
            }

            Spacer()

            // Record button
            HStack {
                Button(action: { viewModel.toggleRecording() }) {
                    HStack {
                        Image(systemName: viewModel.isRecording ? "stop.fill" : "mic.fill")
                        Text(viewModel.isRecording ? "Stop Recording" : "Start Recording")
                    }
                    .frame(maxWidth: .infinity)
                }
                .controlSize(.large)
                .keyboardShortcut(.return, modifiers: [])
                .disabled(viewModel.isProcessing)

                if viewModel.isProcessing {
                    ProgressView()
                        .controlSize(.small)
                }
            }
        }
        .padding()
    }

    private var statusColor: Color {
        if viewModel.isRecording {
            return .red
        } else if viewModel.isProcessing {
            return .orange
        } else {
            return .green
        }
    }

    private func formatDuration(_ seconds: Float) -> String {
        let mins = Int(seconds) / 60
        let secs = Int(seconds) % 60
        return String(format: "%d:%02d", mins, secs)
    }
}

struct AudioLevelView: View {
    let level: Float

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color(nsColor: .separatorColor))

                RoundedRectangle(cornerRadius: 4)
                    .fill(levelColor)
                    .frame(width: max(0, geometry.size.width * CGFloat(min(level * 5, 1.0))))
            }
        }
    }

    private var levelColor: Color {
        let normalized = min(level * 5, 1.0)
        if normalized > 0.8 {
            return .red
        } else if normalized > 0.5 {
            return .yellow
        } else {
            return .green
        }
    }
}
