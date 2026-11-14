import Foundation

struct ReceivedStore {
    let root: URL
    private func checked(_ url: URL) throws -> URL {
        let base = root.standardizedFileURL.resolvingSymlinksInPath()
        let path = url.standardizedFileURL
        let resolved = path.resolvingSymlinksInPath()
        guard resolved.path.hasPrefix(base.path + "/"),
              try path.resourceValues(forKeys: [.isSymbolicLinkKey]).isSymbolicLink != true else {
            throw CocoaError(.fileReadNoPermission)
        }
        return resolved
    }
    func contents(of folder: URL, search: String = "") throws -> [URL] {
        let actual = folder.standardizedFileURL.resolvingSymlinksInPath()
        if actual != root.standardizedFileURL.resolvingSymlinksInPath() { _ = try checked(folder) }
        return try FileManager.default.contentsOfDirectory(at: actual, includingPropertiesForKeys: [.isDirectoryKey, .isSymbolicLinkKey], options: [.skipsHiddenFiles])
            .filter { (try? $0.resourceValues(forKeys: [.isSymbolicLinkKey]).isSymbolicLink) != true }
            .filter { search.isEmpty || $0.lastPathComponent.localizedStandardContains(search) }
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
    }
    @discardableResult func rename(_ url: URL, to name: String) throws -> URL {
        let source = try checked(url)
        guard !name.isEmpty, name != ".", name != "..", !name.hasPrefix("."), name.utf8.count <= 255,
              !name.contains("/"), !name.contains("\\"), !name.unicodeScalars.contains(where: { CharacterSet.controlCharacters.contains($0) }) else {
            throw CocoaError(.fileWriteInvalidFileName)
        }
        let destination = source.deletingLastPathComponent().appendingPathComponent(name)
        if destination == source { return source }
        guard !FileManager.default.fileExists(atPath: destination.path) else { throw CocoaError(.fileWriteFileExists) }
        try FileManager.default.moveItem(at: source, to: destination)
        return destination
    }
    func delete(_ urls: [URL]) throws {
        let paths = try urls.map(checked)
        for path in paths { try FileManager.default.removeItem(at: path) }
    }
}
