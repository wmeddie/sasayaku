import SwiftUI

struct ModelInfo {
    let filename: String
    let label: String
    let size: String
}

private let whisperModels: [ModelInfo] = [
    ModelInfo(filename: "ggml-tiny.bin",             label: "Tiny (fastest)",          size: "75 MB"),
    ModelInfo(filename: "ggml-base.bin",             label: "Base",                    size: "142 MB"),
    ModelInfo(filename: "ggml-small.bin",            label: "Small",                   size: "466 MB"),
    ModelInfo(filename: "ggml-medium.bin",           label: "Medium",                  size: "1.5 GB"),
    ModelInfo(filename: "ggml-large-v3-turbo.bin",   label: "Large v3 Turbo",          size: "1.6 GB"),
    ModelInfo(filename: "ggml-large-v3.bin",         label: "Large v3 (most accurate)", size: "3.1 GB"),
]

struct SettingsView: View {
    let engine: SasayakuEngine
    var onSaved: (() -> Void)? = nil
    @Binding var micGain: Double

    @State private var apiBaseURL = ""
    @State private var apiKey = ""
    @State private var apiModel = ""
    @State private var selectedModelFilename = "ggml-large-v3-turbo.bin"
    @State private var useGPU = true
    @State private var statusMessage = ""
    @State private var isDownloading = false
    @State private var downloadProgress: Double = 0
    @State private var downloadedModels: Set<String> = []

    private var modelsDir: String {
        if let home = ProcessInfo.processInfo.environment["HOME"] {
            return home + "/Library/Application Support/Sasayaku/models"
        }
        return ""
    }

    var body: some View {
        Form {
            Section("AI API") {
                TextField("Base URL", text: $apiBaseURL)
                    .textFieldStyle(.roundedBorder)

                SecureField("API Key", text: $apiKey)
                    .textFieldStyle(.roundedBorder)

                TextField("Model", text: $apiModel)
                    .textFieldStyle(.roundedBorder)
            }

            Section("Whisper Model") {
                // Model picker
                Picker("Model", selection: $selectedModelFilename) {
                    ForEach(whisperModels, id: \.filename) { model in
                        HStack {
                            Text(model.label)
                            Spacer()
                            if downloadedModels.contains(model.filename) {
                                Text(model.size)
                                    .foregroundStyle(.secondary)
                            } else {
                                Text("\(model.size) — not downloaded")
                                    .foregroundStyle(.orange)
                            }
                        }
                        .tag(model.filename)
                    }
                }

                // Status of selected model
                HStack {
                    if downloadedModels.contains(selectedModelFilename) {
                        Label("Downloaded", systemImage: "checkmark.circle.fill")
                            .foregroundStyle(.green)
                            .font(.caption)
                    } else {
                        Label("Not downloaded", systemImage: "arrow.down.circle")
                            .foregroundStyle(.orange)
                            .font(.caption)

                        Button("Download") {
                            downloadModel(selectedModelFilename)
                        }
                        .disabled(isDownloading)
                    }

                    Spacer()

                    if isDownloading {
                        ProgressView(value: downloadProgress)
                            .progressViewStyle(.linear)
                            .frame(width: 150)
                    }
                }

                Toggle("Use GPU (Metal)", isOn: $useGPU)
            }

            Section("Audio") {
                HStack {
                    Text("Mic Gain")
                    Slider(value: $micGain, in: 0.1...5.0, step: 0.1)
                    Text(String(format: "%.1fx", micGain))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                        .frame(width: 35, alignment: .trailing)
                }

                Text("Models from huggingface.co/ggerganov/whisper.cpp")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
            }

            Section {
                HStack {
                    Button("Save") {
                        saveSettings()
                    }
                    .keyboardShortcut(.defaultAction)

                    if !statusMessage.isEmpty {
                        Text(statusMessage)
                            .foregroundStyle(statusMessage.contains("Error") || statusMessage.contains("failed") ? .red : .green)
                            .font(.caption)
                    }
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 550, height: 420)
        .onAppear {
            loadSettings()
            scanDownloadedModels()
        }
    }

    private func scanDownloadedModels() {
        var found: Set<String> = []
        for model in whisperModels {
            let path = modelsDir + "/" + model.filename
            if FileManager.default.fileExists(atPath: path) {
                found.insert(model.filename)
            }
        }
        downloadedModels = found
    }

    private func loadSettings() {
        let settings = engine.currentSettings()
        apiBaseURL = settings.apiBaseURL
        apiKey = settings.apiKey
        apiModel = settings.apiModel
        useGPU = settings.useGPU

        // Determine selected model from the path
        let currentPath = settings.whisperModelPath
        if let model = whisperModels.first(where: { currentPath.hasSuffix($0.filename) }) {
            selectedModelFilename = model.filename
        }
    }

    private func saveSettings() {
        let settings = SasayakuSettings()
        settings.apiBaseURL = apiBaseURL
        settings.apiKey = apiKey
        settings.apiModel = apiModel
        settings.whisperModelPath = modelsDir + "/" + selectedModelFilename
        settings.useGPU = useGPU

        do {
            try engine.save(settings)
            do {
                try engine.reinitialize()
                statusMessage = "Settings saved"
                onSaved?()
            } catch {
                statusMessage = "Saved. Engine: \(error.localizedDescription)"
            }
        } catch {
            statusMessage = "Error: \(error.localizedDescription)"
        }

        DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
            statusMessage = ""
        }
    }

    private func downloadModel(_ filename: String) {
        let urlString = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/\(filename)"
        guard let url = URL(string: urlString) else {
            statusMessage = "Error: invalid URL"
            return
        }

        try? FileManager.default.createDirectory(
            atPath: modelsDir,
            withIntermediateDirectories: true
        )

        let destPath = modelsDir + "/" + filename
        let destURL = URL(fileURLWithPath: destPath)

        isDownloading = true
        downloadProgress = 0
        statusMessage = ""

        let delegate = DownloadDelegate { progress in
            DispatchQueue.main.async {
                self.downloadProgress = progress
            }
        }

        let session = URLSession(configuration: .default, delegate: delegate, delegateQueue: nil)
        let task = session.downloadTask(with: url) { tempURL, response, error in
            DispatchQueue.main.async {
                self.isDownloading = false

                if let error {
                    self.statusMessage = "Download failed: \(error.localizedDescription)"
                    return
                }

                guard let tempURL else {
                    self.statusMessage = "Download failed: no file"
                    return
                }

                do {
                    try? FileManager.default.removeItem(at: destURL)
                    try FileManager.default.moveItem(at: tempURL, to: destURL)
                    self.downloadedModels.insert(filename)
                    self.statusMessage = "Downloaded \(filename)"
                    // Auto-save if this is the selected model
                    if filename == self.selectedModelFilename {
                        self.saveSettings()
                    }
                } catch {
                    self.statusMessage = "Failed to save: \(error.localizedDescription)"
                }
            }
        }
        task.resume()
    }
}

private class DownloadDelegate: NSObject, URLSessionDownloadDelegate {
    let onProgress: (Double) -> Void

    init(onProgress: @escaping (Double) -> Void) {
        self.onProgress = onProgress
    }

    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask,
                    didWriteData bytesWritten: Int64, totalBytesWritten: Int64,
                    totalBytesExpectedToWrite: Int64) {
        if totalBytesExpectedToWrite > 0 {
            onProgress(Double(totalBytesWritten) / Double(totalBytesExpectedToWrite))
        }
    }

    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask,
                    didFinishDownloadingTo location: URL) {
    }
}
