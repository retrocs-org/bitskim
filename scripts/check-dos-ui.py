#!/usr/bin/env python3
"""Exercise the real DOS UI through private DOS runtime BIOS keyboard/video memory.
The localhost-only debug API is enabled only in this disposable test profile.
Requires Pillow for a preview of the actual text screen using its BIOS font.
"""
import re,argparse,ctypes as C,importlib.util,os,shutil,socket,struct,subprocess,time,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def port():
    with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--runner',required=True);p.add_argument('--packet-driver',required=True);a=p.parse_args()
    run=ROOT/'work/dos-ui-check';run.mkdir(exist_ok=True)
    for name in ('BITSKIM.CFG','RECEIVED.BIN','FROMHOST.BIN','TIMEOUT.TXT'):
        f=run/name
        if f.exists():f.unlink()
    (run/'FILES').mkdir(exist_ok=True)
    for name in ('TREEIN','EMPTYIN','FROMDOS','FILES/TREEIN','FILES/EMPTYIN'):
        path=run/name
        if path.is_dir():shutil.rmtree(path)
    for old in (run/'FILES').glob('BATCH*.TXT'):old.unlink()
    for name in ('BITSKIM.CFG','FROMHOST.BIN','QUEUE1.BIN','QUEUE2.BIN'):
        f=run/'FILES'/name
        if f.exists():f.unlink()
    (run/'FILES/TAG2.TXT').write_bytes(b'Second tagged file\r\n')
    (run/'FILES/HELLO.TXT').write_bytes(b'Hello from the DOS text interface!\r\n')
    shutil.copy2(ROOT/'build/dos/BITSKIM.EXE',run/'BITSKIM.EXE');shutil.copy2(a.packet_driver,run/'NE2000.COM')
    # Query the VGA 8x16 font through BIOS and write it to redirected stdout.
    (run/'FONT.COM').write_bytes(bytes.fromhex('b83011b706cd1089ea8cc08ed8bb0100b90010b440cd21b8004ccd21'))
    # BIOS palette snapshot utility, so exiting must restore the actual VGA DAC.
    probe=bytearray.fromhex('0e1f0e07b81710bb0000b90001ba0000cd10bb0100b90003ba0000b440cd21b8004ccd21')
    offset=256+len(probe);struct.pack_into('<H',probe,14,offset);struct.pack_into('<H',probe,25,offset)
    (run/'PALETTE.COM').write_bytes(probe+bytes(768))
    (run/'MTCP.CFG').write_text('PACKETINT 0x60\nHOSTNAME BitSkim-DOS\nIPADDR 10.0.2.15\nNETMASK 255.255.255.0\nGATEWAY 10.0.2.2\nMTU 1500\n')
    api,forward,hostport=port(),port(),port()
    config=run/'ui.conf';config.write_text('[sdl]\noutput=texture\n[mixer]\nnosound=true\n[cpu]\ncore=normal\ncycles=12000\n[ethernet]\nne2000=on\nnicbase=300\nnicirq=3\ntcp_port_forwards='+str(forward)+':'+str(forward)+'\nudp_port_forwards='+str(forward)+':46440\n[webserver]\nwebserver_enabled=on\nwebserver_bind_address=127.0.0.1\nwebserver_port='+str(api)+'\n[autoexec]\nmount c "'+str(run)+'"\nc:\nFONT.COM > FONT.BIN\nPALETTE.COM > BEFORE.PAL\nNE2000 0x60 3 0x300 > DRIVER.TXT\nset MTCPCFG=C:\\MTCP.CFG\nset BITSKIM_NAT_HOST=10.0.2.2\nset BITSKIM_PORT='+str(forward)+'\nBITSKIM\nC:\\PALETTE.COM > C:\\AFTER.PAL\nexit\n')
    spec=importlib.util.spec_from_file_location('host_core',ROOT/'tests/test_core.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.CoreTests.setUpClass();lib=m.CoreTests.lib
    cfg=m.Config(b'Host test peer',b'Test',b'1234567890abcdef1234567890abcdef',b'127.0.0.1',hostport,0,1);client=lib.bs_create(C.byref(cfg),None,None);assert client
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    slirp=ROOT/'work/dos-tools/libslirp/4.9.7/lib'
    if slirp.exists():env['DYLD_LIBRARY_PATH']=str(slirp)+(':'+env['DYLD_LIBRARY_PATH'] if env.get('DYLD_LIBRARY_PATH') else '')
    log=(run/'test system.log').open('w');process=subprocess.Popen([str(Path(a.runner).resolve()),'--noprimaryconf','--nolocalconf','--conf',str(config)],env=env,stdout=log,stderr=subprocess.STDOUT)
    base='http://127.0.0.1:'+str(api)+'/api/v1/memory/'
    def memory(address,size):return urllib.request.urlopen(base+hex(address)+'/'+str(size),timeout=.5).read()
    def write(address,data):
        request=urllib.request.Request(base+hex(address),data=data,method='PUT',headers={'Content-Type':'application/octet-stream'})
        urllib.request.urlopen(request,timeout=1).close()
    def screen():return ''.join(chr(b) for b in memory(0xb8000,4000)[::2])
    def wait(test,seconds=12):
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            lib.bs_tick(client)
            try:
                if test():return
            except (OSError,urllib.error.URLError):pass
            time.sleep(.005)
        try:(run/'TIMEOUT.TXT').write_text('\n'.join(screen()[i*80:(i+1)*80] for i in range(25)))
        except Exception:pass
        raise AssertionError('DOS UI timed out; host status='+repr(lib.bs_status(client).contents.message)+'; see work/dos-ui-check logs')
    def key(value):
        wait(lambda:process.poll() is not None or struct.unpack('<HH',memory(0x41a,4))[0]==struct.unpack('<HH',memory(0x41a,4))[1])
        head,tail=struct.unpack('<HH',memory(0x41a,4));write(0x400+tail,struct.pack('<H',value));write(0x41c,struct.pack('<H',0x1e if tail+2>=0x3e else tail+2))
        wait(lambda:process.poll() is not None or struct.unpack('<HH',memory(0x41a,4))[0]==struct.unpack('<HH',memory(0x41a,4))[1])
        for _ in range(8):lib.bs_tick(client);time.sleep(.005)
    def text(value):
        for c in value:key(ord(c))
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    udp.bind(('127.0.0.1',0));udp.setblocking(False)
    try:
        wait(lambda:'NEARBY COMPUTERS' in screen())
        greeting=bytes.fromhex('1234567890abcdef1234567890abcdef')+struct.pack('>HBB',hostport,14,4)+b'Host test peerTest'
        last=[0];dos_reply=[None]
        def discovered():
            if time.monotonic()-last[0]>.2:udp.sendto(b'BSKD\x01\x01'+greeting,('127.0.0.1',forward));last[0]=time.monotonic()
            try:
                reply,_=udp.recvfrom(512)
                if reply.startswith(b'BSKD\x01\x02') and b'MS-DOS' in reply:dos_reply[0]=reply
            except BlockingIOError:pass
            return 'Host test peer' in screen() and dos_reply[0] is not None
        wait(discovered)
        assert struct.unpack('>H',dos_reply[0][22:24])[0]==forward,'DOS must advertise the forwarded TCP port'
        assert '10.0.2.2' in screen(),'DOS must map the forwarded host loopback address'
        # Feed the actual DOS discovery reply to the host engine, as a Mac
        # client receives it; its tile must resolve to host loopback:forward.
        udp.sendto(dos_reply[0],('127.0.0.1',hostport));dos_id=dos_reply[0][6:22].hex().encode()
        wait(lambda:any(lib.bs_peer(client,i).contents.id==dos_id for i in range(lib.bs_peer_count(client))))
        # Choose the test peer explicitly; other running clients may also reply.
        rows=[screen()[i*80:(i+1)*80] for i in range(25)]
        peer_row=next((i-6)//3 for i in (6,9,12,15) if 'Host test peer' in rows[i])
        key(0x4700)
        for _ in range(peer_row):key(0x5000)
        key(0x3b00);wait(lambda:' HELP ' in screen());text('t');wait(lambda:'TRANSFER TOOLS' in screen());assert 'Stack: mTCP' in screen();text('h');wait(lambda:'No transfer history.' in screen());text('c');wait(lambda:'Conflicts: SKIP' in screen());text('c');wait(lambda:'Conflicts: REPLACE' in screen());text('c');wait(lambda:'Conflicts: RENAME' in screen());key(27)
        key(0x4300);wait(lambda:'R: retry/continue' in screen());key(27)
        key(9);key(0x4700);
        rows=[screen()[i*80:(i+1)*80] for i in range(25)];folder_row=next(i-7 for i in range(7,19) if 'FILES' in rows[i][11:] and '<DIR>' in rows[i][11:])
        for _ in range(folder_row):key(0x5000)
        key(13);wait(lambda:'C:\\FILES' in screen());key(0x5000)
        # Preferences stay tied to the launch directory after browsing.
        key(0x3c00);wait(lambda:'PREFERENCES' in screen())
        for _ in range(len('BitSkim-DOS')):key(8)
        text('Demo DOS');key(13);wait(lambda:'Demo DOS' in screen())
        assert 'NAME=Demo DOS' in (run/'BITSKIM.CFG').read_text();assert not (run/'FILES/BITSKIM.CFG').exists()
        # Capture actual video RAM and the BIOS font before network transfers.
        # A BIOS/REP copy can be sampled halfway through a screen update.
        key(9);wait(lambda:all(word in screen() for word in ('NEARBY COMPUTERS','READY TO SEND','Preferences saved.')))
        raw=memory(0xb8000,4000)
        for _ in range(20):
            current=memory(0xb8000,4000)
            if current==raw:break
            raw=current;time.sleep(.03)
        (run/'SCREEN.BIN').write_bytes(raw)
        font=(run/'FONT.BIN').read_bytes();assert len(font)==4096
        from PIL import Image
        colors=(ROOT/'clients/dos/tui.h').read_text().split('ui_colors[16][3]={',1)[1].split('};',1)[0]
        palette=[tuple(int(v)*255//63 for v in rgb) for rgb in re.findall(r'\{(\d+),(\d+),(\d+)\}',colors)]
        assert len(palette)==16
        def render(raw,destination):
            image=Image.new('RGB',(720,400))
            for cell in range(2000):
                char,attr=raw[cell*2:cell*2+2];x=(cell%80)*9;y=(cell//80)*16
                for row in range(16):
                    bits=font[char*16+row]
                    for col in range(9):
                        on=bits&(128>>col) if col<8 else (bits&1 if 0xc0<=char<=0xdf else 0)
                        image.putpixel((x+col,y+row),palette[attr&15 if on else (attr>>4)&7])
            image.resize((1440,800),Image.Resampling.NEAREST).save(destination)
        preview=ROOT/'build/dos/UI-PREVIEW.png';render(raw,preview)
        key(9);wait(lambda:' LOCAL FILES ' in screen());render(memory(0xb8000,4000),ROOT/'build/dos/FILEBROWSER-PREVIEW.png')
        # F5 sends the selected local file to the selected stable ID.
        key(0x3f00);wait(lambda:' SEND FILE ' in screen());key(13)
        wait(lambda:lib.bs_status(client).contents.awaiting_decision);assert lib.bs_accept(client,os.fsencode(run/'RECEIVED.BIN'))
        wait(lambda:lib.bs_transfer_result(client)==1 and not lib.bs_status(client).contents.active)
        assert (run/'RECEIVED.BIN').read_bytes()==(run/'FILES/HELLO.TXT').read_bytes()
        wait(lambda:'waiting | F9 controls' in screen())
        # An open preferences panel must not stop incoming offer processing.
        key(0x3c00);text(' Draft');payload=bytes(range(256))*65;(run/'FROMHOST.BIN').write_bytes(payload)
        assert lib.bs_send_peer(client,dos_id,os.fsencode(run/'FROMHOST.BIN'))
        wait(lambda:' INCOMING FILE ' in screen());assert 'From: Host test peer' in screen();assert '16640 bytes' in screen()
        (run/'OFFER.BIN').write_bytes(memory(0xb8000,4000))
        key(13);wait(lambda:' ACCEPT INCOMING FILE ' in screen());key(13)
        wait(lambda:lib.bs_transfer_result(client)==1 and not lib.bs_status(client).contents.active)
        assert (run/'FILES/FROMHOST.BIN').read_bytes()==payload
        wait(lambda:'PREFERENCES' in screen());assert 'Demo DOS Draft' in screen();assert 'NAME=Demo DOS\n' in (run/'BITSKIM.CFG').read_text();key(27)
        # A second offer may be declined from its automatic popup.
        wait(lambda:'waiting | F9 controls' in screen())
        assert lib.bs_send_peer(client,dos_id,os.fsencode(run/'FROMHOST.BIN'))
        wait(lambda:' INCOMING FILE ' in screen());key(9);key(13)
        wait(lambda:lib.bs_transfer_result(client)==4 and not lib.bs_status(client).contents.active)
        assert (run/'FILES/FROMHOST.BIN').read_bytes()==payload
        wait(lambda:'waiting | F9 controls' in screen())
        # A sender cancellation also removes the pending decision popup.
        assert lib.bs_send_peer(client,dos_id,os.fsencode(run/'FROMHOST.BIN'))
        wait(lambda:' INCOMING FILE ' in screen());lib.bs_cancel(client)
        wait(lambda:' INCOMING FILE ' not in screen() and 'waiting | F9 controls' in screen())
        # Tag two files through the real UI and verify both sequential offers.
        key(9);wait(lambda:' LOCAL FILES ' in screen());key(0x4700)
        rows=[screen()[i*80:(i+1)*80] for i in range(25)]
        first=next(i-7 for i in range(7,19) if 'HELLO.TXT' in rows[i][11:])
        second=next(i-7 for i in range(7,19) if 'TAG2.TXT' in rows[i][11:])
        for _ in range(first):key(0x5000)
        key(32)
        for _ in range(second-first):key(0x5000)
        key(32);key(0x3f00)
        for n,expected in enumerate([b'Hello from the DOS text interface!\r\n',b'Second tagged file\r\n'],1):
            wait(lambda:lib.bs_status(client).contents.awaiting_decision!=0)
            destination=run/'FILES'/('QUEUE%d.BIN'%n)
            assert lib.bs_accept(client,os.fsencode(destination))
            wait(lambda:destination.exists() and not lib.bs_status(client).contents.active)
            assert destination.read_bytes()==expected
        wait(lambda:'0 waiting | F9 controls' in screen())
        # A complete list larger than one screen needs only one folder approval.
        lib.bs_queue_peer.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p]
        lib.bs_queue_count.argtypes=[C.c_void_p]
        for n in range(1,13):
            source=run/('BATCH%02d.TXT'%n);source.write_bytes(('Collection file %d'%n).encode())
            assert lib.bs_queue_peer(client,dos_id,os.fsencode(source))
        wait(lambda:' INCOMING BATCH ' in screen());assert 'BATCH01.TXT' in screen()
        key(0x5100);wait(lambda:'BATCH09.TXT' in screen());key(0x4900);wait(lambda:'BATCH01.TXT' in screen())
        key(13);wait(lambda:' ACCEPT INCOMING FILE ' in screen());key(13)
        wait(lambda:lib.bs_queue_count(client)==0 and not lib.bs_status(client).contents.active,seconds=25)
        for n in range(1,13):assert (run/'FILES'/('BATCH%02d.TXT'%n)).read_bytes()==('Collection file %d'%n).encode()
        assert ' INCOMING BATCH ' not in screen()
        # New folder protocol: hierarchy and empty directories, one approval.
        source=run/'TREEIN';(source/'NEST'/'EMPTY').mkdir(parents=True);(source/'A.TXT').write_bytes(b'folder payload');(source/'NEST'/'ZERO.BIN').write_bytes(b'')
        assert lib.bs_queue_peer(client,dos_id,os.fsencode(source))
        wait(lambda:' INCOMING BATCH ' in screen());assert 'TREEIN/' in screen()
        key(0x5100);wait(lambda:' INCOMING BATCH ' in screen());key(0x4900)
        key(13);wait(lambda:' ACCEPT INCOMING FILE ' in screen());assert 'Need about' in screen();key(13)
        wait(lambda:lib.bs_queue_count(client)==0 and not lib.bs_status(client).contents.active,seconds=25)
        assert (run/'FILES/TREEIN/NEST/EMPTY').is_dir();assert (run/'FILES/TREEIN/A.TXT').read_bytes()==b'folder payload';assert (run/'FILES/TREEIN/NEST/ZERO.BIN').read_bytes()==b''
        # A single empty directory must still use collection/folder approval.
        empty=run/'EMPTYIN';empty.mkdir();assert lib.bs_queue_peer(client,dos_id,os.fsencode(empty))
        wait(lambda:' INCOMING BATCH ' in screen());key(13);wait(lambda:' ACCEPT INCOMING FILE ' in screen());key(13)
        wait(lambda:lib.bs_queue_count(client)==0 and not lib.bs_status(client).contents.active);assert (run/'FILES/EMPTYIN').is_dir()
        # Send a selected DOS folder with F5, while Enter remains navigation.
        wait(lambda:'0 waiting | F9 controls' in screen());key(9);wait(lambda:' LOCAL FILES ' in screen());key(0x4700)
        rows=[screen()[i*80:(i+1)*80] for i in range(25)];index=next(i-7 for i in range(7,19) if 'TREEIN' in rows[i][11:] and '<DIR>' in rows[i][11:])
        for _ in range(index):key(0x5000)
        lib.bs_batch_enable.argtypes=[C.c_void_p,C.c_int];lib.bs_batch_is_tree.argtypes=[C.c_void_p];lib.bs_accept_batch.argtypes=[C.c_void_p,C.c_char_p];lib.bs_batch_enable(client,1)
        key(0x3f00);wait(lambda:' SEND FILE ' in screen());assert 'TREEIN' in screen();key(13)
        wait(lambda:lib.bs_status(client).contents.awaiting_decision);assert lib.bs_batch_is_tree(client)
        target=run/'FROMDOS';target.mkdir();assert lib.bs_accept_batch(client,os.fsencode(target))
        wait(lambda:(target/'TREEIN/A.TXT').exists() and not lib.bs_status(client).contents.active,seconds=25)
        assert (target/'TREEIN/NEST/EMPTY').is_dir();assert (target/'TREEIN/A.TXT').read_bytes()==b'folder payload'
        wait(lambda:'0 waiting | F9 controls' in screen())
        key(0x4400);process.wait(timeout=8);assert process.returncode==0
        assert len((run/'BEFORE.PAL').read_bytes())==768 and (run/'BEFORE.PAL').read_bytes()==(run/'AFTER.PAL').read_bytes(),'DOS UI must restore the original VGA palette'
        print('PASS: VGA palette restoration; dashboard/browser navigation; native DOS folder send/receive, single empty-folder approval, disk-space preview, hierarchy preservation; actual DOS UI tools/diagnostics/history/conflict policy, navigation, help, preferences persistence, F5 send, two-file Space-tagged FIFO queue, F9 queue modal, automatic incoming popup during preferences, accept/save, draft restoration, decline, sender cancellation, 12-file streamed collection with paging and one folder approval, F10 quit; NAT discovery addresses and ports verified in both directions')
        print('Actual text-screen preview: '+str(preview))
    finally:
        udp.close();lib.bs_destroy(client);lib.bs_platform_cleanup()
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        log.close()
if __name__=='__main__':main()
