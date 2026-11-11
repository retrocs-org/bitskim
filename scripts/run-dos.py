#!/usr/bin/env python3
"""Start a private BitSkim DOS runtime session with NE2000 and mTCP ready."""
import argparse,os,shutil,socket,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def prepare(run,exe,driver,port):
    if any(c in str(run) for c in ('"','\r','\n')):raise ValueError("Choose a test-drive path without quotes or line breaks.")
    run.mkdir(parents=True,exist_ok=True)
    shutil.copy2(exe,run/'BITSKIM.EXE');shutil.copy2(driver,run/'NE2000.COM')
    for folder in ('FILES','INBOX'):(run/folder).mkdir(exist_ok=True)
    demo=run/'FILES/HELLO.TXT'
    if not demo.exists():demo.write_bytes(b'Hello from BitSkim 0.1 for DOS!\r\n')
    (run/'BSNET.CFG').write_bytes(b'PACKETINT 0x60\r\nHOSTNAME BitSkim-DOS\r\nIPADDR 10.0.2.15\r\nNETMASK 255.255.255.0\r\nGATEWAY 10.0.2.2\r\nMTU 1500\r\n')
    boot=['@echo off','NE2000 0x60 3 0x300','if errorlevel 1 goto failed','set MTCPCFG=C:\\BSNET.CFG','set BITSKIM_NAT_HOST=10.0.2.2','set BITSKIM_PORT='+str(port),'BITSKIM','echo.','echo BitSkim has stopped. Type EXIT to close DOS runtime.','goto done',':failed','echo The NE2000 packet driver could not start. Check runner.log.',':done']
    (run/'BSSTART.BAT').write_bytes(('\r\n'.join(boot)+'\r\n').encode('ascii'))
    config=run/'runner.conf'
    config.write_text('[sdl]\nfullscreen=false\noutput=texture\n[mixer]\nnosound=true\n[cpu]\ncycles=12000\n[ethernet]\nne2000=on\nnicbase=300\nnicirq=3\ntcp_port_forwards='+str(port)+':'+str(port)+'\nudp_port_forwards='+str(port)+':46440\n[autoexec]\nmount c "'+str(run)+'"\nc:\ncall BSSTART.BAT\n')
    (run/'START-HERE.txt').write_text('BitSkim DOS runtime test drive\n\nF3: connect to your Mac at 10.0.2.2 (default port 46440).\nOn the Mac: Connect by IP to 127.0.0.1:'+str(port)+' to send to DOS.\nTab switches computer/file panes. Enter opens FILES or INBOX.\nF5 sends a file, F6 accepts an offer, F2 edits preferences, F10 quits.\nChoose INBOX\\NEWFILE.ZIP when accepting into the Inbox folder.\nDiscovery from this host uses the NAT host alias and the forwarded TCP port. Other LANs are not bridged.\nYour DOS settings and files remain here between launches.\nType EXIT at the DOS prompt to close DOS runtime.\n')
    return config

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runner',default=os.environ.get('DOS_RUNNER'));p.add_argument('--packet-driver',default=os.environ.get('DOS_PACKET_DRIVER'))
    p.add_argument('--run-dir',type=Path,default=ROOT/'work/dos-play');p.add_argument('--host-port',type=int,default=46442)
    p.add_argument('--prepare-only',action='store_true');p.add_argument('--headless',action='store_true',help='Use dummy SDL drivers for automated checks')
    a=p.parse_args()
    choices=[a.runner,str(ROOT/'work/dos-tools/runner')]
    binary=next((Path(x).resolve() for x in choices if x and Path(x).is_file()),None)
    driver=Path(a.packet_driver or ROOT/'work/dos-tools/packet/NE2000.COM').resolve()
    exe=ROOT/'build/dos/BITSKIM.EXE'
    if not exe.is_file():p.error('Build the DOS client first; see docs/DOS.md.')
    if not driver.is_file():p.error('Set --packet-driver to your NE2000.COM packet driver. See docs/DOS.md.')
    if not binary:p.error('Install DOS runtime or provide --runner /path/to/runner.')
    if not 1024<=a.host_port<=65535:p.error('Choose a host port from 1024 to 65535.')
    run=a.run_dir.resolve();run.mkdir(parents=True,exist_ok=True)
    lock=(run/'.session.lock').open('a+b')
    try:
        try:
            import fcntl
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:p.error('This BitSkim DOS runtime test drive is already running. Close that session first.')
        except ImportError:pass
        if not a.prepare_only:
            for kind in (socket.SOCK_STREAM,socket.SOCK_DGRAM):
                with socket.socket(socket.AF_INET,kind) as probe:
                    try:probe.bind(('0.0.0.0',a.host_port))
                    except OSError:p.error('Host port is busy; close the other session or use --host-port with another port.')
        config=prepare(run,exe,driver,a.host_port)
        print('BitSkim DOS test drive: '+str(run),flush=True)
        print('DOS -> Mac: F3, 10.0.2.2 (the Mac client must be running).',flush=True)
        print('Mac -> DOS: Connect by IP, 127.0.0.1:'+str(a.host_port),flush=True)
        print('F10 quits BitSkim. Type EXIT at the DOS prompt to close DOS runtime.',flush=True)
        if a.prepare_only:return 0
        env=dict(os.environ)
        for name in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER'):env.pop(name,None)
        if a.headless:env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        # The locally prepared test binary needs Slirp beside it. Installed
        # DOS runtime applications normally provide their own library search path.
        slirp=ROOT/'work/dos-tools/libslirp/4.9.7/lib'
        if slirp.exists():env['DYLD_LIBRARY_PATH']=str(slirp)+(':'+env['DYLD_LIBRARY_PATH'] if env.get('DYLD_LIBRARY_PATH') else '')
        with (run/'runner.log').open('w') as log:
            return subprocess.run([str(binary),'--noprimaryconf','--nolocalconf','--conf',str(config)],env=env,stdout=log,stderr=subprocess.STDOUT).returncode
    finally:lock.close()
if __name__=='__main__':sys.exit(main())
