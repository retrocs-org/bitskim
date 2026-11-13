import SwiftUI
import AppKit
import UniformTypeIdentifiers

private func text<T>(_ value: T) -> String {
    var copy = value
    return withUnsafePointer(to: &copy) {
        $0.withMemoryRebound(to: CChar.self, capacity: MemoryLayout<T>.size) { String(cString: $0) }
    }
}
struct Peer: Identifiable, Equatable {
    let id: String, name: String, platform: String, address: String
    let port: UInt16
    var features: UInt32 = 0
    var receiverLimits = "Capabilities checked when connecting"
    var transferFeatures: String {
        var options = ["File transfers"]
        if features & UInt32(BS_CAP_DIRECTORIES) != 0 { options.append("Folder transfers available") }
        if features & UInt32(BS_CAP_BATCH) != 0 { options.append("Batch approval available") }
        if features & UInt32(BS_CAP_TLS) != 0 { options.append("TLS available") }
        if features & UInt32(BS_CAP_DEVICE_TRUST) != 0 { options.append("Verified device pairing available") }
        if features & UInt32(BS_CAP_DISK_VERIFY) != 0 { options.append("Written-file verification available") }
        if features & UInt32(BS_CAP_RESUME) != 0 { options.append("Interrupted-file resume available") }
        return options.joined(separator: " · ") + "\n" + receiverLimits + "\nActive-file pause is not supported. Queue pause takes effect after the current file."
    }
}

final class CollectionTable: NSObject, NSTableViewDataSource {
    let engine: OpaquePointer
    let first: Int
    let count: Int
    let serial: UInt
    init(engine: OpaquePointer, first: Int, count: Int) {
        self.engine = engine; self.first = first; self.count = count
        self.serial = bs_transfer_serial(engine)
    }
    func numberOfRows(in tableView: NSTableView) -> Int { count - first }
    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        guard bs_transfer_serial(engine) == serial,
              let pointer = bs_batch_item(engine, Int32(first + row)) else { return "" }
        var item = pointer.pointee
        let name = withUnsafePointer(to: &item.filename) {
            $0.withMemoryRebound(to: CChar.self, capacity: 256) { String(cString: $0) }
        }
        if name.hasSuffix("/") { return name + " (folder)" }
        return "\(name) · \(ByteCountFormatter.string(fromByteCount: Int64(item.total), countStyle: .file))"
    }
}

final class ClientModel: ObservableObject {
    @Published var peers: [Peer] = []
    @Published var status = "Starting BitSkim…"
    @Published var filename = ""
    @Published var fraction = 0.0
    @Published var transferAmounts = ""
    @Published var transferSpeed = ""
    @Published var active = false
    @Published var scanning = false
    @Published var queued = 0
    @Published var queuePaused = false
    @Published var waiting = 0
    @Published var filePosition = ""
    @Published var encrypted = false
    @Published var sendLimitKiB: String
    @Published var receiveLimitKiB: String
    @Published var securityMode: Int
    @Published var pairingKey: String
    @Published var resumable: Bool
    @Published var manual = false
    @Published var selected: String?
    @Published var manualIP = ""
    @Published var manualPort = String(BS_PORT)
    @Published var name: String
    @Published var visible: Bool
    @Published var receiving: Bool
    @Published var completionSound: Bool
    private var completionSerial: UInt = 0
    @Published var autoAccept: Bool
    @Published var trustedAutoAcceptOnly: Bool
    @Published var askLocation: Bool
    @Published var folder: String
    @Published var startupError = ""
    @Published var themeID: String
    @Published var keepInMenuBar: Bool
    @Published var startInMenuBar: Bool
    var revealWindow: (() -> Void)?
    var openPreferences: (() -> Void)?
    var hideWindow: (() -> Void)?
    var preferencesChanged: (() -> Void)?
    var theme: AppTheme { AppTheme.find(themeID) }
    var transportMode: Int { engine.map { Int(bs_security_mode($0)) } ?? 0 }
    fileprivate var engine: OpaquePointer?
    private var timer: Timer?
    private var deciding = false
    private var decisionSerial: UInt = 0
    private var incomingAlert: NSAlert?
    private var incomingPanel: NSSavePanel?
    private var lastDisplayUpdate = Date.distantPast
    private var platformReady = false
    @Published var tools = false
    @Published var conflictPolicy = 0
    @Published var otherPairingCode = ""
    @Published var progressSummary = ""
    @Published var testAddress = ""
    func configureFolders(_ client: OpaquePointer) {
        let directory = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("BitSkim")
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            _ = bs_folders_configure(client, directory.appendingPathComponent("folders.dat").path)
        } catch { self.error("Could not save recent folders.") }
    }
    func recentFolder(_ incoming: Bool) -> String {
        guard let engine else { return folder }
        let value = String(cString: bs_last_folder(engine, incoming ? 1 : 0))
        return value.isEmpty ? folder : value
    }
    func testConnection() {
        guard let engine else { return }
        if bs_network_test(engine, testAddress, 0) == 0 { error(String(cString: bs_last_error(engine))) }
    }
    func diagnostics() -> String {
        guard let engine else { return startupError }
        var bytes = [CChar](repeating: 0, count: 2048); _ = bs_diagnostics(engine, &bytes, bytes.count)
        return String(cString: bytes)
    }
    var historyCount: Int { engine.map { Int(bs_history_count($0)) } ?? 0 }
    func historyText(_ index: Int) -> String {
        guard let engine else { return "" }; var bytes = [CChar](repeating: 0, count: 2048)
        _ = bs_history_text(engine, Int32(index), &bytes, bytes.count); return String(cString: bytes)
    }
    func historySerial(_ index: Int) -> UInt {guard let engine else {return 0};var item=BSHistoryItem();return bs_history_item(engine,Int32(index),&item) != 0 ? item.serial : 0}
    func retryHistory(_ serial: UInt) {
        guard let engine else { return }; let index=bs_history_find(engine,serial); if index<0 || bs_retry_history(engine, index) == 0 { error(String(cString: bs_last_error(engine))) }
    }
    func revealHistory(_ serial: UInt) {
        guard let engine else { return }; var item = BSHistoryItem()
        let index=bs_history_find(engine,serial);guard index>=0 && bs_history_item(engine, index, &item) != 0 else { return }
        let url = URL(fileURLWithPath: text(item.path))
        if FileManager.default.fileExists(atPath: url.path) { NSWorkspace.shared.activateFileViewerSelecting([url]) }
    }
    var verificationCode: String {
        guard let engine else { return "" }; var bytes = [CChar](repeating: 0, count: 20)
        return bs_pairing_code(engine, &bytes) != 0 ? String(cString: bytes) : ""
    }
    var trustedDevice: Bool { engine.map { bs_trust_status($0) == 1 } ?? false }
    func trustDevice() {
        guard let engine else { return }
        if bs_trust_current(engine, otherPairingCode.uppercased()) == 0 { error(String(cString: bs_last_error(engine))) }
        else { otherPairingCode = ""; objectWillChange.send() }
    }
    func forgetDevice() {
        guard let engine else { return }; let alert = NSAlert()
        alert.messageText = "Forget this device’s saved identity?"
        alert.informativeText = "Compare verification codes again before trusting it in the future."
        alert.addButton(withTitle: "Forget"); alert.addButton(withTitle: "Cancel")
        if alert.runModal() == .alertFirstButtonReturn { _ = bs_forget_current(engine); objectWillChange.send() }
    }
    private let defaults = UserDefaults.standard

    init() {
        sendLimitKiB = defaults.string(forKey: "sendLimitKiB") ?? "0"
        receiveLimitKiB = defaults.string(forKey: "receiveLimitKiB") ?? "0"
        securityMode = (defaults.object(forKey: "securityMode") as? Int) ?? 1
        pairingKey = defaults.string(forKey: "pairingKey") ?? ""
        resumable = defaults.object(forKey: "resumable") as? Bool ?? true
        themeID = AppTheme.find(defaults.string(forKey: "theme") ?? "mocha").id
        keepInMenuBar = defaults.object(forKey: "keepInMenuBar") as? Bool ?? true
        startInMenuBar = defaults.bool(forKey: "startInMenuBar")
        name = defaults.string(forKey: "name") ?? ProcessInfo.processInfo.hostName
        visible = defaults.object(forKey: "visible") as? Bool ?? true
        receiving = defaults.object(forKey: "receiving") as? Bool ?? true
        autoAccept = defaults.bool(forKey: "autoAccept")
        trustedAutoAcceptOnly = defaults.object(forKey: "trustedAutoAcceptOnly") as? Bool ?? true
        completionSound = defaults.bool(forKey: "completionSound")
        askLocation = defaults.object(forKey: "askLocation") as? Bool ?? true
        folder = defaults.string(forKey: "folder") ?? FileManager.default.urls(for: .downloadsDirectory, in: .userDomainMask)[0].path
        if startInMenuBar { keepInMenuBar = true }
        let id = defaults.string(forKey: "installationID") ?? UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
        defaults.set(id, forKey: "installationID")
        platformReady = bs_platform_init() != 0
        if platformReady { engine = bs_create_app(wireName, "macOS", id, visible ? 1 : 0, receiving ? 1 : 0) }
        if let engine { configureFolders(engine) }
        if let engine {
            let directory = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("BitSkim/Trust", isDirectory: true)
            do { try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
                if bs_trust_configure(engine, directory.path) == 0 { startupError = "Cannot initialize saved device identities." }
            } catch { startupError = "Cannot open the device trust folder: \(error.localizedDescription)" }
            if bs_security_configure(engine, Int32(securityMode), pairingKey) == 0 {
                startupError = String(cString: bs_last_error(engine))
                bs_destroy(engine); self.engine = nil
            }
            if self.engine != nil {
                var up: UInt = 0, down: UInt = 0
                if bs_limit_parse(sendLimitKiB,&up) == 0 || bs_limit_parse(receiveLimitKiB,&down) == 0 {sendLimitKiB="0";receiveLimitKiB="0";up=0;down=0}
                bs_limits_configure(engine,up,down)
                bs_resume_enable(engine, resumable ? 1 : 0); bs_batch_enable(engine, 1) }
        }
        if engine == nil {
            if startupError.isEmpty { startupError = "Cannot listen on port \(BS_PORT). Close any other BitSkim instance, then reopen the app." }
            status = startupError
        }
        timer = Timer(timeInterval: 0.02, repeats: true) { [weak self] _ in self?.tick() }
        RunLoop.main.add(timer!, forMode: .common)
    }
    deinit {
        timer?.invalidate()
        if let engine { bs_destroy(engine) }
        if platformReady { bs_platform_cleanup() }
    }
    var wireName: String {
        let clean = name.unicodeScalars.filter { $0.value >= 32 && $0.value != 127 }
        var result = String(String.UnicodeScalarView(clean))
        while result.utf8.count > 63 { result.removeLast() }
        return result.isEmpty ? "BitSkim" : result
    }
    func savePreferences() {
        var up: UInt = 0, down: UInt = 0
        guard bs_limit_parse(sendLimitKiB, &up) != 0, bs_limit_parse(receiveLimitKiB, &down) != 0 else { error("Speed limits: enter 0–1048575 KiB/s. Zero means Unlimited."); return }
        if engine == nil && platformReady {
            let candidate = bs_create_app(wireName, "macOS", defaults.string(forKey: "installationID")!, visible ? 1 : 0, receiving ? 1 : 0)
            if let candidate {
                configureFolders(candidate)
                if bs_security_configure(candidate, Int32(securityMode), pairingKey) == 0 {
                    let message = String(cString: bs_last_error(candidate)); bs_destroy(candidate); error(message); return
                }
                engine = candidate; bs_batch_enable(candidate, 1); startupError = ""
            }
        }
        if let engine, !active, queued == 0 {
            guard bs_security_configure(engine, Int32(securityMode), pairingKey) != 0 else {
                error(String(cString: bs_last_error(engine))); return
            }
            bs_resume_enable(engine, resumable ? 1 : 0)
            defaults.set(securityMode, forKey: "securityMode")
            defaults.set(pairingKey, forKey: "pairingKey")
            defaults.set(resumable, forKey: "resumable")
        }
        if let engine {bs_limits_configure(engine,up,down)}
        defaults.set(sendLimitKiB, forKey: "sendLimitKiB"); defaults.set(receiveLimitKiB, forKey: "receiveLimitKiB")
        defaults.set(name, forKey: "name"); defaults.set(visible, forKey: "visible")
        defaults.set(completionSound, forKey: "completionSound")
        defaults.set(receiving, forKey: "receiving"); defaults.set(autoAccept, forKey: "autoAccept"); defaults.set(trustedAutoAcceptOnly, forKey: "trustedAutoAcceptOnly")
        defaults.set(askLocation, forKey: "askLocation"); defaults.set(folder, forKey: "folder")
        if startInMenuBar { keepInMenuBar = true }
        defaults.set(themeID, forKey: "theme")
        defaults.set(keepInMenuBar, forKey: "keepInMenuBar")
        defaults.set(startInMenuBar, forKey: "startInMenuBar")
        if let engine { bs_configure(engine, wireName, visible ? 1 : 0, receiving ? 1 : 0) }
        preferencesChanged?()
    }
    private func byteSize(_ value: UInt) -> String {
        if value < 1024 { return "\(value) B" }
        if value < 1048576 { return String(format: "%.1f KiB", Double(value) / 1024) }
        if value < 1073741824 { return String(format: "%.1f MiB", Double(value) / 1048576) }
        return String(format: "%.2f GiB", Double(value) / 1073741824)
    }
    private func tick() {
        guard let engine else { return }
        bs_service(engine, 4)
        let current = bs_status(engine)!.pointee
        let currentSerial = bs_transfer_serial(engine)
        if deciding && (current.awaiting_decision == 0 || currentSerial != decisionSerial) {
            if let alert = incomingAlert {
                if let parent = alert.window.sheetParent { parent.endSheet(alert.window, returnCode: .abort) }
                else if NSApp.modalWindow === alert.window { NSApp.abortModal(); alert.window.orderOut(nil) }
            }
            incomingPanel?.cancel(nil)
            incomingAlert = nil; incomingPanel = nil; deciding = false
        }
        if current.awaiting_decision != 0 && !deciding {
            deciding = true; decisionSerial = currentSerial
            let peer = text(current.peer_name), address = text(current.peer_address)
            let file = text(current.filename), size = current.total
            DispatchQueue.main.async { [weak self] in
                guard let self, self.offerMatches(currentSerial) else { return }
                self.incoming(peer: peer, address: address, file: file, size: size)
            }
        }
        let now = Date()
        guard now.timeIntervalSince(lastDisplayUpdate) >= 0.2 else { return }
        lastDisplayUpdate = now
        if bs_completion_notice(engine, &completionSerial) != 0 && completionSound { NSSound.beep() }
        queued = Int(bs_queue_count(engine)); waiting = Int(bs_queue_waiting(engine)); queuePaused = bs_queue_paused(engine) != 0
        scanning = bs_queue_scanning(engine) != 0
        encrypted = bs_transfer_encrypted(engine) != 0
        let snapshot = bs_status(engine)!.pointee
        var position: UInt = 0, count: UInt = 0
        bs_queue_progress(engine, &position, &count)
        if snapshot.active != 0 && snapshot.incoming != 0 && (bs_batch_count(engine) > 1 || bs_batch_is_tree(engine) != 0) {
            filePosition = "File \(bs_batch_index(engine) + 1)/\(bs_batch_count(engine))"
            waiting = Int(bs_batch_count(engine) - bs_batch_index(engine) - 1)
        } else { filePosition = snapshot.active != 0 && snapshot.incoming == 0 && queued > 0 ? "File \(position)/\(count)" : "" }
        var report = [CChar](repeating: 0, count: 256)
        _ = bs_progress_summary(engine, &report, report.count)
        let summaryText = String(cString: report)
        if progressSummary != summaryText { progressSummary = summaryText }
        let code = verificationCode
        let nextStatus = text(snapshot.message) + (snapshot.active != 0 && !code.isEmpty ? " · Verify: " + code : ""), nextFilename = text(snapshot.filename)
        let nextActive = snapshot.active != 0
        let nextFraction = snapshot.total == 0 ? 0 : Double(snapshot.transferred) / Double(snapshot.total)
        if status != nextStatus { status = nextStatus }
        if filename != nextFilename { filename = nextFilename }
        if active != nextActive { active = nextActive }
        if fraction != nextFraction { fraction = nextFraction }
        let done = min(snapshot.transferred, snapshot.total)
        let amounts = "\(byteSize(done)) / \(byteSize(snapshot.total)) · \(Int(nextFraction * 100))% · \(byteSize(snapshot.total - done)) left"
        let speed = byteSize(bs_transfer_rate(engine)) + "/s"
        if transferAmounts != amounts { transferAmounts = amounts }
        if transferSpeed != speed { transferSpeed = speed }
        var found: [Peer] = []
        for index in 0..<bs_peer_count(engine) {
            let p = bs_peer(engine, index)!.pointee
            var summary = [CChar](repeating: 0, count: 512)
            _ = bs_peer_summary(engine, text(p.id), &summary, summary.count)
            let limits = String(cString: summary)
            found.append(Peer(id: text(p.id) + ":" + text(p.address), name: text(p.name), platform: text(p.platform), address: text(p.address), port: p.port, features: bs_peer_features(engine, text(p.id)), receiverLimits: limits))
        }
        found.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
        if peers != found { peers = found }
    }

    func refresh() { if let engine { bs_refresh(engine) } }
    func cancel() { if let engine { bs_cancel(engine) } }
    func applyTheme(_ id: String) { themeID = AppTheme.find(id).id; savePreferences() }
    func error(_ message: String) {
        revealWindow?()
        let alert = NSAlert(); alert.messageText = "BitSkim"; alert.informativeText = message
        alert.addButton(withTitle: "OK")
        if let window = NSApp.keyWindow { alert.beginSheetModal(for: window) }
        else { alert.runModal() }
    }
    func generateKey() {
        var bytes = [CChar](repeating: 0, count: 65)
        if bs_generate_pairing_key(&bytes) != 0 { pairingKey = String(cString: bytes) }
    }
    func send(_ url: URL, to peer: Peer) {
        guard let engine else { error(startupError); return }
        guard securityMode == transportMode else { error("Save your encryption preferences before sending."); return }
        guard bs_status(engine)!.pointee.active == 0, bs_queue_scanning(engine) == 0 else { error("A transfer is already in progress."); return }
        let wasPaused = bs_queue_count(engine) > 0 && bs_queue_paused(engine) != 0
        let result: Int32
        if peer.id.count > 32 { result = bs_queue_peer(engine, String(peer.id.prefix(32)), url.path) }
        else { result = bs_queue_address(engine, peer.address, peer.port, url.path) }
        if result == 0 { error(String(cString: bs_last_error(engine))) }
        else if !wasPaused { bs_queue_continue(engine) }
    }
    func chooseFile(for peer: Peer) {
        revealWindow?()
        let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = true; panel.allowsMultipleSelection = true
        panel.directoryURL = URL(fileURLWithPath: recentFolder(false))
        panel.message = "Choose a file or directory to send to \(peer.name)"
        panel.begin { [weak self] response in if response == .OK { for url in panel.urls { self?.send(url, to: peer) } } }
    }
    func sendSelected() {
        guard let peer = peers.first(where: { $0.id == selected }) else { return }
        chooseFile(for: peer)
    }
    func sendManual() {
        guard let port = UInt16(manualPort), port > 0 else { error("Enter a port between 1 and 65535."); return }
        let ip = manualIP.trimmingCharacters(in: .whitespacesAndNewlines)
        let parts = ip.split(separator: ".", omittingEmptySubsequences: false)
        guard parts.count == 4, parts.allSatisfy({ UInt8($0) != nil }) else { error("Enter an IPv4 address, such as 192.168.1.20."); return }
        chooseFile(for: Peer(id: ip, name: ip, platform: "Manual connection", address: ip, port: port))
    }
    func drop(_ providers: [NSItemProvider], on peer: Peer) -> Bool {
        guard !providers.isEmpty else { return false }
        let group = DispatchGroup()
        var files = [URL?](repeating: nil, count: providers.count)
        for (index, provider) in providers.enumerated() {
            group.enter()
            provider.loadItem(forTypeIdentifier: UTType.fileURL.identifier, options: nil) { value, _ in
                var file = value as? URL
                if let data = value as? Data { file = URL(dataRepresentation: data, relativeTo: nil) }
                DispatchQueue.main.async { files[index] = file; group.leave() }
            }
        }
        group.notify(queue: .main) { [weak self] in
            guard let self else { return }
            guard files.allSatisfy({ $0 != nil }) else { self.error("Could not read every dropped item."); return }
            for file in files.compactMap({ $0 }) { self.send(file, to: peer) }
        }
        return true
    }
    private var automaticallyAcceptCurrent: Bool {
        guard let engine else { return false }
        return autoAccept && (!trustedAutoAcceptOnly || bs_trust_status(engine) == 1)
    }
    private func offerMatches(_ serial: UInt) -> Bool {
        guard let engine else { return false }
        let state = bs_status(engine)!.pointee
        return bs_transfer_serial(engine) == serial && state.incoming != 0 && state.awaiting_decision != 0
    }
    private func incoming(peer: String, address: String, file: String, size: UInt) {
        guard let engine, bs_status(engine)!.pointee.awaiting_decision != 0 else { return }
        let serial = bs_transfer_serial(engine)
        NSApp.requestUserAttention(.informationalRequest)
        if (bs_batch_count(engine) > 1 || bs_batch_is_tree(engine) != 0) { incomingBatch(peer: peer); return }
        if automaticallyAcceptCurrent { chooseDestination(file, serial: serial); return }
        revealWindow?()
        let alert = NSAlert(); alert.messageText = "\(peer) wants to send a file"
        alert.informativeText = "\(file) · \(ByteCountFormatter.string(fromByteCount: Int64(size), countStyle: .file))\nFrom \(address)\n\n\(bs_transfer_encrypted(engine) != 0 ? "Encrypted; verify the device in Transfer tools." : "This transfer is unencrypted.") Computer names are not verified."
        let policy = NSPopUpButton(frame: NSRect(x: 0, y: 0, width: 360, height: 26))
        policy.addItems(withTitles: ["Existing files: Rename safely", "Existing files: Skip", "Existing files: Replace"])
        policy.selectItem(at: conflictPolicy); alert.accessoryView = policy
        alert.addButton(withTitle: "Accept"); alert.addButton(withTitle: "Decline")
        var report = [CChar](repeating: 0, count: 256)
        _ = bs_progress_summary(engine, &report, report.count)
        let summaryText = String(cString: report)
        if progressSummary != summaryText { progressSummary = summaryText }
        let code = verificationCode
        if !code.isEmpty && !trustedDevice {
            alert.informativeText += "\n\nCompare with the sender: " + code + "\nChoose Accept & Trust only if both screens show the same full code."
            alert.addButton(withTitle: "Accept & Trust")
        }
        let decision: (NSApplication.ModalResponse) -> Void = { [weak self] response in
            guard let self, let engine = self.engine, self.offerMatches(serial) else { return }
            self.conflictPolicy = policy.indexOfSelectedItem; bs_conflict_policy(engine, Int32(self.conflictPolicy))
            if response == .alertThirdButtonReturn {
                if bs_trust_current(engine, code) == 0 { let reason = String(cString: bs_last_error(engine)); bs_decline(engine); self.error(reason); return }
                self.chooseDestination(file, serial: serial)
            } else if response == .alertFirstButtonReturn { self.chooseDestination(file, serial: serial) } else { bs_decline(engine) }
        }
        incomingAlert = alert
        if let window = NSApp.windows.first(where: { $0.isVisible }) { alert.beginSheetModal(for: window, completionHandler: decision) }
        else { decision(alert.runModal()) }
    }
    private func spaceReport(_ destination: String, collection: Bool) -> String {
        guard let engine else { return "" };var bytes=[CChar](repeating:0,count:256)
        _ = bs_destination_space_report(engine,destination,collection ? 1 : 0,&bytes,bytes.count);return String(cString:bytes)
    }
    private func incomingBatch(peer: String) {
        guard let engine else { return }
        bs_conflict_policy(engine, Int32(conflictPolicy))
        let serial = bs_transfer_serial(engine)
        let first = Int(bs_batch_index(engine)), count = Int(bs_batch_count(engine))
        let receive: (URL) -> Void = { [weak self] url in
            guard let self, let engine = self.engine, bs_transfer_serial(engine) == serial,
                  bs_status(engine)!.pointee.awaiting_decision != 0 else { return }
            if bs_accept_batch(engine, url.path) == 0 {
                let reason = String(cString: bs_last_error(engine)); bs_decline(engine); self.error(reason)
            }
        }
        let choose: () -> Void = { [weak self] in
            guard let self, let engine = self.engine, bs_transfer_serial(engine) == serial,
                  bs_status(engine)!.pointee.awaiting_decision != 0 else { return }
            if !self.askLocation { receive(URL(fileURLWithPath: self.recentFolder(true))); return }
            let panel = NSOpenPanel(); panel.canChooseFiles = false; panel.canChooseDirectories = true
            panel.allowsMultipleSelection = false; panel.title = "Save all incoming files"; panel.prompt = "Receive all"
            panel.message = "The hierarchy is preserved in a new folder; existing roots are kept. " + self.spaceReport(self.folder, collection: true)
            panel.directoryURL = URL(fileURLWithPath: self.recentFolder(true))
            self.incomingPanel = panel
            panel.begin { response in
                guard let engine = self.engine, self.offerMatches(serial) else { return }
                if response == .OK, let url = panel.url { receive(url) } else { bs_decline(engine) }
            }
        }
        if automaticallyAcceptCurrent { choose(); return }
        revealWindow?()
        var summary = [CChar](repeating: 0, count: 180); _ = bs_batch_summary(engine, &summary, summary.count)
        let alert = NSAlert(); alert.messageText = "\(peer): \(String(cString: summary))"
        let source = CollectionTable(engine: engine, first: first, count: count)
        let table = NSTableView(frame: NSRect(x: 0, y: 0, width: 460, height: 220))
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("file"))
        column.width = 440; column.title = "Files"; table.addTableColumn(column)
        table.dataSource = source; table.rowHeight = 24
        let scroll = NSScrollView(frame: NSRect(x: 0, y: 0, width: 460, height: 220))
        scroll.hasVerticalScroller = true; scroll.documentView = table
        let policy = NSPopUpButton(frame: NSRect(x: 0, y: 228, width: 460, height: 26))
        policy.addItems(withTitles: ["Existing files: Rename safely", "Existing files: Skip", "Existing files: Replace"])
        policy.selectItem(at: conflictPolicy)
        let accessory = NSView(frame: NSRect(x: 0, y: 0, width: 460, height: 260))
        accessory.addSubview(scroll); accessory.addSubview(policy); alert.accessoryView = accessory
        alert.informativeText = spaceReport(folder, collection: true) + "\nAccepting applies only to this list. Choose one folder for all files. Existing files use the session conflict policy in Transfer tools. Sender names are unverified."
        alert.addButton(withTitle: "Accept all"); alert.addButton(withTitle: "Decline")
        var report = [CChar](repeating: 0, count: 256)
        _ = bs_progress_summary(engine, &report, report.count)
        let summaryText = String(cString: report)
        if progressSummary != summaryText { progressSummary = summaryText }
        let code = verificationCode
        if !code.isEmpty && !trustedDevice { alert.informativeText += "\nCompare with the sender: " + code + "\nTrust only if both full codes match."; alert.addButton(withTitle: "Accept all & Trust") }
        let decision: (NSApplication.ModalResponse) -> Void = { [weak self, source] response in
            _ = source // Keep the lazy table source alive for the approval sheet.
            guard let engine = self?.engine, bs_transfer_serial(engine) == serial,
                  bs_status(engine)!.pointee.awaiting_decision != 0 else { return }
            self?.conflictPolicy = policy.indexOfSelectedItem; bs_conflict_policy(engine, Int32(policy.indexOfSelectedItem))
            if response == .alertThirdButtonReturn {
                if bs_trust_current(engine, code) == 0 { let reason = String(cString: bs_last_error(engine)); bs_decline(engine); self?.error(reason); return }; choose()
            } else if response == .alertFirstButtonReturn { choose() } else { bs_decline(engine) }
        }
        incomingAlert = alert
        if let window = NSApp.keyWindow { alert.beginSheetModal(for: window, completionHandler: decision) }
        else { decision(alert.runModal()) }
    }
    private func uniqueURL(_ url: URL) -> URL {
        if !FileManager.default.fileExists(atPath: url.path) { return url }
        let parent = url.deletingLastPathComponent(), ext = url.pathExtension
        let base = url.deletingPathExtension().lastPathComponent
        for n in 1...9999 {
            let candidate = parent.appendingPathComponent(base + " (\(n))" + (ext.isEmpty ? "" : "." + ext))
            if !FileManager.default.fileExists(atPath: candidate.path) { return candidate }
        }
        return parent.appendingPathComponent(UUID().uuidString + "-" + url.lastPathComponent)
    }
    private func accept(_ url: URL, serial: UInt) {
        guard let engine, offerMatches(serial) else { return }
        bs_conflict_policy(engine, Int32(conflictPolicy))
        var destination = url
        if FileManager.default.fileExists(atPath: url.path) {
            switch conflictPolicy {
            case 1: if bs_skip_incoming(engine) == 0 { error(String(cString: bs_last_error(engine))) }; return
            case 2: if bs_accept_conflict(engine, url.path) == 0 { error(String(cString: bs_last_error(engine))) }; return
            default: destination = uniqueURL(url)
            }
        }
        var restart: Int32 = 0
        if bs_resume_available(engine, destination.path) == 1 {
            let alert = NSAlert(); alert.messageText = "Resume interrupted file?"
            alert.informativeText = "BitSkim will check the retained data against the sender before continuing. Restart discards this file’s checkpoint."
            alert.addButton(withTitle: "Resume"); alert.addButton(withTitle: "Restart"); alert.addButton(withTitle: "Cancel")
            incomingAlert = alert
            let answer = alert.runModal()
            guard offerMatches(serial) else { return }
            if answer == .alertThirdButtonReturn { bs_decline(engine); return }
            if answer == .alertSecondButtonReturn { restart = 1 }
        }
        if bs_accept_resumable(engine, destination.path, restart) == 0 {
            let reason = String(cString: bs_last_error(engine)); bs_decline(engine); error(reason)
        }
    }
    private func chooseDestination(_ file: String, serial: UInt) {
        guard offerMatches(serial) else { return }
        if !askLocation {
            var isDirectory: ObjCBool = false
            if FileManager.default.fileExists(atPath: folder, isDirectory: &isDirectory), isDirectory.boolValue,
               FileManager.default.isWritableFile(atPath: folder) {
                accept(URL(fileURLWithPath: folder).appendingPathComponent(file), serial: serial); return
            }
        }
        revealWindow?()
        let panel = NSSavePanel(); panel.nameFieldStringValue = file
        panel.title = "Save incoming file"; panel.prompt = "Receive"
        panel.message = spaceReport(URL(fileURLWithPath: folder).appendingPathComponent(file).path, collection: false)
        panel.message += "\nExisting files use the conflict policy in Transfer tools; Rename is the default."
        panel.directoryURL = URL(fileURLWithPath: folder)
        incomingPanel = panel
        panel.begin { [weak self] response in
            guard let self, let engine = self.engine, self.offerMatches(serial) else { return }
            if response == .OK, let url = panel.url { self.accept(url, serial: serial) }
            else { bs_decline(engine) }
        }
    }
    func chooseFolder() {
        let panel = NSOpenPanel(); panel.canChooseFiles = false; panel.canChooseDirectories = true
        panel.prompt = "Use folder"; panel.directoryURL = URL(fileURLWithPath: folder)
        panel.begin { [weak self] response in
            if response == .OK, let url = panel.url { self?.folder = url.path; self?.savePreferences() }
        }
    }
}

final class PeerTileState: ObservableObject { @Published var targeted = false; @Published var hovered = false }
struct PeerTile: View {
    let peer: Peer
    @ObservedObject var model: ClientModel
    @StateObject private var tile = PeerTileState()
    private var highlighted: Bool { tile.targeted || tile.hovered }
    var body: some View {
        Button {
            model.selected = peer.id
            model.chooseFile(for: peer)
        } label: {
            VStack(spacing: 6) {
                Image(systemName: peer.platform.lowercased().contains("amiga") ? "desktopcomputer" : "laptopcomputer")
                    .font(.system(size: 24, weight: .light))
                    .foregroundStyle(model.theme.accent)
                    .frame(width: 48, height: 48)
                    .background(model.theme.base, in: Circle())
                    .accessibilityHidden(true)
                Text(peer.name).font(.system(size: 12, weight: .semibold))
                    .multilineTextAlignment(.center).lineLimit(2).frame(height: 32)
                Text(peer.platform).font(.system(size: 10)).foregroundStyle(model.theme.muted).lineLimit(1)
                Text(peer.address + (peer.port == BS_PORT ? "" : ":\(peer.port)"))
                    .font(.system(size: 9, design: .monospaced)).foregroundStyle(model.theme.muted)
                    .lineLimit(1).truncationMode(.middle)
            }
            .padding(12).frame(maxWidth: .infinity, maxHeight: .infinity)
            .aspectRatio(1, contentMode: .fit)
            .background(tile.targeted ? model.theme.accent.opacity(0.18) : model.theme.surface)
            .clipShape(RoundedRectangle(cornerRadius: 14))
            .overlay(RoundedRectangle(cornerRadius: 14).stroke(highlighted ? model.theme.accent : .clear, lineWidth: tile.targeted ? 2 : 1))
            .contentShape(RoundedRectangle(cornerRadius: 14))
        }
        .buttonStyle(.plain)
        .onHover { tile.hovered = $0 }
        .onDrop(of: [UTType.fileURL.identifier], isTargeted: $tile.targeted) { model.drop($0, on: peer) }
        .help("Click or drop a file or directory to send to \(peer.name)\n\(peer.transferFeatures)")
        .accessibilityLabel("\(peer.name), \(peer.platform), \(peer.address)")
        .accessibilityHint("Choose a file to send, or drop a file onto this computer")
    }
}
struct MainView: View {
    @ObservedObject var model: ClientModel
    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 30, height: 30)
                Text("BitSkim").font(.system(size: 17, weight: .semibold, design: .rounded))
                Text("0.2").font(.system(size: 9, design: .monospaced)).foregroundStyle(model.theme.muted)
                Spacer()
                if model.keepInMenuBar {
                    Button { model.hideWindow?() } label: { Image(systemName: "menubar.arrow.up.rectangle") }
                        .help("Hide to menu bar").accessibilityLabel("Hide to menu bar")
                }
                Button { model.tools = true } label: { Image(systemName: "info.circle") }.help("Transfer tools, history and device pairing")
                Button { model.openPreferences?() } label: { Image(systemName: "slider.horizontal.3") }
                    .help("Preferences").accessibilityLabel("Preferences")
            }.buttonStyle(.plain).padding(.horizontal, 16).padding(.vertical, 12)
            Rectangle().fill(model.theme.muted.opacity(0.16)).frame(height: 1)
            HStack {
                Text("NEARBY").font(.system(size: 9, weight: .medium, design: .monospaced)).foregroundStyle(model.theme.muted)
                Spacer()
                Button { model.refresh() } label: { Image(systemName: "arrow.clockwise") }.help("Refresh computers").accessibilityLabel("Refresh")
                Button("Connect by IP") { model.manual = true }.font(.system(size: 11))
            }.buttonStyle(.plain).padding(.horizontal, 16).padding(.top, 13).padding(.bottom, 8)
            ScrollView {
                if model.peers.isEmpty {
                    VStack(spacing: 9) {
                        Image(systemName: "dot.radiowaves.left.and.right").font(.system(size: 25, weight: .ultraLight)).foregroundStyle(model.theme.accent)
                        Text("Waiting for your other computer").font(.system(size: 12, weight: .medium))
                        Text("Open BitSkim on the same LAN, or connect by IP.").font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.frame(maxWidth: .infinity).padding(.vertical, 32)
                } else {
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 160, maximum: 190), spacing: 12)], alignment: .leading, spacing: 12) {
                        ForEach(model.peers) { PeerTile(peer: $0, model: model) }
                    }
                    .padding(.horizontal, 16).padding(.top, 4).padding(.bottom, 16)
                }
            }.frame(minHeight: 130)
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 6) {
                    Circle().fill(model.startupError.isEmpty ? model.theme.accent : .orange).frame(width: 5, height: 5)
                    Text(model.status).font(.system(size: 10)).lineLimit(2)
                    Spacer(minLength: 0)
                    if model.active || model.scanning { Button("Cancel") { model.cancel() }.font(.system(size: 10)) }
                }
                if model.active {
                    Text((model.filePosition.isEmpty ? "" : model.filePosition + " · ") + model.filename).font(.system(size: 9)).foregroundStyle(model.theme.muted).lineLimit(1)
                    Text(model.transferAmounts).font(.system(size: 10, design: .monospaced))
                        .foregroundStyle(model.theme.muted).lineLimit(2)
                    HStack(spacing: 12) {
                        ProgressView(value: model.fraction).controlSize(.mini)
                        Text(model.transferSpeed).font(.system(size: 10, design: .monospaced))
                            .foregroundStyle(model.theme.accent).frame(minWidth: 76, alignment: .trailing)
                    }
                } else {
                    Text(model.receiving ? (model.visible ? "Visible as \(model.wireName)" : "Hidden · manual reception enabled") : "Incoming transfers off")
                        .font(.system(size: 9)).foregroundStyle(model.theme.muted).lineLimit(1)
                }
                Text(model.progressSummary).font(.system(size: 10)).foregroundStyle(model.theme.muted).lineLimit(2)
                Text(model.active ? (model.encrypted ? "LOCAL NETWORK · TLS ENCRYPTED" : (model.transportMode == 2 ? "LOCAL NETWORK · TLS CONNECTING" : "LOCAL NETWORK · UNENCRYPTED")) : (model.transportMode == 2 ? "LOCAL NETWORK · ENCRYPTION REQUIRED" : model.transportMode == 1 ? "LOCAL NETWORK · PREFER ENCRYPTION" : "LOCAL NETWORK · ENCRYPTION DISABLED")).font(.system(size: 8, design: .monospaced)).foregroundStyle(model.theme.muted)
            }.frame(maxWidth: .infinity, alignment: .leading).padding(13).background(model.theme.surface)
        }
        .foregroundStyle(model.theme.text).background(model.theme.base)
        .tint(model.theme.accent).accentColor(model.theme.accent).preferredColorScheme(model.theme.dark ? .dark : .light)
        .sheet(isPresented: $model.tools) { TransferToolsView(model: model) }
        .sheet(isPresented: $model.manual) {
            VStack(alignment: .leading, spacing: 14) {
                Text("Connect by IP").font(.system(size: 16, weight: .semibold))
                Text("BitSkim must be running on the other computer.").font(.system(size: 11)).foregroundStyle(model.theme.muted)
                TextField("IPv4 address", text: $model.manualIP)
                HStack { Text("Port").font(.system(size: 11)); TextField("Port", text: $model.manualPort).frame(width: 80) }
                HStack { Spacer(); Button("Cancel") { model.manual = false }; Button("Choose File…") { model.manual = false; model.sendManual() }.keyboardShortcut(.defaultAction) }
            }.padding(20).frame(width: 320).foregroundStyle(model.theme.text).background(model.theme.base)
                .preferredColorScheme(model.theme.dark ? .dark : .light).tint(model.theme.accent)
        }
    }
}
struct PreferencesView: View {
    @ObservedObject var model: ClientModel
    var close: () -> Void
    var body: some View {
        VStack(spacing: 0) {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Text("Preferences").font(.system(size: 20, weight: .semibold, design: .rounded))
                GroupBox {
                    VStack(alignment: .leading, spacing: 11) {
                        Picker("Theme", selection: $model.themeID) {
                            ForEach(AppTheme.all) { Text($0.name).tag($0.id) }
                        }.onChange(of: model.themeID) { _ in model.savePreferences() }
                        HStack(spacing: 5) {
                            ForEach(AppTheme.all) { theme in
                                Button { model.applyTheme(theme.id) } label: {
                                    Circle().fill(theme.accent).frame(width: 18, height: 18).padding(3)
                                        .background(Circle().fill(theme.base))
                                        .overlay(Circle().stroke(model.themeID == theme.id ? model.theme.text : .clear, lineWidth: 1))
                                }.buttonStyle(.plain).help(theme.name).accessibilityLabel(theme.name)
                            }
                        }
                    }.padding(6)
                } label: { Text("Appearance").font(.system(size: 11, weight: .semibold)) }
                GroupBox {
                    VStack(alignment: .leading, spacing: 9) {
                        Toggle("Keep running in the menu bar", isOn: $model.keepInMenuBar)
                            .onChange(of: model.keepInMenuBar) { value in if !value { model.startInMenuBar = false }; model.savePreferences() }
                        Toggle("Start minimized to the menu bar", isOn: $model.startInMenuBar)
                            .onChange(of: model.startInMenuBar) { _ in model.savePreferences() }
                        Text("Closing the window keeps discovery and reception active when menu-bar mode is enabled. This does not add a login item.")
                            .font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.padding(6)
                } label: { Text("Menu bar & startup").font(.system(size: 11, weight: .semibold)) }
                GroupBox {
                    VStack(alignment: .leading, spacing: 9) {
                        TextField("Client name", text: $model.name)
                        Toggle("Visible to other computers", isOn: $model.visible)
                        Toggle("Allow incoming transfers", isOn: $model.receiving)
                        Toggle("Automatically accept file offers", isOn: $model.autoAccept)
                        Toggle("Auto-accept only verified devices", isOn: $model.trustedAutoAcceptOnly)
                        Toggle("Play completion sound", isOn: $model.completionSound)
                        Toggle("Always ask where to save", isOn: $model.askLocation)
                        HStack { Text(model.folder).font(.system(size: 10)).lineLimit(2); Spacer(); Button("Folder…") { model.chooseFolder() } }
                        Text("Unknown devices ask for approval when verified-only is on. Auto-accept still asks for a location when “Always ask” is on.")
                            .font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.padding(6)
                } label: { Text("This computer & incoming files").font(.system(size: 11, weight: .semibold)) }
                GroupBox {
                    VStack(alignment: .leading, spacing: 9) {
                        HStack { Text("Send limit (KiB/s)"); Spacer(); TextField("0", text: $model.sendLimitKiB).frame(width: 100) }
                        HStack { Text("Receive limit (KiB/s)"); Spacer(); TextField("0", text: $model.receiveLimitKiB).frame(width: 100) }
                        Text("0 = Unlimited. Limits apply to file contents, starting with the next file. Receiving with a cap requires an updated sender.").font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.padding(6)
                } label: { Text("Network").font(.system(size: 11, weight: .semibold)) }
                GroupBox {
                    VStack(alignment: .leading, spacing: 9) {
                        Picker("Encryption", selection: $model.securityMode) {
                            Text("Disable encryption").tag(0)
                            Text("Prefer encryption").tag(1)
                            Text("Force encryption").tag(2)
                        }
                        SecureField("Optional 64-character pairing key", text: $model.pairingKey)
                        HStack {
                            Button("Generate key") { model.generateKey() }
                            Button("Copy key") { NSPasteboard.general.clearContents(); NSPasteboard.general.setString(model.pairingKey, forType: .string) }
                        }
                        Text("Prefer encrypts automatically when both clients allow it; otherwise it uses plaintext. Force rejects plaintext. Without a pairing key, encryption does not authenticate the peer. An optional matching key authenticates paired computers. Discovery remains public.")
                            .font(.system(size: 10)).foregroundStyle(model.theme.muted)
                        Toggle("Retain interrupted downloads for resume", isOn: $model.resumable)
                        Text("Resume works with capable clients. Explicit cancellation removes the current checkpoint. Updated peers can accept a displayed batch once; older peers ask per file. Batch transfers restart interrupted files from the beginning.").font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.padding(6).disabled(model.active || model.queued > 0)
                } label: { Text("Transfers & encryption").font(.system(size: 11, weight: .semibold)) }
                GroupBox {
                    VStack(alignment: .leading, spacing: 9) {
                        Link("github.com/retrocs-org/bitskim", destination: URL(string: "https://github.com/retrocs-org/bitskim")!)
                            .accessibilityLabel("BitSkim source repository on GitHub")
                        Link("bitskim.retrocs.org", destination: URL(string: "https://bitskim.retrocs.org")!)
                            .accessibilityLabel("BitSkim website")
                        Text("Direct local transfers. No account required.")
                            .font(.system(size: 10)).foregroundStyle(model.theme.muted)
                    }.padding(6).frame(maxWidth: .infinity, alignment: .leading)
                } label: { Text("About BitSkim 0.2").font(.system(size: 11, weight: .semibold)) }
            }.padding(18).font(.system(size: 11))
        }
        Divider()
        HStack {
            Text("BitSkim 0.2").font(.system(size: 9)).foregroundStyle(model.theme.muted)
            Spacer()
            Button("Save Preferences") { model.savePreferences() }.keyboardShortcut("s", modifiers: .command)
            Button("Close") { model.savePreferences(); close() }
                .keyboardShortcut("w", modifiers: .command)
        }.padding(12).font(.system(size: 11))
        }.onExitCommand { model.savePreferences(); close() }
        .foregroundStyle(model.theme.text).background(model.theme.base).tint(model.theme.accent)
            .preferredColorScheme(model.theme.dark ? .dark : .light)
    }
}

struct TransferToolsView: View {
    @ObservedObject var model: ClientModel
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack { Text("Transfer tools").font(.title2); Spacer(); Button("Close") { model.tools = false }.keyboardShortcut(.cancelAction) }
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Picker("Existing files", selection: $model.conflictPolicy) {
                        Text("Rename safely").tag(0); Text("Skip").tag(1); Text("Replace").tag(2)
                    }.onChange(of: model.conflictPolicy) { value in if let engine = model.engine { bs_conflict_policy(engine, Int32(value)) } }
                    Text("Applies to remaining files in an approved collection. Rename is the default. Replace is an explicit choice for this session.").font(.caption)
                    Divider()
                    Text("Device pairing").font(.headline)
                    if !model.verificationCode.isEmpty {
                        Text("This connection: " + model.verificationCode).font(.system(.title3, design: .monospaced)).textSelection(.enabled)
                        Text(model.trustedDevice ? "Saved device identity matches." : "Compare the entire code with the other computer over a trusted channel. Enter the code shown there, then trust on both computers.")
                        TextField("Code from the other screen (XXXX-XXXX-XXXX)", text: $model.otherPairingCode)
                        Button("Trust device") { model.trustDevice() }
                    } else { Text("Pair during an encrypted connection. Shared-key connections use the existing key instead. Retro clients without TLS cannot pair.") }
                    Button("Forget current or last device") { model.forgetDevice() }
                    Divider(); Text("Network status").font(.headline)
                    HStack { TextField("IPv4 address or IPv4:port", text: $model.testAddress); Button("Test connection") { model.testConnection() } }
                    Text(model.diagnostics()).font(.system(.caption, design: .monospaced)).textSelection(.enabled)
                    Divider(); Text("Recent transfers (this session)").font(.headline)
                    if model.historyCount == 0 { Text("No transfers yet.") }
                    ForEach(0..<model.historyCount, id: \.self) { index in
                        let serial=model.historySerial(index)
                        VStack(alignment: .leading) {
                            Text(model.historyText(index)).font(.system(.caption, design: .monospaced)).textSelection(.enabled)
                            HStack { Button("Retry outgoing") { model.retryHistory(serial) }; Button("Show in Finder") { model.revealHistory(serial) } }
                        }.padding(10).background(model.theme.surface).cornerRadius(8)
                    }
                }.frame(maxWidth: .infinity, alignment: .leading)
            }
        }.padding(20).frame(width: 600, height: 600).foregroundStyle(model.theme.text).background(model.theme.base)
    }
}
