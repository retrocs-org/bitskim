import SwiftUI
import UniformTypeIdentifiers

@main struct BitSkimMobileApp: App {
    @StateObject private var client = IOSClient()
    @Environment(\.scenePhase) private var phase
    var body: some Scene {
        WindowGroup {
            MobileHome(client: client)
                .tint(.purple)
                .onAppear { client.start() }
                .onChange(of: phase) { state in
                    if state == .active { client.start(); client.refreshSharedFiles() }
                    else if state == .background { client.stop() }
                }
        }
    }
}

struct MobileHome: View {
    @ObservedObject var client: IOSClient
    @State private var address = ""
    var body: some View {
        TabView {
            NavigationStack {
                List {
                    Section {
                        Label("BitSkim 0.2 · Experimental", systemImage: "arrow.left.arrow.right")
                        Text("Keep BitSkim open to receive. Incoming files are saved in Files → On My iPhone/iPad → BitSkim → Received.").font(.footnote).foregroundStyle(.secondary)
                    }
                    if !client.sharedFiles.isEmpty {
                        Section("Shared inbox · \(client.sharedFiles.count) files") {
                            ForEach(client.sharedFiles, id: \.self) { Text($0.lastPathComponent) }
                            Text("Choose a computer below to send these files.").font(.footnote).foregroundStyle(.secondary)
                        }
                    }
                    Section("Nearby computers") {
                        ForEach(client.peers) { peer in
                            Button { client.prepare(peer: peer) } label: {
                                VStack(alignment: .leading) {
                                    Label(peer.name, systemImage: "desktopcomputer")
                                    Text("\(peer.platform) · \(peer.address)").font(.caption).foregroundStyle(.secondary)
                                }
                            }.disabled(client.active || client.importing)
                        }
                        if client.peers.isEmpty {
                            Text(client.discovery ? "No computers found. Check Local Network permission, Wi-Fi and the other computer’s firewall." : "This build uses manual connections. Enter the computer’s IPv4 address below.")
                                .font(.footnote).foregroundStyle(.secondary)
                        }
                        if client.discovery { Button("Refresh computers") { client.refresh() } }
                    }
                    Section("Receive on this device") {
                        ForEach(client.addresses, id: \.self) { ip in Text("\(ip):\(client.listeningPort)").monospaced().textSelection(.enabled) }
                        if client.addresses.isEmpty { Text("Connect to Wi-Fi to see a local receiving address.").font(.footnote).foregroundStyle(.secondary) }
                        Text("Enter this address on the sending computer. BitSkim must stay open.").font(.footnote).foregroundStyle(.secondary)
                    }
                    Section("Connect by IP") {
                        TextField("192.168.1.20 or 192.168.1.20:46440", text: $address)
                            .keyboardType(.numbersAndPunctuation).textInputAutocapitalization(.never).autocorrectionDisabled()
                        Button(client.sharedFiles.isEmpty ? "Choose files to send" : "Send shared files") { client.prepare(address: address) }
                            .disabled(client.active || client.importing || address.isEmpty)
                        Text("Use the receiving computer’s LAN address. It must be running BitSkim and allow incoming connections.").font(.footnote).foregroundStyle(.secondary)
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
            }.tabItem { Label("Computers", systemImage: "network") }
            NavigationStack { ReceivedFiles(folder: client.received) }.tabItem { Label("Received", systemImage: "tray.and.arrow.down") }
            NavigationStack {
                List {
                    Section("Device pairing") { PairingControls(client: client) }
                    Section("Encryption") {
                        Picker("Mode", selection: Binding(get: { client.security }, set: { client.setSecurity($0) })) {
                            Text("Disabled").tag(0); Text("Prefer encryption").tag(1); Text("Require encryption").tag(2)
                        }.disabled(client.active)
                        Text("Prefer can use plaintext with older clients. Require rejects plaintext. Compare the full code on both computers before trusting a device.").font(.footnote).foregroundStyle(.secondary)
                    }
                    Section("Recent transfers") {
                        ForEach(client.history) { entry in
                            VStack(alignment: .leading, spacing: 5) {
                                Text(entry.name).font(.headline); Text(entry.message); Text(entry.integrity).font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                    Section("Diagnostics") { Text(client.diagnostics).font(.system(.caption, design: .monospaced)).textSelection(.enabled) }
                    Section {
                        Link("Website", destination: URL(string: "https://bitskim.retrocs.org")!)
                        Link("Source code", destination: URL(string: "https://github.com/retrocs-org/bitskim")!)
                        Text("Experimental iPhone/iPad client. No always-on background receiving.").font(.footnote).foregroundStyle(.secondary)
                    }
                }.navigationTitle("Transfer tools")
            }.tabItem { Label("Tools", systemImage: "slider.horizontal.3") }
        }
        .fileImporter(isPresented: $client.filePicker, allowedContentTypes: [.item], allowsMultipleSelection: true) { client.imported($0) }
        .sheet(item: $client.offer) { incoming in IncomingOffer(client: client, incoming: incoming).interactiveDismissDisabled() }
        .alert("BitSkim", isPresented: Binding(get: { client.error != nil }, set: { if !$0 { client.error = nil } })) {
            Button("OK") { client.error = nil }
        } message: { Text(client.error ?? "") }
    }
}
struct PairingControls: View {
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
    @ObservedObject var client: IOSClient
    let incoming: MobileOffer
    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("\(incoming.peer) wants to send \(incoming.count > 0 ? "a collection" : incoming.name)")
                    Text("Computer names are not verified. Existing files are preserved with safe renamed destinations.").font(.footnote).foregroundStyle(.secondary)
                }
                Section("Device identity") { PairingControls(client: client) }
                if incoming.count > 0 {
                    Section("Files and folders · \(incoming.count) items") {
                        ForEach(0..<incoming.count, id: \.self) { index in Text(client.manifestName(index, serial: incoming.id)).font(.caption) }
                    }
                } else { Section("File") { Text(incoming.name); Text("\(incoming.size) bytes") } }
                Section {
                    Text("Save into BitSkim’s Received folder, available in Files.")
                    Text(client.spaceReport(incoming)).font(.caption).foregroundStyle(.secondary)
                    if let error = client.error { Text(error).foregroundStyle(.red) }
                    Button("Accept and receive") { client.accept(incoming) }
                    Button("Decline", role: .destructive) { client.decline(incoming) }
                }
            }.navigationTitle("Incoming transfer")
        }
    }
}
struct ReceivedFiles: View {
    let folder: URL
    @State private var files: [URL] = []
    var body: some View {
        List {
            if files.isEmpty { Text("No received files in this folder.").foregroundStyle(.secondary) }
            ForEach(files, id: \.self) { file in
                if (try? file.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true {
                    NavigationLink { ReceivedFiles(folder: file) } label: { Label(file.lastPathComponent, systemImage: "folder") }
                } else { ShareLink(item: file) { Label(file.lastPathComponent, systemImage: "doc") } }
            }
        }.navigationTitle(folder.lastPathComponent).onAppear(perform: reload).refreshable { reload() }
    }
    private func reload() {
        files = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.isDirectoryKey], options: [.skipsHiddenFiles])) ?? []).sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
    }
}
