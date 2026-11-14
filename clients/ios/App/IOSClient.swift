import SwiftUI
import UIKit
import Network
import Darwin

func wireText<T>(_ value: T) -> String {
    var value = value
    return withUnsafeBytes(of: &value) { bytes in
        String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self)
    }
}
struct MobileOffer: Identifiable {
    let id: UInt, name: String, peer: String, size: UInt, count: Int
}
struct MobileHistory: Identifiable, Equatable {
    let id: UInt, name: String, message: String, integrity: String
}

@MainActor final class IOSClient: ObservableObject {
    @Published var peers: [MobilePeer] = []
    @Published var addresses: [String] = []
    @Published var status = "Open BitSkim to receive files."
    @Published var active = false
    @Published var transferred: UInt = 0
    @Published var total: UInt = 0
    @Published var offer: MobileOffer?
    @Published var code = ""
    @Published var trusted = false
    @Published var otherCode = ""
    @Published var diagnostics = ""
    @Published var history: [MobileHistory] = []
    @Published var error: String?
    @Published var importing = false
    @Published var filePicker = false
    @Published var sharedFiles: [URL] = []
    @Published var review: SendReview?
    @Published var progress = MobileProgress()
    private var reviewLease = false
    private var reviewCopies: [URL] = []
    private var sendPlan: [SendFile] = []
    @Published var security = UserDefaults.standard.object(forKey: "security") as? Int ?? 1
    private var fileBrowserRequests = 0
    private var importBrowserOpen = false
    private var browseBeforeRecipient = false
    private var generation: UInt = 0
    private var engine: OpaquePointer?
    private var timer: Timer?
    private var displayedOffer: UInt = 0
    private var lastDisplay = Date.distantPast
    private var lastAddressUpdate = Date.distantPast
    private var sending: [URL] = []
    private var destination: (peer: String?, address: String, port: UInt16, name: String)?
    private var platformReady = false
    let received: URL
    let listeningPort: UInt16
    let discovery = (Bundle.main.object(forInfoDictionaryKey: "BitSkimBroadcastDiscovery") as? String) == "YES"
    private let support: URL
    private let imports: URL
    private let defaults = UserDefaults.standard

    init(listeningPort: UInt16? = nil) {
        self.listeningPort = listeningPort ?? {
            guard let value = Bundle.main.object(forInfoDictionaryKey: "BitSkimListeningPort") as? String, let port = UInt16(value), port > 0 else { return UInt16(BS_PORT) }
            return port
        }()
        let fm = FileManager.default
        received = fm.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Received", isDirectory: true)
        support = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        imports = fm.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("Outgoing", isDirectory: true)
    }
    func start() {
        guard engine == nil else { return }
        do {
            let fm = FileManager.default, trust = support.appendingPathComponent("Trust", isDirectory: true)
            for folder in [received, support, imports, trust] {
                try fm.createDirectory(at: folder, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700, .protectionKey: FileProtectionType.completeUntilFirstUserAuthentication])
            }
            var privateURL = trust, values = URLResourceValues(); values.isExcludedFromBackup = true
            try privateURL.setResourceValues(values)
            let id = defaults.string(forKey: "installationID") ?? UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
            defaults.set(id, forKey: "installationID")
            platformReady = bs_platform_init() != 0
            var config = BSConfig()
            func store<T>(_ string: String, in field: inout T) {
                withUnsafeMutableBytes(of: &field) { buffer in
                    let bytes = Array(string.utf8); precondition(bytes.count < buffer.count)
                    for index in buffer.indices { buffer[index] = 0 }
                    for (index, byte) in bytes.enumerated() { buffer[index] = byte }
                }
            }
            store(UIDevice.current.model, in: &config.name); store("iOS", in: &config.platform); store(id, in: &config.id)
            config.port = listeningPort; config.discovery_port = UInt16(BS_PORT); config.visible = discovery ? 1 : 0; config.receiving = fileBrowserRequests == 0 ? 1 : 0
            guard platformReady, let client = bs_create(&config, nil, nil) else {
                throw messageError("Cannot start the local listener. Check Local Network permission and whether another BitSkim copy is open.")
            }
            engine = client
            guard bs_trust_configure(client, trust.path) != 0, bs_security_configure(client, Int32(security), "") != 0 else {
                throw messageError("Cannot initialize saved device identities and encryption.")
            }
            bs_conflict_policy(client, BS_CONFLICT_RENAME)
            bs_batch_enable(client, 1)
            bs_resume_enable(client, 0)
            timer = Timer.scheduledTimer(withTimeInterval: 0.02, repeats: true) { [weak self] _ in
                MainActor.assumeIsolated { self?.tick() }
            }
            refreshSharedFiles()
            tick()
        } catch {
            self.error = error.localizedDescription
            stop()
        }
    }
    func stop() {
        closeReview()
        generation &+= 1
        timer?.invalidate(); timer = nil
        if let client = engine { bs_cancel(client); updateHistory(client); bs_destroy(client) }
        engine = nil
        if platformReady { bs_platform_cleanup(); platformReady = false }
        cleanupSending(); offer = nil; filePicker = false; displayedOffer = 0; active = false; peers = []; code = ""; trusted = false
        status = "Receiving paused while BitSkim is in the background. Open the app to reconnect."
    }
    private func tick() {
        guard let client = engine else { return }
        bs_service(client, 4)
        let state = bs_status(client)!.pointee, serial = bs_transfer_serial(client)
        if offer != nil && (state.awaiting_decision == 0 || serial != displayedOffer) { offer = nil }
        if state.awaiting_decision != 0 && serial != displayedOffer {
            displayedOffer = serial; filePicker = false
            let next = MobileOffer(id: serial, name: wireText(state.filename), peer: wireText(state.peer_name), size: state.total, count: Int(bs_batch_count(client)))
            // Let a document picker finish dismissing before presenting approval.
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) { [weak self] in
                guard let self, self.matches(serial) else { return }; self.offer = next
            }
        }
        guard Date().timeIntervalSince(lastDisplay) >= 0.2 else { return }
        lastDisplay = Date()
        if Date().timeIntervalSince(lastAddressUpdate) >= 5 {
            let nextAddresses = Self.lanAddresses(); if addresses != nextAddresses { addresses = nextAddresses }
            lastAddressUpdate = Date()
        }
        if active != (state.active != 0) { active = state.active != 0 }
        let message = wireText(state.message); if status != message { status = message }
        if transferred != state.transferred { transferred = state.transferred }; if total != state.total { total = state.total }
        let next = (0..<Int(bs_peer_count(client))).compactMap { i -> MobilePeer? in
            guard let p = bs_peer(client, Int32(i))?.pointee else { return nil }
            return MobilePeer(deviceID: wireText(p.id), name: wireText(p.name), platform: wireText(p.platform), address: wireText(p.address), port: p.port)
        }
        if peers != next { peers = next }
        var buffer = [CChar](repeating: 0, count: 8192)
        _ = bs_diagnostics(client, &buffer, buffer.count); let report = String(cString: buffer)
        if diagnostics != report { diagnostics = report }
        var pairing = [CChar](repeating: 0, count: 20)
        let nextCode = bs_pairing_code(client, &pairing) != 0 ? String(cString: pairing) : ""
        if code != nextCode { code = nextCode }
        let nextTrust = bs_trust_status(client) == 1; if trusted != nextTrust { trusted = nextTrust }
        updateProgress(client, state: state)
        updateHistory(client)
        if !active && bs_queue_count(client) == 0 { cleanupSending() }
    }
    private func updateProgress(_ client: OpaquePointer, state: BSStatus) {
        var next = MobileProgress()
        next.filename = wireText(state.filename)
        next.queued = Int(bs_queue_count(client)); next.paused = bs_queue_paused(client) != 0
        next.rate = state.active != 0 ? UInt64(bs_transfer_rate(client)) : 0
        var report = [CChar](repeating: 0, count: 256)
        _ = bs_progress_summary(client, &report, report.count)
        next.summary = String(cString: report)
        var position: UInt = 0, count: UInt = 0
        bs_queue_progress(client, &position, &count)
        if state.incoming == 0, !sendPlan.isEmpty, Int(count) == sendPlan.count, position > 0 {
            let completed = min(Int(position - 1), sendPlan.count)
            next.position = "File \(position) of \(count)"
            next.overallTotal = sendPlan.reduce(0) { $0 + $1.bytes }
            next.overallDone = min(next.overallTotal, sendPlan.prefix(completed).reduce(0) { $0 + $1.bytes } + UInt64(state.transferred))
            next.remainingSeconds = state.active != 0 ? MobileProgress.eta(remaining: next.overallTotal - next.overallDone, rate: next.rate) : nil
        } else {
            let count = bs_batch_count(client)
            if count > 0 { next.position = "Item \(bs_batch_index(client) + 1) of \(count)" }
            next.remainingSeconds = state.active != 0 && state.awaiting_decision == 0 ? MobileProgress.eta(remaining: UInt64(state.total - min(state.total, state.transferred)), rate: next.rate) : nil
        }
        if progress != next { progress = next }
    }
    private func updateHistory(_ client: OpaquePointer) {
        let next: [MobileHistory] = (0..<Int(bs_history_count(client))).compactMap { i in
            var item = BSHistoryItem(); guard bs_history_item(client, Int32(i), &item) != 0 else { return nil }
            let integrity = item.result != BS_RESULT_COMPLETE.rawValue ? "Incomplete — not verified" : (item.verified != 0 ? "Written file verified (CRC32)" : "Receiver verification not confirmed")
            return MobileHistory(id: item.serial, name: wireText(item.filename), message: wireText(item.message), integrity: integrity)
        }
        if history != next { history = next }
    }
    private func matches(_ serial: UInt) -> Bool {
        guard let client = engine else { return false }
        return bs_transfer_serial(client) == serial && bs_status(client)!.pointee.awaiting_decision != 0
    }
    func manifestName(_ index: Int, serial: UInt) -> String {
        guard matches(serial), let client = engine, let item = bs_batch_item(client, Int32(index))?.pointee else { return "Offer expired" }
        return wireText(item.filename)
    }
    func accept(_ incoming: MobileOffer) {
        guard matches(incoming.id), let client = engine else { return }
        let success: Int32
        if incoming.count > 0 { success = bs_accept_batch(client, received.path) }
        else {
            var path = [CChar](repeating: 0, count: Int(BS_PATH_MAX) + 1)
            guard bs_suggest_destination(received.path, incoming.name, 0, &path, path.count) != 0 else { error = "Cannot choose a safe destination."; return }
            success = bs_accept(client, String(cString: path))
        }
        if success == 0 { error = String(cString: bs_last_error(client)); return }
        offer = nil
    }
    func decline(_ incoming: MobileOffer) {
        if matches(incoming.id), let client = engine { bs_decline(client) }; offer = nil
    }
    func trust() {
        guard let client = engine, !code.isEmpty else { return }
        if bs_trust_current(client, otherCode.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()) == 0 { error = String(cString: bs_last_error(client)) }
        else { trusted = true; otherCode = ""; error = nil }
    }
    func cancel() { if let client = engine { bs_cancel(client) }; offer = nil; tick() }
    func retry() { if let client = engine { bs_queue_continue(client) } }
    func refresh() { if let client = engine { bs_refresh(client) }; refreshSharedFiles() }
    func setSecurity(_ value: Int) {
        guard let client = engine, !active, bs_queue_count(client) == 0 else { return }
        if bs_security_configure(client, Int32(value), "") != 0 { security = value; defaults.set(value, forKey: "security") }
    }
    func prepare(peer: MobilePeer) {
        guard review == nil, !active, !importing else { return }
        browseBeforeRecipient = false
        destination = (peer.deviceID, peer.address, peer.port, peer.name); chooseFiles()
    }
    func prepare(address: String) {
        guard let target = ConnectionTarget(address) else { error = "Enter an IPv4 address, optionally followed by :port."; return }
        guard review == nil, !active, !importing else { return }
        browseBeforeRecipient = false
        destination = (nil, target.address, target.port, ConnectionTarget.endpoint(target.address, port: target.port)); chooseFiles()
    }
    private func chooseFiles() {
        guard engine != nil, !active, !importing else { return }
        if sharedFiles.isEmpty {
            guard beginFileBrowser() else { return }
            importBrowserOpen = true; filePicker = true
        } else { presentReview(sharedFiles) }
    }
    func browseFiles() {
        guard !importing, beginFileBrowser() else { return }
        browseBeforeRecipient = true; destination = nil
        importBrowserOpen = true; filePicker = true
    }
    func beginFileBrowser() -> Bool {
        guard let client = engine, bs_status(client)!.pointee.active == 0 else {
            error = "Finish or cancel the current transfer before opening Files."; return false
        }
        fileBrowserRequests += 1
        bs_configure(client, UIDevice.current.model, discovery ? 1 : 0, 0)
        return true
    }
    func endFileBrowser() {
        fileBrowserRequests = max(0, fileBrowserRequests - 1)
        if let client = engine { bs_configure(client, UIDevice.current.model, discovery ? 1 : 0, fileBrowserRequests == 0 ? 1 : 0) }
    }
    func endImportBrowser() {
        guard importBrowserOpen else { return }; importBrowserOpen = false; endFileBrowser()
    }
    func imported(_ result: Result<[URL], Error>) {
        endImportBrowser()
        switch result {
        case .failure(let failure):
            if (failure as NSError).code != NSUserCancelledError { error = failure.localizedDescription }
        case .success(let urls):
            guard !urls.isEmpty, urls.count <= 32 else { error = "Choose between 1 and 32 files at a time."; return }
            importing = true
            let holdForClient = browseBeforeRecipient
            let root: URL
            do { root = try ShareInbox.root() }
            catch { importing = false; self.error = error.localizedDescription; return }
            let currentGeneration = generation
            Task {
                do {
                    let staged = try await Task.detached(priority: .userInitiated) {
                        var copies: [URL] = []
                        do {
                            for url in urls {
                                let access = url.startAccessingSecurityScopedResource()
                                defer { if access { url.stopAccessingSecurityScopedResource() } }
                                copies.append(try ShareInbox.stage(url, inbox: root))
                            }
                            return copies
                        } catch { for copy in copies { try? FileManager.default.removeItem(at: copy.deletingLastPathComponent()) }; throw error }
                    }.value
                    importing = false
                    guard generation == currentGeneration else { removeOwned(staged); return }
                    refreshSharedFiles()
                    if !holdForClient {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                            guard self.generation == currentGeneration else { return }
                            self.presentReview(staged)
                        }
                    }
                } catch { importing = false; self.error = error.localizedDescription }
            }
        }
    }
    private func presentReview(_ files: [URL]) {
        guard let destination, review == nil, let client = engine else { return }
        guard bs_queue_count(client) == 0 else {
            error = "Retry or cancel the queued transfer before starting another send."; return
        }
        do {
            let manifest = try SendReview.make(recipient: destination.name, urls: files, endpoint: ConnectionTarget.endpoint(destination.address, port: destination.port))
            guard beginFileBrowser() else { return }
            reviewLease = true; reviewCopies = files; review = manifest
        } catch { self.error = error.localizedDescription }
    }
    func closeReview() {
        guard reviewLease else { return }
        review = nil; reviewLease = false
        let temporary = reviewCopies.filter { $0.standardizedFileURL.path.hasPrefix(imports.standardizedFileURL.path + "/") }
        removeOwned(temporary); reviewCopies = []
        endFileBrowser()
    }
    func confirmSend(_ manifest: SendReview) {
        guard review?.id == manifest.id, let client = engine, bs_status(client)!.pointee.active == 0 else { return }
        do {
            let fresh = try SendReview.make(recipient: manifest.recipient, urls: manifest.files.map(\.url), endpoint: manifest.endpoint)
            guard fresh.files.map(\.bytes) == manifest.files.map(\.bytes) else { throw messageError("The selected files changed. Review them again before sending.") }
            sendPlan = fresh.files
            reviewCopies = []; closeReview()
            enqueue(fresh.files.map(\.url))
        } catch { self.error = error.localizedDescription; closeReview() }
    }
    private func enqueue(_ files: [URL]) {
        guard let client = engine, let destination else { removeOwned(files); return }
        for (index, file) in files.enumerated() {
            let ok = destination.peer.map { bs_queue_peer_address(client, $0, destination.address, destination.port, file.path) } ?? bs_queue_address(client, destination.address, destination.port, file.path)
            if ok == 0 {
                error = String(cString: bs_last_error(client))
                let unsentImports = files[index...].filter { $0.standardizedFileURL.path.hasPrefix(imports.standardizedFileURL.path + "/") }
                removeOwned(Array(unsentImports)); break
            }
            sending.append(file)
        }
        sharedFiles = ((try? ShareInbox.pending()) ?? []).filter { !sending.contains($0) }
        tick()
    }
    func refreshSharedFiles() { sharedFiles = ((try? ShareInbox.pending()) ?? []).filter { !sending.contains($0) } }
    private func cleanupSending() { removeOwned(sending); sending = [] }
    private func removeOwned(_ files: [URL]) {
        let roots = [imports, try? ShareInbox.root()].compactMap { $0 }.map { $0.standardizedFileURL.path + "/" }
        for file in files where roots.contains(where: { file.standardizedFileURL.path.hasPrefix($0) }) {
            try? FileManager.default.removeItem(at: file.deletingLastPathComponent())
        }
    }
    private static func lanAddresses() -> [String] {
        var first: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&first) == 0, let first else { return [] }
        defer { freeifaddrs(first) }
        var result: Set<String> = [], cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let item = cursor {
            defer { cursor = item.pointee.ifa_next }
            let name = String(cString: item.pointee.ifa_name)
            guard name.hasPrefix("en") || name.hasPrefix("bridge"), let address = item.pointee.ifa_addr,
                  address.pointee.sa_family == UInt8(AF_INET), (item.pointee.ifa_flags & UInt32(IFF_UP)) != 0 else { continue }
            var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            if getnameinfo(address, socklen_t(address.pointee.sa_len), &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 {
                result.insert(String(cString: host))
            }
        }
        return result.sorted()
    }
    func spaceReport(_ incoming: MobileOffer) -> String {
        guard matches(incoming.id), let engine else { return "Offer expired" }
        var buffer = [CChar](repeating: 0, count: 512)
        _ = bs_destination_space_report(engine, received.path, incoming.count > 0 ? 1 : 0, &buffer, buffer.count)
        return String(cString: buffer)
    }
    private func messageError(_ message: String) -> NSError { NSError(domain: "BitSkim", code: 1, userInfo: [NSLocalizedDescriptionKey: message]) }
}
