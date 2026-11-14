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
                Section("Files · \(review.files.count)") {
                    ForEach(review.files) { file in
                        HStack { Text(file.url.lastPathComponent); Spacer(); Text(MobileProgress.size(file.bytes)).font(.caption) }
                    }
                    LabeledContent("Total", value: MobileProgress.size(review.total))
                }
                Section {
                    Text(client.security == 2 ? "Encryption required." : client.security == 1 ? "Encryption preferred. Older clients may use an unencrypted connection." : "Encryption is disabled for this transfer.")
                    Text("The recipient must approve the transfer. Keep BitSkim open until it finishes.").font(.footnote)
                    Button(review.files.count == 1 ? "Send file" : "Send \(review.files.count) files") { client.confirmSend(review) }.font(.headline)
                    Button("Cancel", role: .cancel) { client.closeReview() }
                }
            }.navigationTitle("Review transfer")
        }
    }
}
