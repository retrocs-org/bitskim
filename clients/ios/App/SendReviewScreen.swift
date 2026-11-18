import SwiftUI

struct SendReviewScreen: View {
    @ObservedObject var client: IOSClient
    let review: SendReview
    var body: some View {
        TransferReviewContent(review: review, security: client.security, checking: client.importing,
                              cancel: client.closeReview, send: { client.confirmSend(review) })
    }
}

// The same presentation is exercised by the local accessibility UI harness.
struct TransferReviewContent: View {
    let review: SendReview
    let security: Int
    let checking: Bool
    let cancel: () -> Void
    let send: () -> Void
    var body: some View {
        NavigationStack {
            MobileList {
                Section("Recipient") {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(review.recipient).font(.headline).fixedSize(horizontal: false, vertical: true)
                        if !review.endpoint.isEmpty, review.endpoint != review.recipient {
                            Text(review.endpoint).font(.caption).monospaced().fixedSize(horizontal: false, vertical: true)
                        }
                    }.accessibilityElement(children: .combine).accessibilityIdentifier("review-recipient")
                }
                Section("Contents · \(review.entries.count) items") {
                    ForEach(Array(review.entries.enumerated()), id: \.offset) { index, file in
                        VStack(alignment: .leading, spacing: 6) {
                            Label(file.path, systemImage: file.isDirectory ? "folder" : "doc")
                                .fixedSize(horizontal: false, vertical: true)
                            if !file.isDirectory { Text(MobileProgress.size(file.bytes)).font(.caption) }
                        }
                        .accessibilityElement(children: .ignore)
                        .accessibilityLabel((file.isDirectory ? "Folder, " : "File, ") + file.path)
                        .accessibilityValue(file.isDirectory ? "" : MobileProgress.size(file.bytes))
                        .accessibilityIdentifier("review-entry-\(index)")
                    }
                    LabeledContent("Total", value: MobileProgress.size(review.total))
                }
                Section {
                    Text(security == 2 ? "Encryption required." : security == 1 ? "Encryption preferred. Older clients may use an unencrypted connection." : "Encryption is disabled for this transfer.")
                    Text("The recipient must approve the transfer. Keep BitSkim open until it finishes.").font(.footnote)
                }
            }
            .navigationTitle("Review transfer")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", role: .cancel, action: cancel)
                        .keyboardShortcut(.cancelAction).accessibilityIdentifier("review-cancel")
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Send", action: send).disabled(checking)
                        .keyboardShortcut("s", modifiers: .command)
                        .accessibilityLabel("Send \(review.entries.count) items")
                        .accessibilityHint("To " + review.recipient)
                        .accessibilityIdentifier("review-send")
                }
            }
            .safeAreaInset(edge: .bottom) {
                if checking { ProgressView("Checking selection…").padding().accessibilityIdentifier("review-checking") }
            }
        }
    }
}
