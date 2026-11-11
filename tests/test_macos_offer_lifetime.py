"""Run the production Mac decision guards against two real sequential offers."""
from pathlib import Path
import socket,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(sys.platform=='darwin','AppKit host required')
class MacOfferLifetime(unittest.TestCase):
 def test_expired_offer_cannot_accept_or_decline_its_replacement(self):
  source=(ROOT/'clients/macos/BitSkim.swift').read_text()
  guard=source[source.index('    private var automaticallyAcceptCurrent'):source.index('    private func incoming(peer:')]
  accept=source[source.index('    private func accept(_ url:'):source.index('    func chooseFolder()')]
  send=source[source.index('    func send(_ url:'):source.index('    func chooseFile(for peer:')]
  swift='''import AppKit
struct Peer {let id: String;let address: String;let port: UInt16}
final class Harness {
 var securityMode=0
 var transportMode=0
 var startupError=""
 var autoAccept=true
 var trustedAutoAcceptOnly=true
 var engine: OpaquePointer?
 var conflictPolicy=2
 var askLocation=false
 var folder="/unused"
 var incomingPanel: NSSavePanel?
 var incomingAlert: NSAlert?
 var revealWindow: (() -> Void)?
 func uniqueURL(_ url: URL) -> URL { url }
 func spaceReport(_ destination: String, collection: Bool) -> String { "" }
 func error(_ message: String) { fatalError(message) }
'''+guard+accept+send+'''
 func verify(sender: OpaquePointer, receiver: OpaquePointer, port: UInt16, directory: URL) {
  engine=receiver
  let source=directory.appendingPathComponent("source")
  let oldDestination=directory.appendingPathComponent("old")
  let destination=directory.appendingPathComponent("new")
  try! Data("replacement".utf8).write(to: source)
  try! Data("original".utf8).write(to: oldDestination)
  func pump(_ ready: () -> Bool) {
   let end=Date().addingTimeInterval(5)
   while Date()<end { bs_service(sender,4);bs_service(receiver,4);if ready(){return};Thread.sleep(forTimeInterval:0.001) }
   fatalError("Transfer timed out")
  }
  assert(bs_send(sender,"127.0.0.1",port,source.path)==1)
  pump {bs_status(receiver)!.pointee.awaiting_decision != 0}
  let expired=bs_transfer_serial(receiver)
  bs_cancel(sender)
  pump {bs_status(sender)!.pointee.active==0 && bs_status(receiver)!.pointee.active==0}
  assert(bs_send(sender,"127.0.0.1",port,source.path)==1)
  pump {bs_status(receiver)!.pointee.awaiting_decision != 0}
  let current=bs_transfer_serial(receiver)
  assert(current != expired && !offerMatches(expired) && offerMatches(current))
  assert(!automaticallyAcceptCurrent)
  trustedAutoAcceptOnly=false;assert(automaticallyAcceptCurrent)
  autoAccept=false;assert(!automaticallyAcceptCurrent)
  accept(oldDestination,serial:expired)
  chooseDestination("old",serial:expired)
  assert(bs_status(receiver)!.pointee.awaiting_decision != 0)
  assert(try! Data(contentsOf:oldDestination)==Data("original".utf8))
  accept(destination,serial:current)
  pump {bs_status(sender)!.pointee.active==0 && bs_status(receiver)!.pointee.active==0}
  assert(try! Data(contentsOf:destination)==Data("replacement".utf8))
  engine=sender
  assert(bs_queue_address(sender,"127.0.0.1",port,source.path)==1)
  bs_queue_pause(sender)
  send(source,to:Peer(id:"127.0.0.1",address:"127.0.0.1",port:port))
  assert(bs_queue_count(sender)==2 && bs_queue_paused(sender) != 0)
 }
}
@main enum TestMain {
 static func main() {
  assert(bs_platform_init() != 0)
  let sender=bs_audit_client(UInt16(CommandLine.arguments[1])!)!
  let port=UInt16(CommandLine.arguments[2])!
  let receiver=bs_audit_client(port)!
  Harness().verify(sender:sender,receiver:receiver,port:port,directory:URL(fileURLWithPath:CommandLine.arguments[3]))
  bs_destroy(sender);bs_destroy(receiver)
  print("PASS stale-offer decision guards")
 }
}
'''
  with tempfile.TemporaryDirectory() as d:
   folder=Path(d);(folder/'harness.swift').write_text(swift)
   (folder/'bridge.h').write_text('#include "bitskim.h"\nBSClient *bs_audit_client(unsigned short port);\n')
   (folder/'client.c').write_text('''#include "bitskim.h"
#include <stdio.h>
#include <string.h>
BSClient *bs_audit_client(unsigned short port){BSConfig c;memset(&c,0,sizeof c);sprintf(c.id,"%032x",port);strcpy(c.name,"audit");strcpy(c.platform,"host");strcpy(c.bind_address,"127.0.0.1");c.port=port;c.receiving=1;return bs_create(&c,NULL,NULL);}
''')
   objects=[]
   for path in (ROOT/'core/bitskim.c',ROOT/'core/platform.c',folder/'client.c'):
    target=folder/(path.stem+'.o');objects.append(str(target))
    subprocess.run(['cc','-std=c89','-I'+str(ROOT/'core'),'-c',str(path),'-o',str(target)],check=True,capture_output=True)
   subprocess.run(['swiftc','-parse-as-library','-swift-version','5','-module-cache-path',str(ROOT/'build/module-cache'),'-I'+str(ROOT/'core'),'-import-objc-header',str(folder/'bridge.h'),str(folder/'harness.swift'),*objects,'-o',str(folder/'check')],check=True,capture_output=True)
   holders=[socket.socket(),socket.socket()]
   for sock in holders:sock.bind(('127.0.0.1',0))
   ports=[str(sock.getsockname()[1]) for sock in holders]
   for sock in holders:sock.close()
   result=subprocess.run([str(folder/'check'),*ports,d],check=True,capture_output=True,text=True)
   self.assertIn('PASS stale-offer',result.stdout)
