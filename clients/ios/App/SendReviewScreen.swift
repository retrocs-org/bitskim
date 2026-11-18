import SwiftUI

struct SendReviewScreen: View {
    @ObservedObject var client: IOSClient
    let review: SendReview
    var body: some View {
        NavigationStack {
            MobileList {
                Section("Recipient") {
                    Text(review.recipient).font(.headline)
                    if !review.endpoint.isEmpty, review.endpoint != review.recipient { Text(review.endpoint).font(.caption).monospaced() }
                }
                Section("Contents · \(review.entries.count) items") {
                    ForEach(review.entries) { file in
                        HStack { Label(file.path, systemImage: file.isDirectory ? "folder" : "doc"); Spacer(); if !file.isDirectory { Text(MobileProgress.size(file.bytes)).font(.caption) } }
                    }
                    LabeledContent("Total", value: MobileProgress.size(review.total))
                }
                Section {
                    Text(client.security == 2 ? "Encryption required." : client.security == 1 ? "Encryption preferred. Older clients may use an unencrypted connection." : "Encryption is disabled for this transfer.")
                    Text("The recipient must approve the transfer. Keep BitSkim open until it finishes.").font(.footnote)
                    Button("Send selected items") { client.confirmSend(review) }.font(.headline).disabled(client.importing)
                    if client.importing { ProgressView("Checking selection…") }
                    Button("Cancel", role: .cancel) { client.closeReview() }
                }
            }.navigationTitle("Review transfer")
        }
    }
}
