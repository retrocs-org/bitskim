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
    func testReviewTotalsEmptyFilesAndRejectsDirectories() throws {
        let first = try file("zero.txt", contents: ""), second = try file("data.txt", contents: "12345")
        let review = try SendReview.make(recipient: "Test", urls: [first, second])
        XCTAssertEqual(review.total, 5); XCTAssertEqual(review.files.map(\.bytes), [0, 5])
        XCTAssertThrowsError(try SendReview.make(recipient: "Test", urls: [root]))
        XCTAssertThrowsError(try SendReview.make(recipient: "Test", urls: []))
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
