import AppKit
import SwiftUI
import Combine

final class AppController: NSObject, NSApplicationDelegate, NSWindowDelegate, NSMenuDelegate {
    private var model: ClientModel!
    private var mainWindow: NSWindow!
    private var preferencesWindow: NSWindow?
    private var statusItem: NSStatusItem?
    private var observation: AnyCancellable?
    private lazy var menuBarIcon: NSImage? = {
        guard let url = Bundle.main.url(forResource: "BitSkim-menu", withExtension: "png"),
              let icon = NSImage(contentsOf: url) else { return nil }
        icon.size = NSSize(width: 18, height: 14)
        icon.isTemplate = true
        return icon
    }()

    func applicationDidFinishLaunching(_ notification: Notification) {
        refreshApplicationIcon()
        model = ClientModel()
        model.revealWindow = { [weak self] in self?.showWindow() }
        model.openPreferences = { [weak self] in self?.showPreferences() }
        model.hideWindow = { [weak self] in self?.hideToMenuBar() }
        model.preferencesChanged = { [weak self] in self?.applyPreferences() }
        mainWindow = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 560, height: 480), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        mainWindow.title = "BitSkim"; mainWindow.identifier = NSUserInterfaceItemIdentifier("BitSkimMain")
        mainWindow.minSize = NSSize(width: 420, height: 380); mainWindow.titlebarAppearsTransparent = true
        mainWindow.contentView = NSHostingView(rootView: MainView(model: model))
        mainWindow.isReleasedWhenClosed = false; mainWindow.delegate = self; mainWindow.center()
        buildMainMenu(); applyPreferences()
        observation = model.objectWillChange.sink { [weak self] in
            DispatchQueue.main.async { [weak self] in self?.updateStatus() }
        }
        if !model.startInMenuBar { showWindow() }
    }
    private func refreshApplicationIcon() {
        guard let url = Bundle.main.url(forResource: "BitSkimRetro", withExtension: "icns"),
              let icon = NSImage(contentsOf: url) else { return }
        NSApp.applicationIconImage = icon
        guard NSApp.activationPolicy() == .regular else { return }
        let tile = NSApp.dockTile
        let view = NSImageView(frame: NSRect(origin: .zero, size: tile.size))
        view.image = icon
        view.imageScaling = .scaleProportionallyUpOrDown
        tile.contentView = view
        tile.display()
    }
    private func showDockIcon() {
        NSApp.setActivationPolicy(.regular)
        refreshApplicationIcon()
        // Dock registration completes after the activation-policy transition.
        DispatchQueue.main.async { [weak self] in self?.refreshApplicationIcon() }
    }
    private func buildMainMenu() {
        let bar = NSMenu(); let appItem = NSMenuItem(); bar.addItem(appItem)
        let app = NSMenu(); appItem.submenu = app
        app.addItem(item("Show BitSkim", #selector(showAction), key: "0"))
        app.addItem(item("Preferences…", #selector(preferencesAction), key: ","))
        app.addItem(.separator()); app.addItem(item("Hide to Menu Bar", #selector(hideAction), key: "h"))
        app.addItem(item("Quit BitSkim", #selector(quitAction), key: "q"))
        let editItem = NSMenuItem(); editItem.title = "Edit"; bar.addItem(editItem)
        let edit = NSMenu(title: "Edit"); editItem.submenu = edit
        for (title, selector, key) in [("Cut", #selector(NSText.cut(_:)), "x"), ("Copy", #selector(NSText.copy(_:)), "c"), ("Paste", #selector(NSText.paste(_:)), "v"), ("Select All", #selector(NSText.selectAll(_:)), "a")] {
            edit.addItem(NSMenuItem(title: title, action: selector, keyEquivalent: key))
        }
        NSApp.mainMenu = bar
    }
    private func item(_ title: String, _ action: Selector, key: String = "") -> NSMenuItem {
        let i = NSMenuItem(title: title, action: action, keyEquivalent: key); i.target = self; return i
    }
    private func applyPreferences() {
        if model.keepInMenuBar {
            if statusItem == nil {
                let s = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
                s.button?.image = menuBarIcon
                let menu = NSMenu(); menu.delegate = self; s.menu = menu; statusItem = s
            }
        } else if let s = statusItem {
            NSStatusBar.system.removeStatusItem(s); statusItem = nil
            if !mainWindow.isVisible { showWindow() }
        }
        for w in [mainWindow, preferencesWindow].compactMap({ $0 }) {
            w.appearance = NSAppearance(named: model.theme.dark ? .darkAqua : .aqua)
            w.backgroundColor = AppTheme.ns(model.theme.baseHex)
        }
        updateStatus()
    }
    private func updateStatus() {
        statusItem?.button?.toolTip = "BitSkim 0.1 — \(model.status)"
        statusItem?.button?.image = menuBarIcon
    }
    func menuWillOpen(_ menu: NSMenu) {
        menu.removeAllItems()
        let status = NSMenuItem(title: model.active ? model.status : "BitSkim · \(model.peers.count) nearby", action: nil, keyEquivalent: "")
        status.isEnabled = false; menu.addItem(status)
        menu.addItem(item("Show BitSkim", #selector(showAction)))
        menu.addItem(item("Connect by IP…", #selector(connectAction)))
        if !model.peers.isEmpty {
            let peers = NSMenuItem(title: "Send File To", action: nil, keyEquivalent: ""); let list = NSMenu()
            for peer in model.peers {
                let entry = item("\(peer.name) — \(peer.platform)", #selector(sendAction)); entry.representedObject = peer; entry.isEnabled = !model.active; list.addItem(entry)
            }
            peers.submenu = list; menu.addItem(peers)
        }
        menu.addItem(item("Refresh Computers", #selector(refreshAction)))
        if model.active { menu.addItem(item("Cancel Transfer", #selector(cancelAction))) }
        menu.addItem(.separator())
        let incoming = item("Allow Incoming Transfers", #selector(receivingAction)); incoming.state = model.receiving ? .on : .off; menu.addItem(incoming)
        let themes = NSMenuItem(title: "Theme", action: nil, keyEquivalent: ""); let palette = NSMenu()
        for theme in AppTheme.all {
            let entry = item(theme.name, #selector(themeAction)); entry.representedObject = theme.id
            entry.state = model.themeID == theme.id ? .on : .off; palette.addItem(entry)
        }
        themes.submenu = palette; menu.addItem(themes)
        menu.addItem(item("Preferences…", #selector(preferencesAction)))
        menu.addItem(.separator()); menu.addItem(item("Quit BitSkim", #selector(quitAction)))
    }
    func showWindow() {
        showDockIcon()
        mainWindow.deminiaturize(nil); mainWindow.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    func hideToMenuBar() {
        guard model.keepInMenuBar else { mainWindow.miniaturize(nil); return }
        mainWindow.orderOut(nil); preferencesWindow?.orderOut(nil)
        NSApp.setActivationPolicy(.accessory)
    }
    func showPreferences() {
        showDockIcon()
        if preferencesWindow == nil {
            let w = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 460, height: 560), styleMask: [.titled, .closable], backing: .buffered, defer: false)
            w.title = "BitSkim Preferences"; w.identifier = NSUserInterfaceItemIdentifier("BitSkimPreferences")
            w.contentView = NSHostingView(rootView: PreferencesView(model: model, close: { [weak self] in self?.preferencesWindow?.performClose(nil) })); w.isReleasedWhenClosed = false; w.delegate = self
            w.center(); preferencesWindow = w
        }
        applyPreferences(); preferencesWindow?.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true)
    }
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        sender.orderOut(nil)
        if sender === mainWindow {
            if model.keepInMenuBar { if preferencesWindow?.isVisible != true { NSApp.setActivationPolicy(.accessory) } }
            else { NSApp.terminate(nil) }
        } else if !mainWindow.isVisible && model.keepInMenuBar { NSApp.setActivationPolicy(.accessory) }
        return false
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool { showWindow(); return false }
    @objc private func showAction() { showWindow() }
    @objc private func hideAction() { hideToMenuBar() }
    @objc private func preferencesAction() { showPreferences() }
    @objc private func connectAction() { showWindow(); model.manual = true }
    @objc private func refreshAction() { model.refresh() }
    @objc private func cancelAction() { model.cancel() }
    @objc private func receivingAction() { model.receiving.toggle(); model.savePreferences() }
    @objc private func themeAction(_ sender: NSMenuItem) { if let id = sender.representedObject as? String { model.applyTheme(id) } }
    @objc private func sendAction(_ sender: NSMenuItem) { if let peer = sender.representedObject as? Peer { model.chooseFile(for: peer) } }
    @objc private func quitAction() { NSApp.terminate(nil) }
}
@main enum BitSkimMain {
    static func main() {
        let app = NSApplication.shared
        let controller = AppController(); app.delegate = controller
        app.setActivationPolicy(.accessory)
        withExtendedLifetime(controller) { app.run() }
    }
}
