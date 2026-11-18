#if DEBUG
import SwiftUI

// Explicit local-test launch mode: synthetic rows, no listener or file operations.
struct ReviewAccessibilityHarness: View {
    @State private var outcome = ""
    private let theme = MobileTheme.resolve(ProcessInfo.processInfo.environment["BITSKIM_REVIEW_THEME"] ?? "mocha")
    private let checking = ProcessInfo.processInfo.arguments.contains("--checking")
    private static let review: SendReview = {
        let root = URL(fileURLWithPath: "/accessibility-fixtures/Folder")
        var entries = [SendEntry(url: root, path: "Folder/", bytes: 0, isDirectory: true)]
        for index in 1...120 {
            let path = "Folder/" + (index == 1 ? String(repeating: "Long filename möte-", count: 8) : "File-\(index)") + ".txt"
            entries.append(SendEntry(url: URL(fileURLWithPath: "/accessibility-fixtures/" + path), path: path, bytes: 12, isDirectory: false))
        }
        let snapshot = SelectionSnapshot(entries: entries, signature: "layout-fixture")
        return SendReview(recipient: "Test recipient", endpoint: "192.0.2.1:46440", files: [SendFile(url: root, bytes: snapshot.total, snapshot: snapshot)], entries: entries)
    }()
    var body: some View {
        Group {
            if outcome.isEmpty {
                TransferReviewContent(review: Self.review, security: 1, checking: checking,
                                      cancel: { outcome = "Review cancelled" }, send: { outcome = "Send selected" })
            } else { Text(outcome).accessibilityIdentifier("review-outcome") }
        }
        .environment(\.mobileTheme, theme).tint(theme?.accent ?? .accentColor)
        .preferredColorScheme(theme.map { $0.dark ? .dark : .light })
    }
}
#endif
