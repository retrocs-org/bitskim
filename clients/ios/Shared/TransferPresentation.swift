import Foundation

struct SendFile: Identifiable {
    var id: URL { url }
    let url: URL
    let bytes: UInt64
    let snapshot: SelectionSnapshot
    var isDirectory: Bool { snapshot.entries.first?.isDirectory == true }
}
struct SendReview: Identifiable {
    let id = UUID()
    let recipient: String
    let endpoint: String
    let files: [SendFile]
    let entries: [SendEntry]
    var total: UInt64 { files.reduce(0) { $0 + $1.bytes } }
    static func make(recipient: String, urls: [URL], endpoint: String = "") throws -> SendReview {
        guard !urls.isEmpty, urls.count <= 32, Set(urls.map(\.standardizedFileURL)).count == urls.count else { throw CocoaError(.fileNoSuchFile) }
        var files: [SendFile] = [], entries: [SendEntry] = []
        for url in urls {
            let snapshot = try SelectionSnapshot.inspect(url)
            guard entries.count + snapshot.entries.count <= SelectionSnapshot.maximumEntries else {
                throw NSError(domain: "BitSkim", code: 4, userInfo: [NSLocalizedDescriptionKey: "Choose no more than 32,760 files and folder entries."])
            }
            files.append(SendFile(url: url, bytes: snapshot.total, snapshot: snapshot))
            entries.append(contentsOf: snapshot.entries)
        }
        return SendReview(recipient: recipient, endpoint: endpoint, files: files, entries: entries)
    }
}
struct MobileProgress: Equatable {
    var filename = ""
    var position = ""
    var rate: UInt64 = 0
    var queued = 0
    var paused = false
    var summary = ""
    var overallDone: UInt64 = 0
    var overallTotal: UInt64 = 0
    var remainingSeconds: UInt64?
    static func eta(remaining: UInt64, rate: UInt64) -> UInt64? {
        guard rate > 0, remaining > 0 else { return nil }
        return remaining / rate + (remaining % rate == 0 ? 0 : 1)
    }
    static func size(_ bytes: UInt64) -> String {
        ByteCountFormatter.string(fromByteCount: Int64(clamping: bytes), countStyle: .file)
    }
    var timeRemaining: String {
        guard let seconds = remainingSeconds else { return "Estimating time remaining…" }
        if seconds < 60 { return "About \(seconds)s remaining" }
        if seconds < 3600 { return "About \((seconds + 59) / 60) min remaining" }
        let minutes = (seconds + 59) / 60
        return "About \(minutes / 60)h \(minutes % 60) min remaining"
    }
}
