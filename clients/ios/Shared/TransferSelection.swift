import Foundation
import CryptoKit

struct SendEntry: Identifiable {
    let url: URL
    let path: String
    let bytes: UInt64
    let isDirectory: Bool
    var id: URL { url }
}
struct SelectionSnapshot {
    static let maximumEntries = 32_760
    let entries: [SendEntry]
    let signature: String
    var total: UInt64 { entries.reduce(0) { $0 + $1.bytes } }
    static func inspect(_ root: URL, allowDirectories: Bool = true, hashContents: Bool = true, rootName: String? = nil) throws -> SelectionSnapshot {
        var entries: [SendEntry] = [], digest = SHA256()
        func fail(_ text: String) -> NSError { NSError(domain: "BitSkim", code: 3, userInfo: [NSLocalizedDescriptionKey: text]) }
        func visit(_ original: URL, relative: String, depth: Int) throws {
            if Task.isCancelled { throw CancellationError() }
            var url = original; url.removeAllCachedResourceValues()
            let name = url.lastPathComponent
            guard !name.isEmpty, name != ".", name != "..", !name.contains("/"), !name.contains("\\"),
                  !name.unicodeScalars.contains(where: { CharacterSet.controlCharacters.contains($0) }),
                  relative.utf8.count <= 255, url.path.utf8.count <= 1023, depth <= 16,
                  entries.count < maximumEntries else { throw fail("The selection exceeds portable filename, depth or item-count limits.") }
            let values = try url.resourceValues(forKeys: [.isRegularFileKey, .isDirectoryKey, .isSymbolicLinkKey, .isAliasFileKey, .fileSizeKey])
            guard values.isSymbolicLink != true, values.isAliasFile != true else { throw fail("Links and aliases cannot be sent. Choose their original files instead.") }
            if values.isDirectory == true {
                guard allowDirectories, depth < 16, relative.utf8.count < 255 else { throw fail("Choose regular files, or use Choose Folder for a supported folder tree.") }
                entries.append(SendEntry(url: url, path: relative + "/", bytes: 0, isDirectory: true))
                digest.update(data: Data(("D\0" + relative + "\0").utf8))
                let children = try FileManager.default.contentsOfDirectory(at: url, includingPropertiesForKeys: nil).sorted { $0.lastPathComponent < $1.lastPathComponent }
                for child in children { try visit(child, relative: relative + "/" + child.lastPathComponent, depth: depth + 1) }
            } else {
                guard values.isRegularFile == true, let size = values.fileSize, size >= 0, size <= 2_147_483_647 else { throw fail("Choose regular files no larger than 2 GiB minus one byte.") }
                digest.update(data: Data(("F\0" + relative + "\0" + String(size) + "\0").utf8))
                if !hashContents { entries.append(SendEntry(url: url, path: relative, bytes: UInt64(size), isDirectory: false)); return }
                let handle = try FileHandle(forReadingFrom: url); defer { try? handle.close() }
                var count: UInt64 = 0
                while let data = try handle.read(upToCount: 65_536), !data.isEmpty {
                    if Task.isCancelled { throw CancellationError() }
                    count += UInt64(data.count)
                    guard count <= 2_147_483_647 else { throw fail("A selected file grew beyond the transfer limit.") }
                    digest.update(data: data)
                }
                guard count == UInt64(size) else { throw fail("The selected files changed. Review them again before sending.") }
                entries.append(SendEntry(url: url, path: relative, bytes: count, isDirectory: false))
            }
        }
        try visit(root, relative: rootName ?? root.lastPathComponent, depth: 1)
        return SelectionSnapshot(entries: entries, signature: digest.finalize().map { String(format: "%02x", $0) }.joined())
    }
}
