import SwiftUI
import UniformTypeIdentifiers

@main struct BitSkimMobileApp: App {
    @StateObject private var client = IOSClient()
    @AppStorage("appearanceTheme") private var themeID = "system"
    @Environment(\.scenePhase) private var phase
    var body: some Scene {
        WindowGroup {
            MobileHome(client: client)
                .environment(\.mobileTheme, MobileTheme.resolve(themeID))
                .tint(MobileTheme.resolve(themeID)?.accent ?? .accentColor)
                .preferredColorScheme(MobileTheme.resolve(themeID).map { $0.dark ? .dark : .light })
                .onAppear { client.start() }
                .onChange(of: phase) { state in
                    if state == .active { client.start(); client.refreshSharedFiles() }
                    else if state == .background { client.stop() }
                }
        }
    }
}

struct MobileHome: View {
    @Environment(\.mobileTheme) private var theme
    @ObservedObject var client: IOSClient
    @State private var address = ""
    @State private var selectedTab = "clients"
    @AppStorage("appearanceTheme") private var themeID = "system"
    var body: some View {
        TabView(selection: $selectedTab) {
            NavigationStack {
                MobileList {
                    Section {
                        Label("BitSkim 0.2 · Experimental", systemImage: "arrow.left.arrow.right")
                        Text("Keep BitSkim open to receive. Incoming files are saved in Files → On My iPhone/iPad → BitSkim → Received.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                    if !client.sharedFiles.isEmpty {
                        Section("Ready to send · \(client.sharedFiles.count) files") {
                            ForEach(client.sharedFiles, id: \.self) { Text($0.lastPathComponent) }
                            Text("Choose a computer below to send these files.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                        }
                    }
                    Section("Nearby clients") {
                        ForEach(PeerCatalog.computers(client.peers)) { computer in
                            if computer.endpoints.count == 1, let peer = computer.endpoints.first {
                                Button { client.prepare(peer: peer) } label: { ComputerRow(computer: computer) }
                                    .buttonStyle(.plain).disabled(client.active || client.importing)
                            } else {
                                Menu {
                                    ForEach(computer.endpoints) { peer in
                                        Button(ConnectionTarget.endpoint(peer.address, port: peer.port)) { client.prepare(peer: peer) }
                                    }
                                } label: { ComputerRow(computer: computer) }
                                    .disabled(client.active || client.importing)
                            }
                        }
                        if client.peers.isEmpty {
                            Text(client.discovery ? "No computers found. Check Local Network permission, Wi-Fi and the other computer’s firewall." : "This build uses manual connections. Enter the computer’s IPv4 address below.")
                                .font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                        }
                        if client.discovery { Button("Refresh clients") { client.refresh() } }
                    }
                    Section("Receive on this device") {
                        ForEach(client.addresses, id: \.self) { ip in Text(verbatim: ConnectionTarget.endpoint(ip, port: client.listeningPort)).monospaced().textSelection(.enabled) }
                        if client.addresses.isEmpty { Text("Connect to Wi-Fi to see a local receiving address.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary) }
                        Text("Enter this address on the sending computer. BitSkim must stay open.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                    Section("Connect by IP") {
                        TextField("192.168.1.20 or 192.168.1.20:46440", text: $address)
                            .keyboardType(.numbersAndPunctuation).textInputAutocapitalization(.never).autocorrectionDisabled()
                        Button(client.sharedFiles.isEmpty ? "Choose from Files" : "Send shared files") { client.prepare(address: address) }
                            .disabled(client.active || client.importing || address.isEmpty)
                        Text("Files can come from iCloud Drive or any cloud location enabled in Files.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                        Text("Use the receiving computer’s LAN address. It must be running BitSkim and allow incoming connections.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                    Section("Transfer") {
                        Text(client.status)
                        if client.importing { ProgressView("Preparing files…") }
                        if client.active {
                            ProgressView(value: Double(client.transferred), total: Double(max(client.total, 1)))
                            Text("\(client.transferred) / \(client.total) bytes").font(.caption).monospacedDigit()
                            Button("Cancel transfer", role: .destructive) { client.cancel() }
                        }
                        Button("Retry queued transfer") { client.retry() }.disabled(client.active)
                    }
                }
                .navigationTitle("BitSkim")
            }.tabItem { Label("Clients", systemImage: "network") }.tag("clients")
            NavigationStack {
                MobileList {
                    Section("Browse files to send") {
                        Button("Browse Files", systemImage: "folder") { client.browseFiles() }
                            .disabled(client.active || client.importing)
                        Text("Browse local files, iCloud Drive and enabled Dropbox or OneDrive locations. Select files here, then choose a client to send them.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                        if client.importing { ProgressView("Preparing files…") }
                    }
                    Section("Ready to send") {
                        ForEach(client.sharedFiles, id: \.self) { file in Label(file.lastPathComponent, systemImage: "doc") }
                        if client.sharedFiles.isEmpty { Text("No files selected.").foregroundStyle(theme?.muted ?? Color.secondary) }
                        else { Button("Choose a client") { selectedTab = "clients" } }
                    }
                    Section {
                        Text("Only files you select are copied into BitSkim. Nothing is sent until you choose a client. Cloud locations appear only when their apps are installed and enabled in Files.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                }.navigationTitle("Files")
            }.tabItem { Label("Files", systemImage: "folder") }.tag("files")
            NavigationStack { ReceivedFiles(folder: client.received, client: client) }.tabItem { Label("Received", systemImage: "tray.and.arrow.down") }.tag("received")
            NavigationStack {
                MobileList {
                    Section("Appearance") {
                        NavigationLink(destination: MobileThemeSettings()) {
                            HStack { Text("Theme"); Spacer(); Text(MobileTheme.resolve(themeID)?.name ?? "Follow system").foregroundStyle(theme?.muted ?? Color.secondary) }
                        }
                        Text("Applies to all BitSkim screens and is remembered on this device.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                    Section("Files and cloud storage") {
                        DisclosureGroup("iCloud Drive, Dropbox and OneDrive") {
                            Text("Install and sign in to your cloud app. In Files, open Browse → … → Edit and enable the provider. Pick a computer in BitSkim, then choose files from that location.")
                            Text("For received files or folders, open Received → … → Save to Files. BitSkim keeps the original; the provider handles cloud syncing.")
                            Link("Files setup guide", destination: URL(string: "https://support.apple.com/en-ie/102238")!)
                        }
                    }
                    Section("Encryption") {
                        Picker("Mode", selection: Binding(get: { client.security }, set: { client.setSecurity($0) })) {
                            Text("Disabled").tag(0); Text("Prefer encryption").tag(1); Text("Require encryption").tag(2)
                        }.disabled(client.active)
                        Text("Prefer can use plaintext with older clients. Require rejects plaintext. Compare the full code on both computers before trusting a device.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                    Section {
                        NavigationLink("Diagnostics", destination: MobileDiagnostics(client: client))
                    }
                    Section {
                        Link("Website", destination: URL(string: "https://bitskim.retrocs.org")!)
                        Link("Source code", destination: URL(string: "https://github.com/retrocs-org/bitskim")!)
                        Text("Experimental iPhone/iPad client. No always-on background receiving.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                }.navigationTitle("Settings")
            }.tabItem { Label("Settings", systemImage: "gearshape") }.tag("settings")
        }
        .sheet(isPresented: $client.filePicker) {
            FileProviderImport(directory: client.received) { result in
                client.filePicker = false; client.imported(result)
            }
        }
        .onChange(of: client.filePicker) { shown in if !shown { client.endImportBrowser() } }
        .sheet(item: $client.offer) { incoming in IncomingOffer(client: client, incoming: incoming).interactiveDismissDisabled() }
        .alert("BitSkim", isPresented: Binding(get: { client.error != nil }, set: { if !$0 { client.error = nil } })) {
            Button("OK") { client.error = nil }
        } message: { Text(client.error ?? "") }
    }
}
struct MobileThemeSettings: View {
    @AppStorage("appearanceTheme") private var themeID = "system"
    var body: some View {
        MobileList {
            themeButton(id: "system", name: "Follow system", color: .primary)
            ForEach(MobileTheme.all) { theme in
                themeButton(id: theme.id, name: theme.name, color: theme.accent)
            }
        }.navigationTitle("Theme")
    }
    private func themeButton(id: String, name: String, color: Color) -> some View {
        Button {
            var transaction = Transaction(); transaction.disablesAnimations = true
            withTransaction(transaction) { themeID = id }
        } label: {
            HStack {
                Circle().fill(color).frame(width: 20, height: 20).accessibilityHidden(true)
                Text(name); Spacer()
                if (MobileTheme.resolve(themeID) == nil ? "system" : themeID) == id {
                    Image(systemName: "checkmark").accessibilityHidden(true)
                }
            }
        }.buttonStyle(.plain)
        .accessibilityAddTraits((MobileTheme.resolve(themeID) == nil ? "system" : themeID) == id ? .isSelected : [])
    }
}
struct MobileDiagnostics: View {
    @Environment(\.mobileTheme) private var theme
    @ObservedObject var client: IOSClient
    var body: some View {
        MobileList {
            Section("Device pairing") { PairingControls(client: client) }
            Section("Recent transfers") {
                ForEach(client.history) { entry in
                    VStack(alignment: .leading, spacing: 5) {
                        Text(entry.name).font(.headline)
                        Text(entry.message)
                        Text(entry.integrity).font(.caption).foregroundStyle(theme?.muted ?? Color.secondary)
                    }
                }
                if client.history.isEmpty { Text("No transfers in this session.") }
            }
            Section("Connection report") {
                Text(client.diagnostics).font(.system(.caption, design: .monospaced)).textSelection(.enabled)
            }
        }.navigationTitle("Diagnostics")
    }
}
struct ComputerRow: View {
    @Environment(\.mobileTheme) private var theme
    let computer: MobileComputer
    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "desktopcomputer").foregroundStyle(.tint)
            VStack(alignment: .leading, spacing: 4) {
                Text(computer.name).foregroundStyle(theme?.text ?? Color.primary)
                Text(computer.platform).font(.caption).foregroundStyle(theme?.muted ?? Color.secondary)
                if computer.endpoints.count == 1, let peer = computer.endpoints.first {
                    Text(verbatim: ConnectionTarget.endpoint(peer.address, port: peer.port)).font(.caption).foregroundStyle(theme?.muted ?? Color.secondary)
                } else { Text("\(computer.endpoints.count) network addresses · choose one").font(.caption).foregroundStyle(theme?.muted ?? Color.secondary) }
            }
        }
    }
}
struct PairingControls: View {
    @Environment(\.mobileTheme) private var theme
    @ObservedObject var client: IOSClient
    var body: some View {
        if client.code.isEmpty { Text("Pair during an encrypted transfer. Both screens must show the same full code.").font(.footnote) }
        else {
            Text(client.code).font(.system(.title3, design: .monospaced)).textSelection(.enabled)
            if client.trusted { Label("Saved device identity matches", systemImage: "checkmark.shield") }
            else {
                TextField("Code from the other computer", text: $client.otherCode).textInputAutocapitalization(.characters).autocorrectionDisabled()
                Button("Trust this device") { client.trust() }.disabled(client.otherCode.isEmpty)
            }
        }
    }
}
struct IncomingOffer: View {
    @Environment(\.mobileTheme) private var theme
    @ObservedObject var client: IOSClient
    let incoming: MobileOffer
    var body: some View {
        NavigationStack {
            MobileList {
                Section {
                    Text("\(incoming.peer) wants to send \(incoming.count > 0 ? "a collection" : incoming.name)")
                    Text("Computer names are not verified. Existing files are preserved with safe renamed destinations.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                }
                Section("Device identity") { PairingControls(client: client) }
                if incoming.count > 0 {
                    Section("Files and folders · \(incoming.count) items") {
                        ForEach(0..<incoming.count, id: \.self) { index in Text(client.manifestName(index, serial: incoming.id)).font(.caption) }
                    }
                } else { Section("File") { Text(incoming.name); Text("\(incoming.size) bytes") } }
                Section {
                    Text("Save into BitSkim’s Received folder, available in Files.")
                    Text(client.spaceReport(incoming)).font(.caption).foregroundStyle(theme?.muted ?? Color.secondary)
                    if let error = client.error { Text(error).foregroundStyle(.red) }
                    Button("Accept and receive") { client.accept(incoming) }
                    Button("Decline", role: .destructive) { client.decline(incoming) }
                }
            }.navigationTitle("Incoming transfer")
        }
    }
}
struct ReceivedFiles: View {
    @Environment(\.mobileTheme) private var theme
    let folder: URL
    @ObservedObject var client: IOSClient
    @State private var files: [URL] = []
    @State private var exporting: FileExportSelection?
    @State private var exportStatus = ""
    var body: some View {
        MobileList {
            Section {
                Text("Save a copy to iCloud Drive, Dropbox, OneDrive or another enabled Files location. Originals stay in BitSkim.").font(.footnote).foregroundStyle(theme?.muted ?? Color.secondary)
                if !exportStatus.isEmpty { Text(exportStatus).font(.footnote) }
            }
            if files.isEmpty { Text("No received files in this folder.").foregroundStyle(theme?.muted ?? Color.secondary) }
            ForEach(files, id: \.self) { file in
                HStack {
                    if (try? file.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true {
                        NavigationLink { ReceivedFiles(folder: file, client: client) } label: { Label(file.lastPathComponent, systemImage: "folder") }
                    } else { ShareLink(item: file) { Label(file.lastPathComponent, systemImage: "doc") }.disabled(client.active) }
                    Menu {
                        Button("Save to Files", systemImage: "folder") {
                            guard client.beginFileBrowser() else { return }
                            exporting = FileExportSelection(urls: [file])
                        }
                        ShareLink(item: file) { Label("Share", systemImage: "square.and.arrow.up") }
                    } label: { Image(systemName: "ellipsis.circle") }
                    .accessibilityLabel("Actions for " + file.lastPathComponent)
                    .disabled(client.active)
                }
            }
        }.navigationTitle(folder.lastPathComponent).onAppear(perform: reload).refreshable { reload() }
        .onChange(of: client.history) { _ in reload() }
        .sheet(item: $exporting, onDismiss: { client.endFileBrowser() }) { selection in
            FileProviderExport(urls: selection.urls) { copied in
                if copied { exportStatus = "Copy handed to Files. Your provider manages cloud syncing." }
                exporting = nil
            }
        }
    }
    private func reload() {
        files = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.isDirectoryKey], options: [.skipsHiddenFiles])) ?? []).sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
    }
}
