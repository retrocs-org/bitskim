import Foundation

struct SendFile: Identifiable {
    var id: URL { url }
    let url: URL
    let bytes: UInt64
}
struct SendReview: Identifiable {
    let id = UUID()
    let recipient: String
    let endpoint: String
    let files: [SendFile]
    var total: UInt64 { files.reduce(0) { $0 + $1.bytes } }
    static func make(recipient: String, urls: [URL], endpoint: String = "") throws -> SendReview {
        guard !urls.isEmpty else { throw CocoaError(.fileNoSuchFile) }
        let files = try urls.map { url -> SendFile in
            var freshURL = url; freshURL.removeAllCachedResourceValues()
            let values = try freshURL.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey])
            guard values.isRegularFile == true, values.isSymbolicLink != true,
                  let size = values.fileSize, size >= 0, size <= 2_147_483_647 else {
                throw CocoaError(.fileReadUnsupportedScheme, userInfo: [NSLocalizedDescriptionKey: "Choose regular files no larger than 2 GiB minus one byte."])
            }
            return SendFile(url: url, bytes: UInt64(size))
        }
        return SendReview(recipient: recipient, endpoint: endpoint, files: files)
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
