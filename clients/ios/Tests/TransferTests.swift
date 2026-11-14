import XCTest
import Foundation
import Darwin
import UIKit
import Combine

final class TransferTests: XCTestCase {
    private var sender: OpaquePointer!, receiver: OpaquePointer!
    private var root: URL!
    private var receiverPort: UInt16 = 0
    private var senderPort: UInt16 = 0
    override func setUpWithError() throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        XCTAssertNotEqual(bs_platform_init(), 0)
        senderPort = freePort(); sender = makeClient("1", port: senderPort); receiverPort = freePort(); receiver = makeClient("2", port: receiverPort)
        XCTAssertNotNil(sender); XCTAssertNotNil(receiver)
        guard sender != nil, receiver != nil else { throw NSError(domain: "BitSkimTests", code: 1) }
        let trust = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("Trust", isDirectory: true)
        try FileManager.default.createDirectory(at: trust, withIntermediateDirectories: true)
        XCTAssertNotEqual(bs_trust_configure(sender, trust.path), 0)
        bs_batch_enable(sender, 1); bs_batch_enable(receiver, 1)
    }
    override func tearDownWithError() throws {
        if let sender { bs_destroy(sender) }; if let receiver { bs_destroy(receiver) }
        bs_platform_cleanup(); if let root { try? FileManager.default.removeItem(at: root) }
    }
    private func freePort() -> UInt16 {
        let socket = Darwin.socket(AF_INET, SOCK_STREAM, 0); defer { Darwin.close(socket) }
        var address = sockaddr_in(); address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size); address.sin_family = sa_family_t(AF_INET); address.sin_addr.s_addr = inet_addr("127.0.0.1")
        withUnsafePointer(to: &address) { pointer in pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { XCTAssertEqual(bind(socket, $0, socklen_t(MemoryLayout<sockaddr_in>.size)), 0) } }
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        withUnsafeMutablePointer(to: &address) { pointer in pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { XCTAssertEqual(getsockname(socket, $0, &length), 0) } }
        return UInt16(bigEndian: address.sin_port)
    }
    private func tuple<T>(_ field: inout T, _ string: String) {
        withUnsafeMutableBytes(of: &field) { buffer in
            let data = Array(string.utf8) + [0]; precondition(data.count <= buffer.count)
            for (index, byte) in data.enumerated() { buffer[index] = byte }
        }
    }
    private func makeClient(_ suffix: String, port: UInt16) -> OpaquePointer? {
        var config = BSConfig(); tuple(&config.id, String(repeating: "0", count: 31) + suffix)
        tuple(&config.name, "iOS unit check"); tuple(&config.platform, "iOS"); tuple(&config.bind_address, "127.0.0.1")
        config.port = port; config.receiving = 1
        return bs_create(&config, nil, nil)
    }
    @discardableResult private func pump(_ ready: () -> Bool) -> Bool {
        let end = Date().addingTimeInterval(12)
        while Date() < end {
            bs_service(sender, 4); bs_service(receiver, 4)
            if ready() { return true }
            usleep(1000)
        }
        XCTFail("Transfer timed out: \(String(cString: bs_last_error(sender))) / \(String(cString: bs_last_error(receiver)))")
        return false
    }
    func testEncryptedUnicodeBinaryAndVerifiedReceipt() throws {
        let source = root.appendingPathComponent("å-λ.bin"), destination = root.appendingPathComponent("received.bin")
        let bytes = Data((0..<262144).map { UInt8($0 & 255) }); try bytes.write(to: source)
        XCTAssertEqual(bs_send(sender, "127.0.0.1", receiverPort, source.path), 1)
        guard pump({ bs_status(receiver)!.pointee.awaiting_decision != 0 }) else { return }
        XCTAssertEqual(bs_transfer_encrypted(sender), 1); XCTAssertEqual(bs_transfer_encrypted(receiver), 1)
        XCTAssertEqual(bs_accept(receiver, destination.path), 1)
        guard pump({ bs_status(sender)!.pointee.active == 0 && bs_status(receiver)!.pointee.active == 0 }) else { return }
        XCTAssertEqual(try Data(contentsOf: destination), bytes)
        var item = BSHistoryItem(); XCTAssertEqual(bs_history_item(receiver, 0, &item), 1)
        XCTAssertEqual(item.result, Int32(BS_RESULT_COMPLETE.rawValue)); XCTAssertEqual(item.verified, 1)
    }
    func testSelectedAdvertisedEndpointKeepsVerifiedReceipt() throws {
        let source = root.appendingPathComponent("selected"), destination = root.appendingPathComponent("received")
        let bytes = Data("selected address".utf8); try bytes.write(to: source)
        let udp = Darwin.socket(AF_INET, SOCK_DGRAM, 0); defer { Darwin.close(udp) }
        var address = sockaddr_in(); address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size); address.sin_family = sa_family_t(AF_INET)
        address.sin_port = senderPort.bigEndian; address.sin_addr.s_addr = inet_addr("127.0.0.1")
        var packet = Array("BSKD".utf8) + [UInt8(1), 2] + [UInt8](repeating: 0, count: 15) + [2, UInt8(receiverPort >> 8), UInt8(receiverPort & 255), 4, 3] + Array("PeeriOS".utf8)
        packet += [UInt8](repeating: 0, count: 160 - packet.count)
        packet.withUnsafeBytes { data in
            withUnsafePointer(to: &address) { pointer in pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                XCTAssertEqual(sendto(udp, data.baseAddress, data.count, 0, $0, socklen_t(MemoryLayout<sockaddr_in>.size)), data.count)
            } }
        }
        guard pump({ bs_peer_count(sender) == 1 }) else { return }
        let identity = String(repeating: "0", count: 31) + "2"
        XCTAssertEqual(bs_queue_peer_address(sender, identity, "127.0.0.1", receiverPort, source.path), 1)
        guard pump({ bs_status(receiver)!.pointee.awaiting_decision != 0 }) else { return }
        XCTAssertEqual(bs_accept(receiver, destination.path), 1)
        guard pump({ bs_queue_count(sender) == 0 && bs_status(receiver)!.pointee.active == 0 }) else { return }
        XCTAssertEqual(try Data(contentsOf: destination), bytes)
        var item = BSHistoryItem(); XCTAssertEqual(bs_history_item(receiver, 0, &item), 1); XCTAssertEqual(item.verified, 1)
    }
    @MainActor func testFilesExportCopiesAndCancellationKeepsOriginal() async throws {
        let source = root.appendingPathComponent("export.txt"); let bytes = Data("keep original".utf8); try bytes.write(to: source)
        let picker = FileProviderExport.controller(for: [source])
        XCTAssertEqual(picker.documentPickerMode, .exportToService)
        var result: Bool?
        let delegate = FileProviderExport.Coordinator { result = $0 }
        delegate.documentPickerWasCancelled(picker)
        XCTAssertEqual(result, false); XCTAssertEqual(try Data(contentsOf: source), bytes)
        delegate.documentPicker(picker, didPickDocumentsAt: [root.appendingPathComponent("copy")])
        XCTAssertEqual(result, true); XCTAssertEqual(try Data(contentsOf: source), bytes)
    }
    @MainActor func testStandardFilesImportAllowsMultipleFilesAndCancels() async throws {
        let picker = FileProviderImport.controller()
        XCTAssertNil(picker.directoryURL)
        XCTAssertTrue(picker.allowsMultipleSelection)
        var selected: [URL]?
        var cancelled = false
        let delegate = FileProviderImport.Coordinator { result in
            switch result {
            case .success(let urls): selected = urls
            case .failure(let error): cancelled = (error as NSError).code == NSUserCancelledError
            }
        }
        delegate.documentPickerWasCancelled(picker)
        XCTAssertTrue(cancelled); XCTAssertNil(selected)
        let source = root.appendingPathComponent("original.txt")
        try Data("original".utf8).write(to: source)
        delegate.documentPicker(picker, didPickDocumentsAt: [source])
        XCTAssertEqual(selected, [source])
        XCTAssertEqual(try String(contentsOf: source), "original")
    }
    @MainActor func testIdleReceiverDoesNotRepublishEmptyOffers() async throws {
        let model = IOSClient(listeningPort: freePort())
        model.start(); defer { model.stop() }
        XCTAssertNil(model.error)
        var emptyOffers = 0
        let subscription = model.$offer.dropFirst().sink { if $0 == nil { emptyOffers += 1 } }
        try await Task.sleep(nanoseconds: 250_000_000)
        withExtendedLifetime(subscription) { XCTAssertEqual(emptyOffers, 0) }
    }
    func testEmptyFile() throws {
        let source = root.appendingPathComponent("empty"), destination = root.appendingPathComponent("received")
        try Data().write(to: source)
        XCTAssertEqual(bs_send(sender, "127.0.0.1", receiverPort, source.path), 1)
        guard pump({ bs_status(receiver)!.pointee.awaiting_decision != 0 }) else { return }
        XCTAssertEqual(bs_accept(receiver, destination.path), 1)
        guard pump({ bs_status(sender)!.pointee.active == 0 && bs_status(receiver)!.pointee.active == 0 }) else { return }
        XCTAssertEqual(try Data(contentsOf: destination), Data())
    }
    func testCancellationRemovesPartial() throws {
        let source = root.appendingPathComponent("large"), destination = root.appendingPathComponent("received")
        try Data(repeating: 42, count: 2 * 1024 * 1024).write(to: source)
        XCTAssertEqual(bs_limits_configure(sender, 65536, 0), 1)
        XCTAssertEqual(bs_send(sender, "127.0.0.1", receiverPort, source.path), 1)
        guard pump({ bs_status(receiver)!.pointee.awaiting_decision != 0 }) else { return }
        XCTAssertEqual(bs_accept(receiver, destination.path), 1)
        guard pump({ bs_status(receiver)!.pointee.transferred > 0 }) else { return }
        bs_cancel(receiver)
        guard pump({ bs_status(sender)!.pointee.active == 0 && bs_status(receiver)!.pointee.active == 0 }) else { return }
        XCTAssertFalse(FileManager.default.fileExists(atPath: destination.path))
        XCTAssertEqual(bs_transfer_result(receiver), BS_RESULT_CANCELLED)
    }
    func testNestedFolderAndEmptyDirectory() throws {
        let source = root.appendingPathComponent("Tree", isDirectory: true)
        try FileManager.default.createDirectory(at: source.appendingPathComponent("sub/empty"), withIntermediateDirectories: true)
        try Data("folder payload".utf8).write(to: source.appendingPathComponent("sub/å-λ.txt"))
        let destination = root.appendingPathComponent("inbox", isDirectory: true)
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: false)
        XCTAssertEqual(bs_queue_address(sender, "127.0.0.1", receiverPort, source.path), 1)
        guard pump({ bs_status(receiver)!.pointee.awaiting_decision != 0 }) else { return }
        XCTAssertNotEqual(bs_batch_is_tree(receiver), 0)
        XCTAssertEqual(bs_accept_batch(receiver, destination.path), 1)
        guard pump({ bs_queue_count(sender) == 0 && bs_status(sender)!.pointee.active == 0 && bs_status(receiver)!.pointee.active == 0 }) else { return }
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("Tree/sub/å-λ.txt")), Data("folder payload".utf8))
        XCTAssertTrue(FileManager.default.fileExists(atPath: destination.appendingPathComponent("Tree/sub/empty").path))
    }
    func testInboxRejectsOversizedFilesAndFoldersWithoutPartialCommit() throws {
        let inbox = root.appendingPathComponent("Inbox"); try FileManager.default.createDirectory(at: inbox, withIntermediateDirectories: false)
        let source = root.appendingPathComponent("too-large")
        FileManager.default.createFile(atPath: source.path, contents: nil)
        let file = try FileHandle(forWritingTo: source); try file.truncate(atOffset: 2_147_483_648); try file.close()
        XCTAssertThrowsError(try ShareInbox.stage(source, inbox: inbox))
        XCTAssertThrowsError(try ShareInbox.stage(root, inbox: inbox))
        XCTAssertTrue(try FileManager.default.contentsOfDirectory(at: inbox, includingPropertiesForKeys: nil).isEmpty)
    }
    func testSharedInboxPreservesOriginalNamesAndEmptyFiles() throws {
        let inbox = root.appendingPathComponent("Inbox"); try FileManager.default.createDirectory(at: inbox, withIntermediateDirectories: false)
        let source = root.appendingPathComponent(".å-λ.txt"); try Data().write(to: source)
        let first = try ShareInbox.stage(source, inbox: inbox), second = try ShareInbox.stage(source, inbox: inbox)
        XCTAssertNotEqual(first, second); XCTAssertEqual(first.lastPathComponent, source.lastPathComponent)
        XCTAssertEqual(try ShareInbox.pending(inbox: inbox).count, 2)
        XCTAssertEqual(try Data(contentsOf: first), Data()); XCTAssertTrue(FileManager.default.fileExists(atPath: source.path))
        let link = root.appendingPathComponent("link"); try FileManager.default.createSymbolicLink(at: link, withDestinationURL: source)
        XCTAssertThrowsError(try ShareInbox.stage(link, inbox: inbox))
        let folders = try FileManager.default.contentsOfDirectory(at: inbox, includingPropertiesForKeys: nil)
        XCTAssertEqual(folders.count, 2); XCTAssertFalse(folders.contains { $0.lastPathComponent.hasPrefix(".pending-") })
    }
}
