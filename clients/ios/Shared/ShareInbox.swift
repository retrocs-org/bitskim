import Foundation

// Each share is committed by an atomic directory rename after its file is complete.
enum ShareInbox {
    static var group: String { Bundle.main.object(forInfoDictionaryKey: "BitSkimAppGroup") as? String ?? "" }
    static func root() throws -> URL {
        guard !group.isEmpty, let container = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: group) else {
            throw CocoaError(.fileNoSuchFile, userInfo: [NSLocalizedDescriptionKey: "Shared inbox is unavailable. Check the App Group signing capability."])
        }
        let root = container.appendingPathComponent("Inbox", isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        return root
    }
    @discardableResult static func stage(_ source: URL, inbox: URL? = nil) throws -> URL {
        let root = try inbox ?? root(), id = UUID().uuidString
        let temporary = root.appendingPathComponent(".pending-" + id, isDirectory: true)
        let final = root.appendingPathComponent(id, isDirectory: true)
        try FileManager.default.createDirectory(at: temporary, withIntermediateDirectories: false)
        do {
            var coordinationError: NSError?, copyError: Error?
            NSFileCoordinator().coordinate(readingItemAt: source, options: [], error: &coordinationError) { readable in
                do {
                    let values = try readable.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey])
                    guard values.isRegularFile == true, values.isSymbolicLink != true, (values.fileSize ?? Int.max) <= 2_147_483_647 else {
                        throw CocoaError(.fileReadUnsupportedScheme, userInfo: [NSLocalizedDescriptionKey: "Choose regular files no larger than 2 GiB minus one byte."])
                    }
                    try FileManager.default.copyItem(at: readable, to: temporary.appendingPathComponent(source.lastPathComponent))
                } catch { copyError = error }
            }
            if let coordinationError { throw coordinationError }; if let copyError { throw copyError }
            try FileManager.default.moveItem(at: temporary, to: final)
            return final.appendingPathComponent(source.lastPathComponent)
        } catch { try? FileManager.default.removeItem(at: temporary); throw error }
    }
    static func pending(inbox: URL? = nil) throws -> [URL] {
        let root = try inbox ?? root()
        return try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]).sorted { $0.lastPathComponent < $1.lastPathComponent }.flatMap {
            try FileManager.default.contentsOfDirectory(at: $0, includingPropertiesForKeys: nil, options: [])
        }
    }
}
