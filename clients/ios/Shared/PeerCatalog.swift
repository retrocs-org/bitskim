import Foundation

struct MobilePeer: Identifiable, Equatable {
    let deviceID: String, name: String, platform: String, address: String
    let port: UInt16
    var id: String { deviceID + "@" + ConnectionTarget.endpoint(address, port: port) }
}
struct MobileComputer: Identifiable {
    let id: String, name: String, platform: String
    let endpoints: [MobilePeer]
}
enum PeerCatalog {
    static func computers(_ peers: [MobilePeer]) -> [MobileComputer] {
        var order: [String] = [], groups: [String: [MobilePeer]] = [:], seen: Set<String> = []
        for peer in peers where seen.insert(peer.id).inserted {
            if groups[peer.deviceID] == nil { order.append(peer.deviceID) }
            groups[peer.deviceID, default: []].append(peer)
        }
        return order.compactMap { id in
            guard let endpoints = groups[id], let first = endpoints.first else { return nil }
            return MobileComputer(id: id, name: first.name, platform: first.platform, endpoints: endpoints.sorted { $0.id < $1.id })
        }
    }
}
