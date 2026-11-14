import SwiftUI

extension MobileTheme {
    static func resolve(_ id: String) -> MobileTheme? { all.first { $0.id == id } }
}
private struct MobileThemeKey: EnvironmentKey {
    static let defaultValue: MobileTheme? = nil
}
extension EnvironmentValues {
    var mobileTheme: MobileTheme? {
        get { self[MobileThemeKey.self] }
        set { self[MobileThemeKey.self] = newValue }
    }
}
struct MobileList<Content: View>: View {
    @Environment(\.mobileTheme) private var theme
    @ViewBuilder let content: Content
    var body: some View {
        List {
            content.listRowBackground(theme?.surface)
        }
        .scrollContentBackground(theme == nil ? .visible : .hidden)
        .background(theme?.base ?? Color(uiColor: .systemGroupedBackground))
        .foregroundStyle(theme?.text ?? Color.primary)
        .toolbarBackground(theme?.base ?? Color(uiColor: .systemBackground), for: .navigationBar)
        .toolbarBackground(theme == nil ? .automatic : .visible, for: .navigationBar)
        .toolbarBackground(theme?.surface ?? Color(uiColor: .systemBackground), for: .tabBar)
        .toolbarBackground(theme == nil ? .automatic : .visible, for: .tabBar)
        .toolbarColorScheme(theme.map { $0.dark ? .dark : .light }, for: .navigationBar)
    }
}
