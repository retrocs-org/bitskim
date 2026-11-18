import AppKit
import SwiftUI

// Uses the packaged native views/engine with a throwaway preference domain/profile.
enum MacStartupReview {
    static func run() -> Int32 {
        let arguments = CommandLine.arguments
        let output: URL
        if let index = arguments.firstIndex(of: "--review-output"), index + 1 < arguments.count {
            output = URL(fileURLWithPath: arguments[index + 1], isDirectory: true)
        } else { output = FileManager.default.temporaryDirectory.appendingPathComponent("BitSkim-native-review-" + UUID().uuidString, isDirectory: true) }
        let profile = output.appendingPathComponent("Profile-" + UUID().uuidString, isDirectory: true)
        let domain = "org.bitskim.native-review." + UUID().uuidString
        guard let defaults = UserDefaults(suiteName: domain) else { return 1 }
        defer { defaults.removePersistentDomain(forName: domain); try? FileManager.default.removeItem(at: profile) }
        do {
            try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
            let received = profile.appendingPathComponent("Received", isDirectory: true)
            try FileManager.default.createDirectory(at: received, withIntermediateDirectories: true)
            defaults.set("Native review", forKey: "name"); defaults.set(false, forKey: "visible"); defaults.set(false, forKey: "receiving")
            defaults.set(false, forKey: "keepInMenuBar"); defaults.set(received.path, forKey: "folder")
            guard Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String == "0.2" else { throw fail("Packaged application version is not 0.2") }
            try autoreleasepool {
                let model = ClientModel(defaults: defaults, storageDirectory: profile)
                guard model.engine != nil else { throw fail("Native listener/security initialization failed. Close another BitSkim copy and retry.") }
                RunLoop.current.run(until: Date().addingTimeInterval(0.12))
                guard model.status.lowercased().contains("ready") else { throw fail("Native startup status: " + model.status) }
                let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 560, height: 480), styleMask: [.titled, .closable, .resizable], backing: .buffered, defer: false)
                window.title = "BitSkim native review"; window.isReleasedWhenClosed = false
                defer { window.contentView = nil; window.close() }
                for theme in AppTheme.all {
                    model.themeID = theme.id
                    let screens: [(String, AnyView, NSSize)] = [
                        ("main", AnyView(MainView(model: model)), NSSize(width: 420, height: 380)),
                        ("preferences", AnyView(PreferencesView(model: model, close: {})), NSSize(width: 620, height: 650)),
                        ("tools", AnyView(TransferToolsView(model: model)), NSSize(width: 720, height: 620))
                    ]
                    for (name, view, size) in screens {
                        let host = NSHostingView(rootView: view); window.setContentSize(size); window.contentView = host
                        window.orderFront(nil); host.layoutSubtreeIfNeeded()
                        RunLoop.current.run(until: Date().addingTimeInterval(0.06))
                        guard let image = host.bitmapImageRepForCachingDisplay(in: host.bounds) else { throw fail("Native view did not render: " + name) }
                        host.cacheDisplay(in: host.bounds, to: image)
                        guard image.pixelsWide > 0, image.pixelsHigh > 0, let png = image.representation(using: .png, properties: [:]) else { throw fail("Native render is empty: " + name) }
                        try png.write(to: output.appendingPathComponent(theme.id + "-" + name + ".png"))
                    }
                }
                guard bs_queue_count(model.engine!) == 0 else { throw fail("Native review queued unexpected work") }
            }
            let result = "PASS native Mac 0.2 bundle version, isolated startup/TLS identity, main/preferences/tools render in all eight themes, no queued sends\n"
            try result.write(to: output.appendingPathComponent("native-review.txt"), atomically: true, encoding: .utf8)
            print(result, terminator: ""); return 0
        } catch { fputs("FAIL native Mac review: \(error.localizedDescription)\n", stderr); return 1 }
    }
    private static func fail(_ message: String) -> NSError { NSError(domain: "BitSkimNativeReview", code: 1, userInfo: [NSLocalizedDescriptionKey: message]) }
}
