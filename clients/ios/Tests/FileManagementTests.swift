import XCTest
import Foundation

final class FileManagementTests: XCTestCase {
    private var root: URL!
    override func setUpWithError() throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
    }
    override func tearDownWithError() throws { try? FileManager.default.removeItem(at: root) }
    private func file(_ name: String, contents: String = "test") throws -> URL {
        let url = root.appendingPathComponent(name); try Data(contents.utf8).write(to: url); return url
    }
    func testRenamePreservesBytesAndRejectsCollisionAndTraversal() throws {
        let source = try file("old.txt"), existing = try file("existing.txt", contents: "keep")
        let store = ReceivedStore(root: root)
        XCTAssertThrowsError(try store.rename(source, to: "existing.txt"))
        for name in ["../escape", "nested/file", "\\escape", ".hidden", "", ".", ".."] {
            XCTAssertThrowsError(try store.rename(source, to: name))
        }
        let result = try store.rename(source, to: "Möte.txt")
        XCTAssertEqual(try Data(contentsOf: result), Data("test".utf8))
        XCTAssertEqual(try Data(contentsOf: existing), Data("keep".utf8))
        XCTAssertFalse(FileManager.default.fileExists(atPath: source.path))
    }
    func testDeleteIsScopedAndValidatesWholeSelectionBeforeRemoving() throws {
        let inside = try file("inside.txt")
        let outside = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("outside".utf8).write(to: outside); defer { try? FileManager.default.removeItem(at: outside) }
        let link = root.appendingPathComponent("link")
        try FileManager.default.createSymbolicLink(at: link, withDestinationURL: outside)
        let store = ReceivedStore(root: root)
        XCTAssertThrowsError(try store.delete([inside, outside]))
        XCTAssertThrowsError(try store.delete([link]))
        XCTAssertThrowsError(try store.delete([root]))
        XCTAssertTrue(FileManager.default.fileExists(atPath: inside.path))
        try store.delete([inside])
        XCTAssertFalse(FileManager.default.fileExists(atPath: inside.path))
        XCTAssertEqual(try Data(contentsOf: outside), Data("outside".utf8))
    }
    func testSearchAndMultipleFilesAndFolders() throws {
        let first = try file("Report.txt"), second = try file("photo.jpg")
        let folder = root.appendingPathComponent("Folder", isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: false)
        let store = ReceivedStore(root: root)
        XCTAssertEqual(try store.contents(of: root, search: "report"), [first])
        XCTAssertEqual(try store.contents(of: root).count, 3)
        try store.delete([first, folder])
        XCTAssertEqual(try store.contents(of: root), [second])
    }
    func testConfiguredRootCanUseACanonicalPathAlias() throws {
        _ = try file("original.txt")
        let alias = root.appendingPathComponent("alias", isDirectory: true)
        try FileManager.default.createSymbolicLink(at: alias, withDestinationURL: root)
        let store = ReceivedStore(root: alias)
        XCTAssertEqual(try store.contents(of: alias).map(\.lastPathComponent), ["original.txt"])
        _ = try store.rename(alias.appendingPathComponent("original.txt"), to: "renamed.txt")
        XCTAssertTrue(FileManager.default.fileExists(atPath: root.appendingPathComponent("renamed.txt").path))
    }
    func testReviewTotalsEmptyFilesAndFolderContents() throws {
        let first = try file("zero.txt", contents: ""), second = try file("data.txt", contents: "12345")
        let review = try SendReview.make(recipient: "Test", urls: [first, second])
        XCTAssertEqual(review.total, 5); XCTAssertEqual(review.files.map(\.bytes), [0, 5])
        let folder = try SendReview.make(recipient: "Test", urls: [root])
        XCTAssertEqual(folder.total, 5); XCTAssertEqual(folder.entries.filter { !$0.isDirectory }.count, 2)
        XCTAssertThrowsError(try SendReview.make(recipient: "Test", urls: []))
    }
    func testFolderStagingPreservesTreeAndRejectsNestedLinksAtomically() throws {
        let folder = root.appendingPathComponent("Tree", isDirectory: true), inbox = root.appendingPathComponent("Inbox", isDirectory: true)
        try FileManager.default.createDirectory(at: folder.appendingPathComponent("Sub/Empty"), withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: inbox, withIntermediateDirectories: true)
        let file = folder.appendingPathComponent("Sub/Möte.txt"); try Data("folder bytes".utf8).write(to: file)
        let staged = try ShareInbox.stage(folder, inbox: inbox, allowDirectories: true)
        XCTAssertEqual(try Data(contentsOf: staged.appendingPathComponent("Sub/Möte.txt")), Data("folder bytes".utf8))
        XCTAssertTrue(FileManager.default.fileExists(atPath: staged.appendingPathComponent("Sub/Empty").path))
        XCTAssertEqual(try SelectionSnapshot.inspect(staged).signature, try SelectionSnapshot.inspect(folder).signature)
        let before = try FileManager.default.contentsOfDirectory(at: inbox, includingPropertiesForKeys: nil)
        try FileManager.default.createSymbolicLink(at: folder.appendingPathComponent("Sub/link"), withDestinationURL: file)
        XCTAssertThrowsError(try ShareInbox.stage(folder, inbox: inbox, allowDirectories: true))
        XCTAssertEqual(try FileManager.default.contentsOfDirectory(at: inbox, includingPropertiesForKeys: nil), before)
        XCTAssertEqual(try Data(contentsOf: file), Data("folder bytes".utf8))
    }
    func testFolderReviewDetectsSameSizeChangesAndAddedEntries() throws {
        let folder = root.appendingPathComponent("Tree", isDirectory: true); try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let file = folder.appendingPathComponent("file"); try Data("abc".utf8).write(to: file)
        let before = try SelectionSnapshot.inspect(folder)
        try Data("xyz".utf8).write(to: file)
        XCTAssertNotEqual(before.signature, try SelectionSnapshot.inspect(folder).signature)
        let changed = try SelectionSnapshot.inspect(folder)
        try FileManager.default.createDirectory(at: folder.appendingPathComponent("Empty"), withIntermediateDirectories: false)
        XCTAssertNotEqual(changed.signature, try SelectionSnapshot.inspect(folder).signature)
        XCTAssertThrowsError(try SendReview.make(recipient: "Test", urls: [file, file]))
    }
    func testQueuedFolderOwnershipSurvivesPartialProgress() throws {
        let folder = root.appendingPathComponent("Tree", isDirectory: true), inbox = root.appendingPathComponent("Inbox", isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: false); try FileManager.default.createDirectory(at: inbox, withIntermediateDirectories: false)
        try Data("keep".utf8).write(to: folder.appendingPathComponent("child"))
        let staged = try ShareInbox.stage(folder, inbox: inbox, allowDirectories: true)
        try ShareInbox.markQueued(staged, inbox: inbox)
        XCTAssertTrue(try ShareInbox.pending(inbox: inbox).isEmpty)
        XCTAssertTrue(FileManager.default.fileExists(atPath: staged.appendingPathComponent("child").path))
        let batch = try XCTUnwrap(ShareInbox.ownedBatch(staged.appendingPathComponent("child"), inbox: inbox))
        XCTAssertEqual(ShareInbox.queuedBatches(inbox: inbox), [batch])
        ShareInbox.removeBatch(batch, inbox: inbox)
        XCTAssertFalse(FileManager.default.fileExists(atPath: staged.path)); XCTAssertEqual(try Data(contentsOf: folder.appendingPathComponent("child")), Data("keep".utf8))
    }
    func testETAHandlesZeroRateAndRoundsUp() {
        XCTAssertNil(MobileProgress.eta(remaining: 100, rate: 0))
        XCTAssertNil(MobileProgress.eta(remaining: 0, rate: 10))
        XCTAssertEqual(MobileProgress.eta(remaining: 101, rate: 10), 11)
        XCTAssertEqual(MobileProgress.eta(remaining: 100, rate: 10), 10)
        var progress = MobileProgress(); progress.remainingSeconds = 7199
        XCTAssertEqual(progress.timeRemaining, "About 2h 0 min remaining")
    }
}
