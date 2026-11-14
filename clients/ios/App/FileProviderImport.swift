import SwiftUI
import UniformTypeIdentifiers
import UIKit

struct FileProviderImport: UIViewControllerRepresentable {
    let completion: (Result<[URL], Error>) -> Void
    static func controller() -> UIDocumentPickerViewController {
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.data], asCopy: true)
        picker.allowsMultipleSelection = true
        return picker
    }
    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let picker = Self.controller()
        picker.delegate = context.coordinator
        return picker
    }
    func updateUIViewController(_ controller: UIDocumentPickerViewController, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator(completion: completion) }
    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let completion: (Result<[URL], Error>) -> Void
        init(completion: @escaping (Result<[URL], Error>) -> Void) { self.completion = completion }
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) { completion(.success(urls)) }
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            completion(.failure(NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)))
        }
    }
}
