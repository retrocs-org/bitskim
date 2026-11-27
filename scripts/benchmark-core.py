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

def run_batch(fixture,reuse,interval,count,size):
 fixture.setUp()
 try:
  lib=fixture.lib
  for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_session_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],'bs_service':[C.c_void_p,C.c_ulong]}.items():getattr(lib,name).argtypes=args
  for client in (fixture.a,fixture.b):lib.bs_batch_enable(client,1)
  lib.bs_session_enable(fixture.a,1);lib.bs_session_enable(fixture.b,int(reuse))
  destination=fixture.root/'batch';destination.mkdir();expected={}
  for i in range(count):
   name='small%04d.bin'%i;data=os.urandom(size);expected[name]=hashlib.sha256(data).digest();path=fixture.root/name;path.write_bytes(data)
   assert lib.bs_queue_address(fixture.a,b'127.0.0.1',fixture.pb,os.fsencode(path))==1
  start=time.monotonic();cpu=time.process_time();approved=False;iterations=0
  while lib.bs_queue_count(fixture.a) or fixture.status(fixture.a).active or fixture.status(fixture.b).active:
   for client in (fixture.a,fixture.b):lib.bs_service(client,4)
   if fixture.status(fixture.b).awaiting_decision:
    assert not approved,'Collection requested a second approval'
    assert lib.bs_accept_batch(fixture.b,os.fsencode(destination))==1;approved=True
   if time.monotonic()-start>120:raise RuntimeError('Batch timed out')
   iterations+=1;time.sleep(interval)
  elapsed=time.monotonic()-start;cpu=time.process_time()-cpu
  assert approved and lib.bs_transfer_result(fixture.a)==1
  for name,digest in expected.items():assert hashlib.sha256((destination/name).read_bytes()).digest()==digest
  return dict(session_reuse=reuse,files=count,file_bytes=size,interval_ms=int(interval*1000),connections=lib.bs_test_connect_count(),seconds=round(elapsed,4),cpu_seconds=round(cpu,4),files_per_second=round(count/elapsed,3),iterations=iterations,verified=True)
 finally:fixture.tearDown()

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mib',type=int,default=8);parser.add_argument('--tls',action='store_true');parser.add_argument('--batch-files',type=int,default=0);parser.add_argument('--batch-bytes',type=int,default=1024);args=parser.parse_args()
 if not 1<=args.mib<=1024:parser.error('--mib must be 1..1024')
 if not 0<=args.batch_files<=1000 or not 1<=args.batch_bytes<=1048576:parser.error('Batch limits: 0..1000 files, 1..1048576 bytes each')
 if args.tls:
  from test_secure_resume import ModernTests
  fixture_type=ModernTests
 else:fixture_type=CoreTests
 fixture_type.setUpClass();fixture=fixture_type('runTest');data=os.urandom(args.mib*1048576)
 batch_results=[run_batch(fixture,reuse,.02,args.batch_files,args.batch_bytes) for reuse in (False,True)] if args.batch_files else []
 print(json.dumps(dict(batch_results=batch_results,transport='TLS' if args.tls else 'plaintext',scope='host loopback engine; not native UI or real LAN',results=[run(fixture,mode,interval,data) for interval in (.02,.05) for mode in ('single-tick','bounded-service')]),indent=2))
if __name__=='__main__':main()
