"""Executable checks for the iOS manual-connection fallback and input parser."""
from pathlib import Path
import subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ManualBroadcast(unittest.TestCase):
 def test_manual_build_never_sends_broadcast_packets(self):
  source=r'''
#include "bitskim.h"
#include "platform.h"
#include <assert.h>
#include <sys/socket.h>
static int packets;
ssize_t sendto(int s,const void *p,size_t n,int f,const struct sockaddr *a,socklen_t length){(void)s;(void)p;(void)n;(void)f;(void)a;(void)length;packets++;return -1;}
int main(void){char packet[160]={0};bsp_broadcast(-1,packet,sizeof packet,BS_PORT);assert(packets==0);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   folder=Path(d);(folder/'check.c').write_text(source)
   subprocess.run(['cc','-std=c89','-DBS_NO_BROADCAST','-I'+str(ROOT/'core'),str(ROOT/'core/platform.c'),str(ROOT/'core/bitskim.c'),str(folder/'check.c'),'-o',str(folder/'check')],check=True,capture_output=True)
   subprocess.run([str(folder/'check')],check=True,capture_output=True)
@unittest.skipUnless(sys.platform=='darwin','Swift/Network host required')
class ManualAddress(unittest.TestCase):
 def test_rejects_invalid_and_ambiguous_destinations(self):
  source='''import Foundation
@main enum Check {static func main(){
 let plain=ConnectionTarget(" 192.168.1.5 ")!;assert(plain.address=="192.168.1.5" && plain.port==46440)
 assert(ConnectionTarget("10.1.2.3:1234")!.port==1234)
 assert(ConnectionTarget.endpoint("10.1.2.3",port:46441)=="10.1.2.3:46441")
 for bad in ["", "host.local", "http://10.1.2.3", "999.1.2.3", "1.2.3.4:0", "1.2.3.4:65536", "1.2.3.4:", "::1", "1.2.3.4:1:2"] {assert(ConnectionTarget(bad)==nil,bad)}
}}
'''
  with tempfile.TemporaryDirectory() as d:
   folder=Path(d);(folder/'check.swift').write_text(source)
   subprocess.run(['swiftc','-parse-as-library','-module-cache-path',str(folder/'cache'),str(ROOT/'clients/ios/Shared/ConnectionTarget.swift'),str(folder/'check.swift'),'-o',str(folder/'check')],check=True,capture_output=True)
   subprocess.run([str(folder/'check')],check=True,capture_output=True)
