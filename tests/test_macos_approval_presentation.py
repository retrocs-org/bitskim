"""Exercise production approval scheduling with window stubs, without opening UI."""
from pathlib import Path
import subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(sys.platform=='darwin','Swift host required')
class ApprovalPresentation(unittest.TestCase):
 def test_waits_for_sheet_dismissal_and_rechecks_offer(self):
  source=(ROOT/'clients/macos/BitSkim.swift').read_text()
  helper=source[source.index('    private func presentIncomingAlert('):source.index('    private func incoming(peer:')]
  swift=r'''import Foundation
struct Identifier {var rawValue:String}
class Window {var identifier:Identifier?;var attachedSheet:Window?;init(_ id:String){identifier=Identifier(rawValue:id)}}
enum NSApplication {typealias ModalResponse=Int}
class NSAlert {
 var parent:Window?;var modalCalls=0
 func runModal()->Int {modalCalls+=1;return 1}
 func beginSheetModal(for window:Window,completionHandler:(Int)->Void){parent=window}
}
class Application {var windows:[Window]=[]}
let NSApp=Application()
class Harness {
 var tools=true,manual=true,pending=true
 var incomingAlert:NSAlert?
 var revealWindow:(()->Void)?
 func offerMatches(_ serial:UInt)->Bool {pending && serial==7}
'''+helper+r'''
 func present(_ alert:NSAlert){incomingAlert=alert;presentIncomingAlert(alert,serial:7){_ in}}
}
let main=Window("BitSkimMain"),preferences=Window("BitSkimPreferences"),sheet=Window("tools")
NSApp.windows=[preferences,sheet,main]
main.attachedSheet=sheet
let h=Harness(),alert=NSAlert();h.present(alert)
assert(!h.tools && !h.manual && alert.parent == nil && alert.modalCalls==0)
main.attachedSheet=nil
RunLoop.main.run(until:Date().addingTimeInterval(0.15))
assert(alert.parent === main)
main.attachedSheet=sheet
let expired=NSAlert();h.present(expired);h.pending=false;main.attachedSheet=nil
RunLoop.main.run(until:Date().addingTimeInterval(0.15))
assert(expired.parent == nil && expired.modalCalls==0)
print("PASS main-window approval scheduling and expired-offer guard")
'''
  with tempfile.TemporaryDirectory() as d:
   path=Path(d);(path/'test.swift').write_text(swift)
   subprocess.run(['swiftc','-module-cache-path',str(path/'cache'),str(path/'test.swift'),'-o',str(path/'test')],check=True,capture_output=True)
   subprocess.run([str(path/'test')],check=True,capture_output=True)
