"""Execute production Mac grouping and mobile-icon rules without opening UI."""
from pathlib import Path
import subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(sys.platform=='darwin','Swift host required')
class MacPeerGrouping(unittest.TestCase):
 def test_one_card_per_identity_preserves_explicit_endpoints(self):
  source=(ROOT/'clients/macos/BitSkim.swift').read_text()
  types=source[source.index('struct Peer:'):source.index('final class CollectionTable:')]
  main=r'''import Foundation
'''+types+r'''
@main enum Check {static func main(){
 let identity=String(repeating:"a",count:32)
 let a=Peer(id:identity+":10.1.2.3",name:"iPhone",platform:"iOS",address:"10.1.2.3",port:46441)
 let b=Peer(id:identity+":192.168.1.3",name:"iPhone",platform:"iOS",address:"192.168.1.3",port:46441)
 let c=Peer(id:String(repeating:"b",count:32)+":10.1.2.4",name:"PC",platform:"Windows",address:"10.1.2.4",port:46440)
 let groups=PeerComputer.grouped([a,a,b,c]);assert(groups.count==2)
 assert(groups[0].id==identity && groups[0].endpoints.count==2)
 assert(a.endpointLabel=="10.1.2.3:46441" && a.symbolName=="iphone")
 let pad=Peer(id:"manual",name:"iPad",platform:"iOS",address:"10.1.2.5",port:46440)
 assert(pad.symbolName=="ipad")
 let refreshed=PeerComputer.grouped([b,c]);assert(refreshed[0].id==identity && refreshed[0].endpoints.count==1)
 print("PASS grouped identities, preserved endpoints, stable cards and mobile icons")
}}
'''
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'check.swift').write_text(main)
   subprocess.run(['swiftc','-parse-as-library','-module-cache-path',str(root/'cache'),'-I'+str(ROOT/'core'),'-import-objc-header',str(ROOT/'core/bitskim.h'),str(root/'check.swift'),'-o',str(root/'check')],check=True,capture_output=True)
   subprocess.run([str(root/'check')],check=True,capture_output=True)
