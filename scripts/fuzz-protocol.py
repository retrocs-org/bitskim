#!/usr/bin/env python3
"""Run bounded production-parser fuzzing with AddressSanitizer and UBSan."""
import argparse,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--seconds',type=int,default=30);a=p.parse_args()
if not 1<=a.seconds<=3600:p.error('--seconds must be 1..3600')
out=ROOT/'build/fuzz';corpus=out/'corpus';corpus.mkdir(parents=True,exist_ok=True)
def frame(t,b):return bytes([t])+struct.pack('>H',len(b))+b
hello=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX40'
for name,data in {'hello':frame(1,hello),'offer':frame(1,hello)+frame(2,struct.pack('>QH',1,1)+b'x'),'collection':frame(1,hello)+frame(16,b'x'*16+struct.pack('>II',0,2))+frame(17,struct.pack('>QH',1,1)+b'x'),'invalid':b'\xff'*20}.items():(corpus/name).write_bytes(data)
compiler=os.environ.get('CC', '/opt/homebrew/opt/llvm/bin/clang' if Path('/opt/homebrew/opt/llvm/bin/clang').is_file() else 'clang')
subprocess.run([compiler,'-std=c89','-g','-O1','-fno-omit-frame-pointer','-fsanitize=fuzzer,address,undefined','tests/fuzz_protocol.c','core/platform.c','-o',str(out/'protocol')],cwd=ROOT,check=True)
subprocess.run([str(out/'protocol'),str(corpus),'-max_len=65536','-timeout=5','-rss_limit_mb=512','-max_total_time='+str(a.seconds),'-artifact_prefix='+str(out)+'/'],cwd=ROOT,check=True)
