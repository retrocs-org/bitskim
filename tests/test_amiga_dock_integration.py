"""Real socket engine + actual Amiga ARexx observer/dispatcher; native UI is stubbed."""
import ctypes as C
import os,socket,subprocess,tempfile,time,unittest
from pathlib import Path
from test_core import Config
ROOT=Path(__file__).resolve().parents[1]

class DockEngineIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();folder=Path(cls.tmp.name)
        (folder/'host.c').write_text(r'''
#include "bitskim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
static BSClient *client;
static int iconified,receiving=1,visible=1,reload_pending,quit_pending;
static unsigned short listen_port;
static char client_name[64]="Host",wire_name[64];
static int restore_ui(void) {iconified=0;bs_discovery_search(client,1);return 1;}
static int iconify_ui(void) {iconified=1;bs_discovery_search(client,0);return 1;}
static void launch_preferences(void) {}
#include "arexx_protocol.h"
#include "receive_name.h"
static void note(void *unused,BSEvent e) {(void)unused;record_event(e);}
BSClient *start_host(unsigned short port) {
 BSConfig c;memset(&c,0,sizeof(c));strcpy(c.name,"Host");strcpy(c.platform,"AmigaOS");memset(c.id,'a',32);
 strcpy(c.bind_address,"127.0.0.1");c.port=c.discovery_port=port;c.receiving=c.visible=1;
 client=NULL;rexx_session_init("123456780000000100000002");listen_port=port;iconified=0;
 client=bs_create(&c,note,NULL);bs_batch_enable(client,1);return client;
}
int command(const char *text,char *out) {char buf[1200];if(strlen(text)>=sizeof(buf))return 10;strcpy(buf,text);return rexx_command(buf,out,4096);}
''')
        dialog_source=(ROOT/'clients/amiga/transfer_dialogs.h').read_text()
        controllers=[]
        for begin,end in [('static void dialog_submit(void)','static void begin_send_file'),('static void incoming(void)','static void dialog_tick'),('static void dialog_tick(void)','static void transfer_tools(void)')]:
            start=dialog_source.index(begin);stop=dialog_source.index(end,start) if end else len(dialog_source)
            controllers.append(dialog_source[start:stop])
        with (folder/'host.c').open('a') as f:
            f.write('\n#include "'+str(ROOT/'tests/amiga_dialog_host.h')+'"\n'+ '\n'.join(controllers))
            f.write(r'''
void ui_folder(const char *s) {copy(download,s,sizeof(download));}
int ui_offer(void) {incoming();return transfer_dialog.mode;}
int ui_poll(void) {dialog_tick();return transfer_dialog.w!=NULL;}
void ui_accept(const char *s) {copy(field,s,sizeof(field));pending_message.Class=IDCMP_GADGETUP;pending_message.IAddress=&accept_gadget;queued=1;dialog_tick();}
int ui_replied_safely(void) {return replied==1 && !close_before_reply;}
int ui_send_target(const char *id,const char *manual,const char *path) {GuiSendTarget t;return gui_target(&t,id,manual)?gui_send(client,&t,path):0;}
''')
        subprocess.run(['cc','-shared','-fPIC','-std=c89','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),'-I'+str(ROOT/'clients/amiga'),str(folder/'host.c'),str(ROOT/'core/bitskim.c'),str(ROOT/'core/platform.c'),'-o',str(folder/'host.so')],check=True)
        cls.lib=l=C.CDLL(str(folder/'host.so'));l.bs_platform_init()
        l.start_host.argtypes=[C.c_ushort];l.start_host.restype=C.c_void_p
        l.bs_create.argtypes=[C.POINTER(Config),C.c_void_p,C.c_void_p];l.bs_create.restype=C.c_void_p
        for name in ('bs_destroy','bs_tick','bs_decline'):getattr(l,name).argtypes=[C.c_void_p]
        l.bs_send.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
        l.bs_accept.argtypes=[C.c_void_p,C.c_char_p]
        l.command.argtypes=[C.c_char_p,C.c_char_p]
        l.ui_folder.argtypes=[C.c_char_p];l.ui_accept.argtypes=[C.c_char_p]
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def setUp(self):
        self.files=tempfile.TemporaryDirectory();self.folder=Path(self.files.name);self.clients=[]
        def port():
            with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
        self.hp=port();self.host=self.lib.start_host(self.hp);self.assertTrue(self.host);self.clients.append(self.host)
        cfg=Config(b'Other peer',b'Test',b'b'*32,b'127.0.0.1',port(),1,1)
        self.peer=self.lib.bs_create(C.byref(cfg),None,None);self.assertTrue(self.peer);self.clients.append(self.peer)
    def tearDown(self):
        for c in self.clients:self.lib.bs_destroy(c)
        self.files.cleanup()
    def query(self,cmd):
        out=C.create_string_buffer(4096);self.assertEqual(self.lib.command(cmd.encode(),out),0,out.value)
        return dict(x.split('=',1) for x in out.value.decode().split() if '=' in x)
    def until(self,predicate):
        end=time.monotonic()+5
        while time.monotonic()<end:
            for c in self.clients:self.lib.bs_tick(c)
            if predicate():return
            time.sleep(.0005)
        self.fail('Engine state did not converge: '+str(self.query('STATUS')))
    def offer(self,name,data):
        source=self.folder/name;source.write_bytes(data)
        self.assertEqual(self.lib.bs_send(self.peer,b'127.0.0.1',self.hp,os.fsencode(source)),1)
        self.until(lambda:self.query('STATUS')['AWAITING']=='1')
    def test_modeless_offer_allows_queries_cancel_and_no_late_accept(self):
        self.lib.ui_folder(os.fsencode(self.folder))
        self.offer('pending.bin',b'pending payload')
        self.assertEqual(self.lib.ui_offer(),1)
        self.assertEqual(self.query('STATUS')['AWAITING'],'1')
        self.query('HIDE');self.query('SHOW')
        self.assertEqual(self.query('DETAILS')['RESULT'],'0')
        self.query('CANCEL')
        self.assertEqual(self.query('LAST')['RESULT'],'3')
        self.assertEqual(self.lib.ui_poll(),0)
        self.lib.ui_accept(os.fsencode(self.folder/'must-not-save'))
        self.assertFalse((self.folder/'must-not-save').exists())

    def test_modeless_accept_only_creates_file_after_user_action(self):
        self.lib.ui_folder(os.fsencode(self.folder))
        self.offer('consent.bin',b'only after accept')
        self.assertEqual(self.lib.ui_offer(),1)
        target=self.folder/'chosen'
        self.assertFalse(target.exists())
        self.query('STATUS');self.query('EVENTDETAILS')
        self.lib.ui_accept(os.fsencode(target))
        self.assertEqual(self.lib.ui_replied_safely(),1)
        self.until(lambda:self.query('STATUS')['ACTIVE']=='0')
        self.assertEqual(target.read_bytes(),b'only after accept')
        self.assertEqual(self.query('LAST')['RESULT'],'1')

    def test_actual_callbacks_retain_delivery_then_decline(self):
        data=os.urandom(300000);self.query('HIDE');self.offer('café with spaces.bin',data)
        first=self.query('DETAILS');self.assertEqual(first['INCOMING'],'1')
        self.assertEqual(self.query('LAST'),{'AVAILABLE':'0'})
        dest=self.folder/'received';self.assertEqual(self.lib.bs_accept(self.host,os.fsencode(dest)),1)
        self.until(lambda:int(self.query('STATUS')['BYTES'])>0)
        self.assertEqual(self.query('STATUS')['ICONIFIED'],'1')
        self.assertTrue(all(v.isdecimal() for v in self.query('STATUS').values()))
        self.until(lambda:self.query('STATUS')['ACTIVE']=='0')
        last=self.query('LAST');self.assertEqual(last['RESULT'],'1');self.assertEqual(last['ID'],first['ID'])
        self.assertEqual(int(last['BYTES']),len(data));self.assertEqual(dest.read_bytes(),data)
        # Pump sender through COMPLETE before starting a new offer.
        for _ in range(8):self.lib.bs_tick(self.peer)
        self.offer('next.bin',b'next');self.assertEqual(self.query('LAST'),last)
        self.lib.bs_decline(self.host);self.until(lambda:self.query('STATUS')['ACTIVE']=='0')
        declined=self.query('LAST');self.assertEqual(declined['RESULT'],'4')
        self.assertNotEqual(declined['ID'],last['ID'])


    def test_batch_dialog_accepts_list_and_folder_once(self):
        self.lib.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
        self.lib.bs_queue_count.argtypes=[C.c_void_p]
        self.lib.bs_batch_count.argtypes=[C.c_void_p]
        target=self.folder/'receive';target.mkdir();self.lib.ui_folder(os.fsencode(target))
        for i in range(3):
            source=self.folder/('batch%d.txt'%i);source.write_bytes(bytes([i])*17)
            self.assertEqual(self.lib.bs_queue_address(self.peer,b'127.0.0.1',self.hp,os.fsencode(source)),1)
        self.until(lambda:self.query('STATUS')['AWAITING']=='1')
        self.assertEqual(self.lib.bs_batch_count(self.host),3)
        self.assertEqual(self.lib.ui_offer(),4)
        self.assertFalse(list(target.iterdir()))
        self.lib.ui_accept(os.fsencode(target));self.assertEqual(self.lib.ui_replied_safely(),1)
        def finished():
            self.assertEqual(self.query('STATUS')['AWAITING'],'0')
            return self.lib.bs_queue_count(self.peer)==0 and self.query('STATUS')['ACTIVE']=='0'
        self.until(finished)
        for i in range(3):self.assertEqual((target/('batch%d.txt'%i)).read_bytes(),bytes([i])*17)

if __name__=='__main__':unittest.main()
