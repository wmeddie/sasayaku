import SwiftUI

struct OverlayView: View {
    @ObservedObject var viewModel: RecordingViewModel

    var body: some View {
        VStack(spacing: 0) {
            // Main content area
            Group {
                switch viewModel.overlayState {
                case .recording:
                    WaveformView(levels: viewModel.waveformLevels, gain: viewModel.micGain)
                        .frame(height: 80)
                        .padding(.horizontal, 20)
                        .padding(.vertical, 16)

                case .processing:
                    WaveformView(levels: viewModel.waveformLevels, frozen: true, gain: viewModel.micGain)
                        .frame(height: 80)
                        .padding(.horizontal, 20)
                        .padding(.vertical, 16)
                        .overlay {
                            ProgressView()
                                .controlSize(.large)
                                .tint(.white)
                        }

                case .done:
                    TranscriptionEditor(text: $viewModel.editableText)
                        .padding(.horizontal, 16)
                        .padding(.vertical, 12)
                }
            }

            Divider()
                .background(Color.white.opacity(0.1))

            // Status bar
            HStack(spacing: 0) {
                // Status indicator
                HStack(spacing: 6) {
                    statusIcon
                    Text(statusLabel)
                        .font(.system(size: 13, weight: .medium))
                }

                Spacer()

                // Mode name + shortcut
                HStack(spacing: 4) {
                    Text(viewModel.currentModeName)
                        .font(.system(size: 12))
                        .foregroundStyle(.secondary)
                    KeyboardShortcutBadge("K")
                }

                Spacer()
                    .frame(width: 16)

                // Action + shortcut
                HStack(spacing: 4) {
                    Text(actionLabel)
                        .font(.system(size: 12))
                        .foregroundStyle(.secondary)
                    KeyboardShortcutBadge("Space")
                }

                Spacer()
                    .frame(width: 16)

                // Cancel/Close + shortcut
                HStack(spacing: 4) {
                    Text(viewModel.overlayState == .done ? "Close" : "Cancel")
                        .font(.system(size: 12))
                        .foregroundStyle(.secondary)
                    KeyboardShortcutBadge("Esc", plain: true)
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 8)
        }
        .frame(width: 580)
        .background(.ultraThinMaterial.opacity(0.9))
        .background(Color.black.opacity(0.7))
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .shadow(color: .black.opacity(0.4), radius: 20, y: 5)
    }

    @ViewBuilder
    private var statusIcon: some View {
        switch viewModel.overlayState {
        case .recording:
            Image(systemName: "record.circle.fill")
                .foregroundStyle(.red)
                .font(.system(size: 14))
        case .processing:
            Image(systemName: "gearshape.2.fill")
                .foregroundStyle(.blue)
                .font(.system(size: 14))
        case .done:
            Image(systemName: "checkmark.circle.fill")
                .foregroundStyle(.green)
                .font(.system(size: 14))
        }
    }

    private var statusLabel: String {
        switch viewModel.overlayState {
        case .recording: return "Recording"
        case .processing: return "Processing"
        case .done: return "Done"
        }
    }

    private var actionLabel: String {
        switch viewModel.overlayState {
        case .recording: return "Stop"
        case .processing: return "Record"
        case .done: return "Record"
        }
    }
}

// MARK: - Waveform visualization

struct WaveformView: View {
    let levels: [Float]
    var frozen: Bool = false
    var gain: Double = 1.0

    var body: some View {
        GeometryReader { geometry in
            let barCount = Int(geometry.size.width / 5)
            let displayLevels = resample(levels, to: barCount)

            HStack(spacing: 2) {
                ForEach(0..<displayLevels.count, id: \.self) { i in
                    let height = max(2, CGFloat(displayLevels[i]) * geometry.size.height)
                    RoundedRectangle(cornerRadius: 1.5)
                        .fill(frozen ? Color.white.opacity(0.3) : Color.white.opacity(0.8))
                        .frame(width: 2.5, height: height)
                        .animation(.linear(duration: 0.05), value: height)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    private func resample(_ input: [Float], to count: Int) -> [Float] {
        guard !input.isEmpty, count > 0 else {
            return Array(repeating: 0.03, count: max(count, 1))
        }

        var result = [Float]()

        for i in 0..<count {
            let start = i * input.count / count
            let end = min(start + max(1, input.count / count), input.count)
            if start < input.count {
                let slice = input[start..<end]
                let avg = slice.reduce(0, +) / Float(slice.count)
                // Logarithmic scaling with user-adjustable gain
                let g = Float(gain)
                let scaled = log10(1.0 + avg * 100.0 * g) / log10(1.0 + 100.0 * g)
                result.append(max(0.03, min(scaled, 1.0)))
            } else {
                result.append(0.03)
            }
        }

        return result
    }
}

// MARK: - Editable transcription text

struct TranscriptionEditor: View {
    @Binding var text: String
    @FocusState private var isFocused: Bool

    var body: some View {
        TextEditor(text: $text)
            .font(.system(size: 14))
            .scrollContentBackground(.hidden)
            .background(.clear)
            .foregroundStyle(.white)
            .focused($isFocused)
            .frame(minHeight: 60, maxHeight: 150)
            .onAppear {
                isFocused = true
            }
    }
}

// MARK: - Keyboard shortcut badge

struct KeyboardShortcutBadge: View {
    let label: String
    let plain: Bool

    init(_ label: String, plain: Bool = false) {
        self.label = label
        self.plain = plain
    }

    var body: some View {
        HStack(spacing: 2) {
            if !plain {
                Text("\u{2325}")  // Option symbol
                    .font(.system(size: 10))
            }
            Text(label)
                .font(.system(size: 10, weight: .medium))
        }
        .padding(.horizontal, 5)
        .padding(.vertical, 2)
        .background(Color.white.opacity(0.15))
        .clipShape(RoundedRectangle(cornerRadius: 3))
        .foregroundStyle(.secondary)
    }
}
