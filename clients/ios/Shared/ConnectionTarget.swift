import Foundation
import Network

struct ConnectionTarget {
    let address: String
    let port: UInt16
    init?(_ text: String) {
        let parts = text.trimmingCharacters(in: .whitespacesAndNewlines).split(separator: ":", omittingEmptySubsequences: false)
        guard (1...2).contains(parts.count), let ip = parts.first, IPv4Address(String(ip)) != nil,
              let port = UInt16(parts.count == 2 ? String(parts[1]) : "46440"), port > 0 else { return nil }
        address = String(ip); self.port = port
    }
}
