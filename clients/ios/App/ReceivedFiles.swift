import SwiftUI

struct ReceivedFiles: View {
    @Environment(\.mobileTheme) private var theme
    let folder: URL
    @ObservedObject var client: IOSClient
    @State private var files: [URL] = []
    @State private var search = ""
    @State private var selected = Set<URL>()
    @State private var editMode = EditMode.inactive
    @State private var exporting: FileExportSelection?
    @State private var previewing: FileExportSelection?
    @State private var sharing: FileExportSelection?
    @State private var renamePrompt = false
    @State private var deletePrompt = false
    @State private var renaming: URL?
    @State private var renameText = ""
    @State private var deleting: [URL] = []
    @State private var message = ""
    @State private var failure: String?
    private var store: ReceivedStore { ReceivedStore(root: client.received) }
    private var visible: [URL] { files.filter { search.isEmpty || $0.lastPathComponent.localizedStandardContains(search) } }
    private var selectedFiles: [URL] { files.filter { selected.contains($0) } }
    var body: some View {
        MobileList(selection: $selected) {
            if editMode.isEditing {
                Section {
                    HStack {
                        Button("Select all") { selected = Set(visible) }.disabled(client.active)
                        Spacer(); Text("\(selected.count) selected").font(.caption)
                    }
                    Menu("Selected file actions") {
                        Button("Share selected", systemImage: "square.and.arrow.up") { open(selectedFiles, action: "share") }
                        Button("Save to Files", systemImage: "folder") { open(selectedFiles, action: "export") }
                        Button("Delete selected", systemImage: "trash", role: .destructive) { deleting = selectedFiles; deletePrompt = true }
                    }.disabled(selected.isEmpty || client.active)
                }
            }
            if !message.isEmpty { Text(message).font(.footnote) }
            if visible.isEmpty { Text(search.isEmpty ? "No received files in this folder." : "No matching files.").foregroundStyle(theme?.muted ?? Color.secondary) }
            ForEach(visible, id: \.self) { file in
                HStack {
                    if editMode.isEditing { fileLabel(file) }
                    else if isFolder(file) {
                        NavigationLink { ReceivedFiles(folder: file, client: client) } label: { fileLabel(file) }
                    } else { Button { open([file], action: "preview") } label: { fileLabel(file) }.buttonStyle(.plain).disabled(client.active) }
                    Spacer()
                    if !editMode.isEditing {
                        Menu {
                            if !isFolder(file) { Button("Preview", systemImage: "eye") { open([file], action: "preview") } }
                            Button("Save to Files", systemImage: "folder") { open([file], action: "export") }
                            Button("Share", systemImage: "square.and.arrow.up") { open([file], action: "share") }
                            Button("Rename", systemImage: "pencil") { renameText = file.lastPathComponent; renaming = file; renamePrompt = true }
                            Button("Delete", systemImage: "trash", role: .destructive) { deleting = [file]; deletePrompt = true }
                        } label: { Image(systemName: "ellipsis.circle") }
                        .accessibilityLabel("Actions for " + file.lastPathComponent).disabled(client.active)
                    }
                }.tag(file)
            }
        }
        .navigationTitle(folder.lastPathComponent)
        .searchable(text: $search, placement: .navigationBarDrawer(displayMode: .always), prompt: "Search this folder")
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                Button(editMode.isEditing ? "Done" : "Select") { editMode = editMode.isEditing ? .inactive : .active }
                    .disabled(client.active || files.isEmpty)
            }

        }
        .onAppear(perform: reload).refreshable { reload() }
        .onChange(of: client.history) { _ in reload() }
        .onChange(of: editMode) { mode in if !mode.isEditing { selected.removeAll() } }
        .sheet(item: $exporting, onDismiss: { client.endFileBrowser() }) { selection in
            FileProviderExport(urls: selection.urls) { copied in
                if copied { message = "Copy handed to Files. Your provider manages cloud syncing." }
                exporting = nil
            }
        }
        .sheet(item: $previewing, onDismiss: { client.endFileBrowser() }) { selection in
            NavigationStack {
                FilePreview(urls: selection.urls)
                    .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { previewing = nil } } }
            }
        }
        .sheet(item: $sharing, onDismiss: { client.endFileBrowser() }) { FileSharing(urls: $0.urls) }
        .alert("Rename", isPresented: $renamePrompt) {
            TextField("Filename, including extension", text: $renameText)
            Button("Rename") {
                guard let file = renaming else { return }
                manage { _ = try store.rename(file, to: renameText) }; renaming = nil
            }
            Button("Cancel", role: .cancel) { renaming = nil }
        } message: { Text("Existing files are never overwritten.") }
        .confirmationDialog("Delete \(deleting.count) items?", isPresented: $deletePrompt, titleVisibility: .visible) {
            Button("Delete permanently", role: .destructive) {
                let targets = deleting; manage { try store.delete(targets) }; deleting = []
            }
            Button("Cancel", role: .cancel) { deleting = [] }
        } message: { Text("This removes the selected files and any selected folders with their contents. This cannot be undone.") }
        .alert("Files", isPresented: Binding(get: { failure != nil }, set: { if !$0 { failure = nil } })) { Button("OK") { failure = nil } } message: { Text(failure ?? "") }
        .environment(\.editMode, $editMode)
    }
    private func isFolder(_ file: URL) -> Bool { (try? file.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true }
    private func fileLabel(_ file: URL) -> some View { Label(file.lastPathComponent, systemImage: isFolder(file) ? "folder" : "doc") }
    private func open(_ urls: [URL], action: String) {
        guard !urls.isEmpty, client.beginFileBrowser() else { return }
        let selection = FileExportSelection(urls: urls)
        switch action {
        case "preview": previewing = selection
        case "export": exporting = selection
        default: sharing = selection
        }
    }
    private func manage(_ operation: () throws -> Void) {
        guard client.beginFileBrowser() else { return }; defer { client.endFileBrowser(); reload() }
        do { try operation(); selected.removeAll() } catch { failure = error.localizedDescription }
    }
    private func reload() {
        do { files = try store.contents(of: folder); selected.formIntersection(Set(files)) }
        catch { failure = error.localizedDescription }
    }
}
