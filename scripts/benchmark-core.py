#!/usr/bin/env python3
"""Compare single ticks and bounded service over loopback; not a native UI benchmark."""
import argparse,ctypes as C,hashlib,json,os,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from test_core import CoreTests

def run(fixture,mode,interval,data):
 fixture.setUp()
 try:
  lib=fixture.lib;lib.bs_service.argtypes=[C.c_void_p,C.c_ulong]
  fixture.start(data);destination=fixture.root/'received'
  assert lib.bs_accept(fixture.b,os.fsencode(destination))==1
  start=time.monotonic();cpu=time.process_time();ticks=0
  while fixture.status(fixture.a).active or fixture.status(fixture.b).active:
   for client in (fixture.a,fixture.b):
    if mode=='single-tick':lib.bs_tick(client)
    else:lib.bs_service(client,4)
   ticks+=1
   if time.monotonic()-start>120:raise RuntimeError('Transfer timed out')
   if fixture.status(fixture.a).active or fixture.status(fixture.b).active:time.sleep(interval)
  elapsed=time.monotonic()-start;cpu=time.process_time()-cpu
  assert lib.bs_transfer_result(fixture.a)==1
  assert hashlib.sha256(destination.read_bytes()).digest()==hashlib.sha256(data).digest()
  return dict(mode=mode,interval_ms=int(interval*1000),bytes=len(data),seconds=round(elapsed,4),cpu_seconds=round(cpu,4),mib_per_second=round(len(data)/1048576/elapsed,3),iterations=ticks,verified=True)
 finally:fixture.tearDown()

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mib',type=int,default=8);parser.add_argument('--tls',action='store_true');args=parser.parse_args()
 if not 1<=args.mib<=1024:parser.error('--mib must be 1..1024')
 if args.tls:
  from test_secure_resume import ModernTests
  fixture_type=ModernTests
 else:fixture_type=CoreTests
 fixture_type.setUpClass();fixture=fixture_type('runTest');data=os.urandom(args.mib*1048576)
 print(json.dumps(dict(transport='TLS' if args.tls else 'plaintext',scope='host loopback engine; not native UI or real LAN',results=[run(fixture,mode,interval,data) for interval in (.02,.05) for mode in ('single-tick','bounded-service')]),indent=2))
if __name__=='__main__':main()
