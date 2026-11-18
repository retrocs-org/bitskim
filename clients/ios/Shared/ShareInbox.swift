import Foundation

// Each import is committed by an atomic directory rename after its contents are complete.
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
    @discardableResult static func stage(_ source: URL, inbox: URL? = nil, allowDirectories: Bool = false) throws -> URL {
        let root = try inbox ?? root(), id = UUID().uuidString
        let originalPath = source.resolvingSymlinksInPath().standardizedFileURL.path
        let stagingPath = root.resolvingSymlinksInPath().standardizedFileURL.path
        guard stagingPath != originalPath, !stagingPath.hasPrefix(originalPath + "/") else {
            throw CocoaError(.fileReadInvalidFileName, userInfo: [NSLocalizedDescriptionKey: "Choose a folder outside BitSkim’s private staging area."])
        }
        let temporary = root.appendingPathComponent(".pending-" + id, isDirectory: true)
        let final = root.appendingPathComponent(id, isDirectory: true)
        try FileManager.default.createDirectory(at: temporary, withIntermediateDirectories: false)
        do {
            var coordinationError: NSError?, copyError: Error?
            NSFileCoordinator().coordinate(readingItemAt: source, options: [], error: &coordinationError) { readable in
                do {
                    let before = try SelectionSnapshot.inspect(readable, allowDirectories: allowDirectories, hashContents: false, rootName: source.lastPathComponent)
                    let copy = temporary.appendingPathComponent(source.lastPathComponent)
                    try FileManager.default.copyItem(at: readable, to: copy)
                    let after = try SelectionSnapshot.inspect(copy, allowDirectories: allowDirectories, hashContents: false)
                    guard before.signature == after.signature else {
                        throw NSError(domain: "BitSkim", code: 5, userInfo: [NSLocalizedDescriptionKey: "The selected content changed while copying. Choose it again."])
                    }
                } catch { copyError = error }
            }
            if let coordinationError { throw coordinationError }; if let copyError { throw copyError }
            try FileManager.default.moveItem(at: temporary, to: final)
            return final.appendingPathComponent(source.lastPathComponent)
        } catch { try? FileManager.default.removeItem(at: temporary); throw error }
    }
    static func ownedBatch(_ file: URL, inbox: URL) -> URL? {
        let base = inbox.standardizedFileURL.path + "/", path = file.standardizedFileURL.path
        guard path.hasPrefix(base) else { return nil }
        let parts = path.dropFirst(base.count).split(separator: "/")
        guard parts.count >= 2, UUID(uuidString: String(parts[0])) != nil else { return nil }
        return inbox.appendingPathComponent(String(parts[0]), isDirectory: true)
    }
    static func markQueued(_ file: URL, inbox: URL) throws {
        guard let batch = ownedBatch(file, inbox: inbox) else { return }
        let marker = inbox.appendingPathComponent(".queued-" + batch.lastPathComponent)
        try Data([1]).write(to: marker, options: .atomic)
    }
    static func queuedBatches(inbox: URL) -> [URL] {
        ((try? FileManager.default.contentsOfDirectory(at: inbox, includingPropertiesForKeys: nil)) ?? []).compactMap { marker in
            guard marker.lastPathComponent.hasPrefix(".queued-"), UUID(uuidString: String(marker.lastPathComponent.dropFirst(8))) != nil,
                  (try? marker.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey])).map({$0.isRegularFile == true && $0.isSymbolicLink != true}) == true else { return nil }
            return inbox.appendingPathComponent(String(marker.lastPathComponent.dropFirst(8)), isDirectory: true)
        }
    }
    static func removeBatch(_ batch: URL, inbox: URL) {
        guard UUID(uuidString: batch.lastPathComponent) != nil, batch.deletingLastPathComponent().standardizedFileURL == inbox.standardizedFileURL else { return }
        try? FileManager.default.removeItem(at: batch)
        if !FileManager.default.fileExists(atPath: batch.path) { try? FileManager.default.removeItem(at: inbox.appendingPathComponent(".queued-" + batch.lastPathComponent)) }
    }
    static func pending(inbox: URL? = nil) throws -> [URL] {
        let root = try inbox ?? root()
        return try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]).sorted { $0.lastPathComponent < $1.lastPathComponent }.filter {
            !FileManager.default.fileExists(atPath: root.appendingPathComponent(".queued-" + $0.lastPathComponent).path)
        }.flatMap { try FileManager.default.contentsOfDirectory(at: $0, includingPropertiesForKeys: nil, options: []) }
    }
}
