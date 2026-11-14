import SwiftUI
import UIKit

struct FileExportSelection: Identifiable {
    let id = UUID()
    let urls: [URL]
}
struct FileProviderExport: UIViewControllerRepresentable {
    let urls: [URL]
    let completion: (Bool) -> Void
    static func controller(for urls: [URL]) -> UIDocumentPickerViewController {
        let picker = UIDocumentPickerViewController(forExporting: urls, asCopy: true)
        return picker
    }
    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let picker = Self.controller(for: urls); picker.delegate = context.coordinator
        return picker
    }
    func updateUIViewController(_ controller: UIDocumentPickerViewController, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator(completion: completion) }
    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let completion: (Bool) -> Void
        init(completion: @escaping (Bool) -> Void) { self.completion = completion }
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) { completion(!urls.isEmpty) }
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) { completion(false) }
    }
}
