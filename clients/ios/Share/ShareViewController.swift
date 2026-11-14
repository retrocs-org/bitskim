import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {
    private let detail = UILabel()
    private let save = UIButton(type: .system)
    private var providers: [NSItemProvider] {
        (extensionContext?.inputItems as? [NSExtensionItem] ?? []).flatMap { $0.attachments ?? [] }
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        let title = UILabel(); title.text = "Add to BitSkim"; title.font = .preferredFont(forTextStyle: .title1)
        detail.text = "Copy these files into BitSkim’s shared inbox. Then open BitSkim and choose a computer to send them to. Files remain on this device until you send them."; detail.numberOfLines = 0; detail.font = .preferredFont(forTextStyle: .body)
        save.setTitle("Add files", for: .normal); save.addTarget(self, action: #selector(addFiles), for: .touchUpInside)
        let cancel = UIButton(type: .system); cancel.setTitle("Cancel", for: .normal); cancel.addTarget(self, action: #selector(cancelShare), for: .touchUpInside)
        let stack = UIStackView(arrangedSubviews: [title, detail, save, cancel]); stack.axis = .vertical; stack.spacing = 20; stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24), stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24), stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 24)])
    }
    @objc private func cancelShare() { extensionContext?.cancelRequest(withError: NSError(domain: NSCocoaErrorDomain, code: NSUserCancelledError)) }
    @objc private func addFiles() {
        let items = providers
        guard !items.isEmpty, items.count <= 32 else { showError("Share between 1 and 32 files at a time."); return }
        save.isEnabled = false; detail.text = "Preparing files…"
        let group = DispatchGroup(), lock = NSLock()
        var failures: [String] = [], saved = 0
        func complete(_ source: URL?, _ failure: Error?) {
            defer { group.leave() }
            do {
                if let failure { throw failure }
                guard let source else { throw NSError(domain: "BitSkim", code: 1, userInfo: [NSLocalizedDescriptionKey: "The provider did not supply a file."]) }
                let access = source.startAccessingSecurityScopedResource(); defer { if access { source.stopAccessingSecurityScopedResource() } }
                try ShareInbox.stage(source)
                lock.lock(); saved += 1; lock.unlock()
            } catch { lock.lock(); failures.append(error.localizedDescription); lock.unlock() }
        }
        for provider in items {
            group.enter()
            if provider.hasItemConformingToTypeIdentifier(UTType.fileURL.identifier) {
                provider.loadItem(forTypeIdentifier: UTType.fileURL.identifier, options: nil) { item, error in complete(item as? URL, error) }
            } else if let type = provider.registeredTypeIdentifiers.first(where: { UTType($0)?.conforms(to: .data) == true }) {
                provider.loadFileRepresentation(forTypeIdentifier: type) { url, error in complete(url, error) }
            } else { complete(nil, NSError(domain: "BitSkim", code: 2, userInfo: [NSLocalizedDescriptionKey: "This item cannot be shared as a file."])) }
        }
        group.notify(queue: .main) { [weak self] in
            guard let self else { return }
            if let failure = failures.first { self.showError("\(saved) files added. \(failure)") }
            else { self.extensionContext?.completeRequest(returningItems: nil) }
        }
    }
    private func showError(_ message: String) {
        let alert = UIAlertController(title: "BitSkim", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Done", style: .default) { [weak self] _ in self?.extensionContext?.completeRequest(returningItems: nil) })
        present(alert, animated: true)
    }
}
