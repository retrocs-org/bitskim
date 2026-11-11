#!/usr/bin/env python3
"""Private DOS runtime smoke/file check and optional real mTCP-to-host core transfer.
Requires a DOS runtime binary and a separately acquired packet driver.
"""
import argparse,ctypes as C,importlib.util,os,shutil,socket,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runner',required=True);p.add_argument('--packet-driver');a=p.parse_args()
run=ROOT/'work/dos-runtime';run.mkdir(parents=True,exist_ok=True)
for name in ('CHECK.TXT','SEND.TXT','OK.TXT','BAD.TXT','BSCHK1.TMP','BSCHK2.TMP','BSCHK3.TMP','RECEIVED.BIN','OUTPUT.BIN','FIRST.TXT','RECV.TXT','BITSKIM.CFG'):
 path=run/name
 if path.exists():path.unlink()
shutil.copy2(ROOT/'build/dos/BITSKIM.EXE',run/'BITSKIM.EXE')
batch=['@echo off','bitskim /CHECK > CHECK.TXT','if errorlevel 1 goto bad']
client=None;lib=None;payload=bytes(range(256))*257;destination=run/'RECEIVED.BIN'
if a.packet_driver:
 spec=importlib.util.spec_from_file_location('host_core',ROOT/'tests/test_core.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 m.CoreTests.setUpClass();lib=m.CoreTests.lib
 s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
 s=socket.socket();s.bind(('127.0.0.1',0));forward=s.getsockname()[1];s.close()
 conf=m.Config(b'Host integration test',b'Test',b'1234567890abcdef1234567890abcdef',b'127.0.0.1',port,0,1)
 client=lib.bs_create(C.byref(conf),None,None)
 if not client:raise SystemExit('Cannot open host test receiver')
 shutil.copy2(a.packet_driver,run/'NE2000.COM');(run/'DATA.BIN').write_bytes(payload)
 (run/'BACK.BIN').write_bytes(payload[::-1])
 (run/'MTCP.CFG').write_text('PACKETINT 0x60\nHOSTNAME BitSkim-DOS\nIPADDR 10.0.2.15\nNETMASK 255.255.255.0\nGATEWAY 10.0.2.2\nMTU 1500\n')
 batch+=['NE2000 0x60 3 0x300 > DRIVER.TXT','if errorlevel 1 goto bad','set MTCPCFG=C:\\MTCP.CFG',f'bitskim /SEND 10.0.2.2:{port} DATA.BIN > SEND.TXT','if errorlevel 1 goto bad','echo success > FIRST.TXT','bitskim /RECEIVE OUTPUT.BIN > RECV.TXT','if errorlevel 1 goto bad']
batch+=['echo success > OK.TXT','goto done',':bad','echo failed > BAD.TXT',':done','exit']
(run/'RUN.BAT').write_bytes(('\r\n'.join(batch)+'\r\n').encode())
config=run/'run.conf';config.write_text('[sdl]\noutput=texture\n[mixer]\nnosound=true\n[cpu]\ncycles=max\n[ethernet]\nne2000='+('on' if client else 'off')+('\ntcp_port_forwards='+str(forward)+':46440\nudp_port_forwards='+str(forward)+':46440' if client else '')+'\n[autoexec]\nmount c "'+str(run)+'"\nc:\ncall run.bat\nexit\n')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy');accepted=False;sent_back=False;probe=None;discovered=False;last_probe=0;first_seen=None;offer_time=None
if client:
 probe=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);probe.bind(('127.0.0.1',0));probe.setblocking(False)
try:
 with (run/'test system.log').open('w') as log:
  process=subprocess.Popen([str(Path(a.runner).resolve()),'--noprimaryconf','--nolocalconf','--conf',str(config)],env=env,stdout=log,stderr=subprocess.STDOUT)
  deadline=time.monotonic()+60
  try:
   while process.poll() is None and time.monotonic()<deadline:
    if client:
     lib.bs_tick(client);status=lib.bs_status(client).contents
     if status.awaiting_decision and offer_time is None:offer_time=time.monotonic()
     if status.awaiting_decision and not accepted and time.monotonic()-offer_time>2:
      assert lib.bs_accept(client,os.fsencode(destination));accepted=True
     if (run/'FIRST.TXT').exists() and first_seen is None:first_seen=time.monotonic()
     if accepted and not status.active and not sent_back and first_seen and time.monotonic()-first_seen>3:
      assert lib.bs_send(client,b'127.0.0.1',forward,os.fsencode(run/'BACK.BIN'));sent_back=True
    if probe and not discovered:
     if time.monotonic()-last_probe>.2:
      probe.sendto(b'BSKD\x01\x01'+m.hello(),('127.0.0.1',forward));last_probe=time.monotonic()
     try:
      reply,_=probe.recvfrom(512)
      discovered=reply.startswith(b'BSKD\x01\x02') and b'MS-DOS' in reply
     except BlockingIOError:pass
    time.sleep(.001)
   if process.poll() is None:raise RuntimeError('DOS runtime check timed out')
  finally:
   if process.poll() is None:process.terminate();process.wait(timeout=5)
 if (run/'BAD.TXT').exists() or not (run/'OK.TXT').exists():raise RuntimeError('DOS checks failed; see work/dos-runtime logs')
 assert 'PASS' in (run/'CHECK.TXT').read_text(), 'DOS file check failed'
 print((run/'CHECK.TXT').read_text().strip())
 if client:
  assert destination.read_bytes()==payload,'Transferred bytes differ'
  assert discovered,'DOS did not answer UDP discovery'
  assert lib.bs_transfer_result(client)==1,'Host did not verify completion'
  assert (run/'OUTPUT.BIN').read_bytes()==payload[::-1], 'DOS received bytes differ'
  print('PASS: DOS mTCP <-> host C engine, 65,792 binary bytes verified each direction, UDP discovery reply verified')
finally:
 if probe:probe.close()
 if client:lib.bs_destroy(client);lib.bs_platform_cleanup()
