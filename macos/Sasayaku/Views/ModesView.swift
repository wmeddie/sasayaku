import SwiftUI

/// Editable copy of a mode for the UI
class EditableMode: ObservableObject, Identifiable {
    let id: String
    @Published var name: String
    @Published var description: String
    @Published var useAI: Bool
    @Published var prompt: String           // System message
    @Published var userTemplate: String     // User message template
    @Published var requiresClipboard: Bool

    init(from info: SasayakuModeInfo) {
        self.id = info.modeId
        self.name = info.name
        self.description = info.modeDescription
        self.useAI = info.useAI
        self.prompt = info.prompt ?? ""
        self.userTemplate = info.userTemplate ?? "{transcript}"
        self.requiresClipboard = info.requiresClipboard
    }

    init(id: String) {
        self.id = id
        self.name = ""
        self.description = ""
        self.useAI = true
        self.prompt = ""
        self.userTemplate = "{transcript}"
        self.requiresClipboard = false
    }
}

struct ModesView: View {
    let engine: SasayakuEngine
    var onModesChanged: (() -> Void)? = nil

    @State private var modes: [EditableMode] = []
    @State private var selectedModeId: String?
    @State private var showingNewModeSheet = false
    @State private var newModeId = ""
    @State private var statusMessage = ""

    var body: some View {
        HSplitView {
            // Mode list
            VStack(spacing: 0) {
                List(modes, selection: $selectedModeId) { mode in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(mode.name.isEmpty ? mode.id : mode.name)
                            .font(.system(size: 13, weight: .medium))
                        Text(mode.description)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                    .tag(mode.id)
                    .padding(.vertical, 2)
                }
                .listStyle(.sidebar)

                Divider()

                HStack {
                    Button(action: { showingNewModeSheet = true }) {
                        Image(systemName: "plus")
                    }

                    Button(action: deleteSelectedMode) {
                        Image(systemName: "minus")
                    }
                    .disabled(selectedModeId == nil || selectedModeId == "voice_to_text")

                    Spacer()
                }
                .padding(6)
            }
            .frame(minWidth: 160, maxWidth: 200)

            // Mode editor
            if let mode = modes.first(where: { $0.id == selectedModeId }) {
                ModeEditorView(mode: mode, onSave: {
                    saveMode(mode)
                })
            } else {
                VStack {
                    Text("Select a mode to edit")
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .frame(minWidth: 600, minHeight: 350)
        .onAppear {
            loadModes()
        }
        .sheet(isPresented: $showingNewModeSheet) {
            NewModeSheet(modeId: $newModeId) {
                createMode()
            }
        }
        .overlay(alignment: .bottom) {
            if !statusMessage.isEmpty {
                Text(statusMessage)
                    .font(.caption)
                    .foregroundStyle(.green)
                    .padding(6)
                    .background(.ultraThinMaterial)
                    .clipShape(RoundedRectangle(cornerRadius: 4))
                    .padding(8)
            }
        }
    }

    private func loadModes() {
        let infos = engine.availableModes()
        modes = infos.map { EditableMode(from: $0) }
        if selectedModeId == nil, let first = modes.first {
            selectedModeId = first.id
        }
    }

    private func saveMode(_ mode: EditableMode) {
        let info = SasayakuModeInfo()
        info.modeId = mode.id
        info.name = mode.name
        info.modeDescription = mode.description
        info.useAI = mode.useAI
        info.prompt = mode.prompt
        info.userTemplate = mode.userTemplate
        info.requiresClipboard = mode.requiresClipboard
        engine.saveMode(info)
        onModesChanged?()

        statusMessage = "Saved \(mode.name)"
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
            statusMessage = ""
        }
    }

    private func deleteSelectedMode() {
        guard let id = selectedModeId, id != "voice_to_text" else { return }
        engine.deleteMode(id)
        modes.removeAll { $0.id == id }
        selectedModeId = modes.first?.id
        onModesChanged?()
    }

    private func createMode() {
        let trimmed = newModeId
            .lowercased()
            .replacingOccurrences(of: " ", with: "_")
            .filter { $0.isLetter || $0.isNumber || $0 == "_" }

        guard !trimmed.isEmpty else { return }
        guard !modes.contains(where: { $0.id == trimmed }) else { return }

        let mode = EditableMode(id: trimmed)
        mode.name = newModeId  // Use original as display name
        mode.description = "Custom mode"
        mode.useAI = true
        mode.prompt = "You are a helpful assistant. Process the user's transcribed speech."
        mode.userTemplate = "{transcript}"
        modes.append(mode)
        selectedModeId = trimmed
        saveMode(mode)
        newModeId = ""
        showingNewModeSheet = false
    }
}

struct ModeEditorView: View {
    @ObservedObject var mode: EditableMode
    var onSave: () -> Void

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text(mode.id)
                    .font(.caption)
                    .foregroundStyle(.tertiary)
                    .padding(.top, 4)

                LabeledContent("Name") {
                    TextField("", text: $mode.name)
                        .textFieldStyle(.roundedBorder)
                }

                LabeledContent("Description") {
                    TextField("", text: $mode.description)
                        .textFieldStyle(.roundedBorder)
                }

                Toggle("Use AI processing", isOn: $mode.useAI)

                if mode.useAI {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("System Prompt")
                            .font(.headline)

                        Text("Instructions for how the AI should behave. Sets the role and context.")
                            .font(.caption)
                            .foregroundStyle(.secondary)

                        TextEditor(text: $mode.prompt)
                            .font(.system(.body, design: .monospaced))
                            .frame(minHeight: 80)
                            .border(Color(nsColor: .separatorColor))
                    }

                    VStack(alignment: .leading, spacing: 4) {
                        Text("User Message")
                            .font(.headline)

                        Text("Template for the user message. Use {transcript} for speech, {clipboard} for clipboard.")
                            .font(.caption)
                            .foregroundStyle(.secondary)

                        TextEditor(text: $mode.userTemplate)
                            .font(.system(.body, design: .monospaced))
                            .frame(minHeight: 40)
                            .border(Color(nsColor: .separatorColor))
                    }

                    Toggle("Include clipboard content ({clipboard})", isOn: $mode.requiresClipboard)
                }

                HStack {
                    Spacer()
                    Button("Save") {
                        onSave()
                    }
                    .keyboardShortcut(.defaultAction)
                }
            }
            .padding()
        }
    }
}

struct NewModeSheet: View {
    @Binding var modeId: String
    var onCreate: () -> Void
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 16) {
            Text("New Mode")
                .font(.headline)

            TextField("Mode name (e.g. Business Japanese)", text: $modeId)
                .textFieldStyle(.roundedBorder)
                .frame(width: 300)

            HStack {
                Button("Cancel") { dismiss() }
                    .keyboardShortcut(.cancelAction)
                Button("Create") { onCreate() }
                    .keyboardShortcut(.defaultAction)
                    .disabled(modeId.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .padding()
    }
}
