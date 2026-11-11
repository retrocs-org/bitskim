"""Exercise the actual portable ARexx command parser with a stub client."""
import ctypes
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class ARexxCommands(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        folder = pathlib.Path(cls.tmp.name)
        harness = r'''
#include "bitskim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static BSClient *client;
static BSStatus state;
static int iconified, receiving=1, visible=1, reload_pending, quit_pending;
static unsigned short listen_port=46440;
static char client_name[64]="Local Amiga",wire_name[64];
static unsigned long serial;
static int outcome,peer_count=2;
static BSPeer peers[2];
static char sent_address[48],sent_path[1024];
static unsigned short sent_port;
static int restore_ui(void) {iconified=0; return 1;}
static int iconify_ui(void) {iconified=1; return 1;}
static void launch_preferences(void) {}
int bs_to_utf8(const char *in,char *out,size_t n) {snprintf(out,n,"%s",in);return 1;}
void bs_configure(BSClient *c,const char *n,int v,int r) {}
const BSStatus *bs_status(const BSClient *c) {return &state;}
int bs_peer_count(const BSClient *c) {return peer_count;}
const BSPeer *bs_peer(const BSClient *c,int i) {return i>=0 && i<peer_count?&peers[i]:NULL;}
unsigned long bs_transfer_serial(const BSClient *c) {return serial;}
BSResult bs_transfer_result(const BSClient *c) {return (BSResult)outcome;}
unsigned long bs_transfer_rate(const BSClient *c) {return state.active?12345:0;}
const char *bs_last_error(const BSClient *c) {return "Cannot open file";}
int bs_send(BSClient *c,const char *a,unsigned short port,const char *path) {
 if(!strcmp(path,"missing"))return 0;
 strcpy(sent_address,a);strcpy(sent_path,path);sent_port=port;
 serial++;state.active=1;state.incoming=0;outcome=0;
 snprintf(state.filename,sizeof(state.filename),"%s",path);state.total=1000;state.transferred=0;
 return 1;
}
int bs_send_peer(BSClient *c,const char *id,const char *path) {int i;for(i=0;i<peer_count;i++)if(!strcmp(peers[i].id,id))return bs_send(c,peers[i].address,peers[i].port,path);return 0;}
void bs_local_capabilities(const BSClient *c,BSCapabilities *out) {memset(out,0,sizeof *out);out->max_receive_file=BS_FILE_MAX;out->local_name_bytes=30;out->send_features=28;out->receive_features=28;}
int bs_queue_count(const BSClient *c) {return 0;}
int bs_queue_paused(const BSClient *c) {return 0;}
void bs_queue_continue(BSClient *c) {}
void bs_queue_pause(BSClient *c) {}
void bs_queue_skip(BSClient *c) {}
void bs_queue_clear(BSClient *c) {}
int bs_queue_peer(BSClient *c,const char *id,const char *path) {return bs_send_peer(c,id,path);}
int bs_queue_address(BSClient *c,const char *a,unsigned short port,const char *path) {return bs_send(c,a,port,path);}
void bs_refresh(BSClient *c) {}
void bs_cancel(BSClient *c) {}
'''
        harness += '\n#include "arexx_protocol.h"\n#include "send_target.h"\nstatic GuiSendTarget gui_selection;\n'
        harness += r'''
int command(const char *text,char *out) {char buf[1200]; if(strlen(text)>=sizeof(buf)) return 10; strcpy(buf,text); return rexx_command(buf,out,4096);}
void reset(void) {
 client=(BSClient *)1;memset(&state,0,sizeof(state));serial=outcome=iconified=0;peer_count=2;receiving=visible=1;
 memset(peers,0,sizeof(peers));memset(peers[0].id,'a',32);memset(peers[1].id,'b',32);
 strcpy(peers[0].address,"10.0.2.15");peers[0].port=46440;strcpy(peers[0].name,"Retro Amiga");strcpy(peers[0].platform,"AmigaOS");
 strcpy(peers[1].address,"10.0.2.16");peers[1].port=46441;strcpy(peers[1].name,"Other peer");
 rexx_session_init("000000010000000200000003");
 record_event(BS_EVENT_PEERS);record_event(BS_EVENT_OFFER);record_event(BS_EVENT_COMPLETE);
}
void reorder(void) {BSPeer p=peers[0];peers[0]=peers[1];peers[1]=p;}
void remove_a(void) {if(peers[0].id[0]=='a')peers[0]=peers[1];peer_count=1;}
const char *sent(void) {return sent_address;}
const char *path_sent(void) {return sent_path;}
void progress(unsigned long n) {state.transferred=n;record_event(BS_EVENT_STATUS);}
void finish(int value,const char *error) {outcome=value;state.active=0;snprintf(state.message,sizeof(state.message),"%s",error);record_event(value==1?BS_EVENT_COMPLETE:BS_EVENT_ERROR);}
void overflow(void) {int i;for(i=0;i<40;i++)record_event(BS_EVENT_STATUS);}
void wrap(void) {event_sequence=0xffffffffUL;record_event(BS_EVENT_STATUS);}
void restart(void) {serial=0;memset(&state,0,sizeof(state));rexx_session_init("000000040000000500000006");}
int gui_capture(const char *id) {return gui_target(&gui_selection,id,"10.0.2.2");}
int gui_send_path(const char *path) {return gui_send(client,&gui_selection,path);}
void peer_name(const char *s) {snprintf(peers[0].name,sizeof(peers[0].name),"%s",s);}
'''
        (folder / 'host.c').write_text(harness)
        subprocess.run(['cc', '-shared', '-fPIC', '-I'+str(ROOT/'core'), '-I'+str(ROOT/'clients/amiga'), str(folder/'host.c'), '-o', str(folder/'host.so')], check=True)
        cls.lib = ctypes.CDLL(str(folder/'host.so'))
        cls.lib.command.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        cls.lib.sent.restype=ctypes.c_char_p;cls.lib.path_sent.restype=ctypes.c_char_p
        cls.lib.finish.argtypes=[ctypes.c_int,ctypes.c_char_p];cls.lib.peer_name.argtypes=[ctypes.c_char_p]
        cls.lib.progress.argtypes=[ctypes.c_ulong]
        cls.lib.gui_capture.argtypes=[ctypes.c_char_p];cls.lib.gui_send_path.argtypes=[ctypes.c_char_p]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        self.lib.reset()

    def command(self, text):
        out = ctypes.create_string_buffer(4096)
        rc = self.lib.command(text.encode(), out)
        return rc, out.value.decode()

    def test_network_profile_query(self):
        fields=self.fields('NETWORKCAPS')
        self.assertEqual(fields['VERSION'],'1');self.assertEqual(fields['LOCALNAME'],'30')
        self.assertEqual(fields['MAXRECEIVE'],'2147483647');self.assertEqual(fields['SECURITY'],'0')
    def test_status_and_version(self):
        self.assertEqual(self.command('version'), (0, '0.1'))
        self.assertIn('PORT=46440', self.command('STATUS')[1])
        self.assertEqual(self.command('VERSION extra')[0], 10)

    def test_send_quoted_path_and_port(self):
        self.assertEqual(self.command('SEND 10.0.2.2:46440 "Work:my file.txt"')[0], 0)
        self.lib.reset()
        self.assertEqual(self.command("SEND 10.0.2.2 'Work:my file.txt'")[0], 0)
        self.lib.reset()
        self.assertEqual(self.command('SEND 10.0.2.2:0 "Work:my file.txt"')[0], 10)
        self.assertEqual(self.command('SEND 10.0.2.2:46440junk file')[0], 10)
        self.assertEqual(self.command('SEND 10.0.2.2 "unclosed')[0], 10)
        self.assertEqual(self.command('SEND 10.0.2.2 "Work:my file.txt" extra')[0], 10)
        self.assertEqual(self.command('SEND 10.0.2.2 '+('a'*1024))[0], 10)
        self.assertEqual(self.command('SEND 10.0.2.2 missing')[0], 20)

    def test_peer_bounds(self):
        self.assertEqual(self.command('PEER 0'), (0, '10.0.2.15:46440 Retro Amiga'))
        for value in ('', '-1', '2', '0x', '999999999999999999999'):
            self.assertEqual(self.command('PEER '+value)[0], 10)

    def test_policy_and_window_controls(self):
        self.assertEqual(self.command('RECEIVING OFF')[0], 0)
        self.assertIn('RECEIVING=0', self.command('STATUS')[1])
        self.assertEqual(self.command('RECEIVING ON')[0], 0)
        self.assertEqual(self.command('HIDE')[0], 0)
        self.assertIn('ICONIFIED=1', self.command('STATUS')[1])
        self.assertEqual(self.command('SHOW')[0], 0)
        self.assertIn('ICONIFIED=0', self.command('STATUS')[1])
        self.assertEqual(self.command('VISIBLE maybe')[0], 10)
        self.assertEqual(self.command('DELETE Work:file')[0], 10)

    def test_independent_event_cursors(self):
        first = self.command('EVENTS 1')
        self.assertEqual(first, (0, 'LATEST=3 LOST=0 2:2 3:4'))
        self.assertEqual(self.command('EVENTS 1'), first)
        self.assertEqual(self.command('EVENTS 3'), (0, 'LATEST=3 LOST=0'))
        self.assertEqual(self.command('EVENTS wrong')[0], 10)

    def fields(self, command):
        rc,text=self.command(command);self.assertEqual(rc,0,text)
        return dict(token.split('=',1) for token in text.split())

    def test_capabilities_and_numeric_status_while_iconified(self):
        self.assertEqual(self.fields('CAPABILITIES')['AREXX'],'2')
        self.command('HIDE')
        for cmd in ('STATUS','SESSION','CAPABILITIES','LAST','DETAILS','PEERS','EVENTDETAILS'):
            self.assertEqual(self.command(cmd)[0],0,cmd)
        status=self.fields('STATUS');self.assertEqual(status['BPS'],'0')
        self.assertTrue(all(v.isdecimal() for v in status.values()))
        self.command('SENDID '+'a'*32+' "Work:file"');self.lib.progress(500)
        self.assertEqual(self.fields('STATUS')['BPS'],'12345')
        self.assertEqual(self.fields('STATUS')['BYTES'],'500')

    def test_stable_id_send_survives_reorder_and_missing_never_falls_back(self):
        self.lib.reorder()
        self.assertEqual(self.command('SENDID '+'a'*32+' "Work:file"')[0],0)
        self.assertEqual(self.lib.sent(),b'10.0.2.15')
        self.assertEqual(self.command('SENDID '+'b'*32+' "Work:file"')[0],20)
        self.assertEqual(self.fields('ERROR')['CODE'],'BUSY')
        self.lib.finish(3,b'Cancelled');self.lib.remove_a()
        self.assertEqual(self.command('SENDID '+'a'*32+' "Work:file"')[0],20)
        self.assertEqual(self.fields('ERROR')['CODE'],'UNAVAILABLE')
        self.assertEqual(self.lib.sent(),b'10.0.2.15')
        self.assertEqual(self.fields('PEERINFO '+'b'*32)['ADDRESS'],'10.0.2.16')

    def test_last_result_survives_new_active_transfer_and_errors_roundtrip(self):
        from urllib.parse import unquote_to_bytes
        self.assertEqual(self.fields('LAST'),{'AVAILABLE':'0'})
        self.command('SENDID '+'a'*32+' "Work:file with spaces"');self.lib.progress(321)
        first=self.fields('DETAILS');self.lib.finish(2,'Error "café" 100%'.encode())
        last=self.fields('LAST');self.assertEqual(last['ID'],first['ID'])
        self.assertEqual(last['RESULT'],'2');self.assertEqual(last['BYTES'],'321')
        self.assertEqual(unquote_to_bytes(last['ERROR']),'Error "café" 100%'.encode())
        self.command('SENDID '+'b'*32+' "Work:next"')
        self.assertNotEqual(self.fields('DETAILS')['ID'],first['ID'])
        self.assertEqual(self.fields('LAST'),last)
        self.lib.finish(1,b'Complete');self.assertEqual(self.fields('LAST')['RESULT'],'1')

    def test_encoded_peer_name_and_literal_path_quoting(self):
        from urllib.parse import unquote_to_bytes
        name='Amiga "café" %'.encode();self.lib.peer_name(name)
        self.assertEqual(unquote_to_bytes(self.fields('PEERINFO '+'a'*32)['NAME']),name)
        self.assertEqual(self.command('SENDID '+'a'*32+' "Work:a""b\\c"')[0],0)
        self.assertEqual(self.lib.path_sent(),b'Work:a"b\\c')

    def test_gui_selection_survives_reorder_and_never_uses_manual_fallback(self):
        self.assertEqual(self.lib.gui_capture(b'a'*32),1)
        self.lib.reorder()
        self.assertEqual(self.lib.gui_send_path(b'Work:file'),1)
        self.assertEqual(self.lib.sent(),b'10.0.2.15')
        self.lib.finish(1,b'Done');self.lib.remove_a()
        self.assertEqual(self.lib.gui_send_path(b'Work:another'),0)
        self.assertEqual(self.lib.sent(),b'10.0.2.15')

    def test_event_overflow_restart_wrap_and_outcomes(self):
        initial=self.fields('SESSION')['SESSION']
        self.command('SENDID '+'a'*32+' "Work:file"');self.lib.finish(3,b'Cancelled')
        rc,events=self.command('EVENTDETAILS 3');self.assertEqual(rc,0)
        self.assertIn(self.fields('LAST')['ID']+':3',events)
        self.lib.overflow();self.assertIn('LOST=1',self.command('EVENTDETAILS 1')[1])
        self.assertEqual(len(self.command('EVENTDETAILS 0')[1].split())-3,32)
        self.lib.wrap();self.assertNotEqual(self.fields('SESSION')['SESSION'],initial)
        self.assertIn('LATEST=1 LOST=1',self.command('EVENTDETAILS 4294967295')[1])
        self.lib.restart();self.assertEqual(self.fields('LAST'),{'AVAILABLE':'0'})
        self.assertNotEqual(self.fields('SESSION')['SESSION'],initial)
        self.assertEqual(self.command('EVENTS 4294967296')[0],10)

if __name__ == '__main__':
    unittest.main()
