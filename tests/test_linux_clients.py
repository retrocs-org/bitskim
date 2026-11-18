"""Real Linux client processes: TLS, folders, saved work and native GTK controls."""
import json,os,platform,select,shlex,shutil,socket,stat,struct,subprocess,sys,tempfile,time,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ARCH={'aarch64':'arm64','arm64':'arm64','x86_64':'x86_64'}.get(platform.machine(),'unknown')
BIN=ROOT/'build/linux'/os.environ.get('BITSKIM_LINUX_TEST_ARCH',ARCH)
RUNNER=shlex.split(os.environ.get('BITSKIM_LINUX_TEST_RUNNER',''))
@unittest.skipUnless(sys.platform=='linux' and (BIN/'bitskim-cli').is_file(),'Built Linux client required')
class LinuxClients(unittest.TestCase):
 def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.processes=[]
 def tearDown(self):
  for p in self.processes:
   if p.poll() is None:p.terminate()
  for p in self.processes:
   try:p.communicate(timeout=4)
   except subprocess.TimeoutExpired:p.kill();p.communicate()
  self.temp.cleanup()
 def port(self):
  with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
 def command(self,profile,port,*args):return RUNNER+[str(BIN/'bitskim-cli'),'--profile',str(self.root/profile),'--bind','127.0.0.1','--port',str(port),'--no-discovery',*args]
 def run_cli(self,profile,port,*args,check=True):
  p=subprocess.run(self.command(profile,port,*args),capture_output=True,text=True,timeout=25)
  if check:self.assertEqual(p.returncode,0,p.stdout+p.stderr)
  return p
 def receiver(self,port=None,metadata=False,invitation=False):
  port=port or self.port();directory=self.root/'received';directory.mkdir(exist_ok=True)
  args=['receive','--directory',str(directory),'--accept','--once','--timeout','20','--json']
  if metadata:args+=['--metadata']
  if invitation:args+=['--invite-address','127.0.0.1']
  p=subprocess.Popen(self.command('receiver',port,*args),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1);self.processes.append(p)
  lines=[];deadline=time.monotonic()+8
  while time.monotonic()<deadline:
   if select.select([p.stderr],[],[],.15)[0]:
    line=os.read(p.stderr.fileno(),4096).decode('utf-8');lines.append(line)
    if 'Listening on TCP' in ''.join(lines):return p,port,directory,''.join(lines)
   if p.poll() is not None:break
  self.fail('Receiver did not start: '+''.join(lines))
 def finish(self,p):
  out,err=p.communicate(timeout=25);self.assertEqual(p.returncode,0,out+err);return [json.loads(line) for line in out.splitlines() if line.startswith('{')]
 def test_version_and_help_do_not_create_profile(self):
  profile=self.root/'unused';p=subprocess.run(RUNNER+[str(BIN/'bitskim-cli'),'--profile',str(profile),'--version'],capture_output=True,text=True);self.assertEqual(p.returncode,0);self.assertIn('0.2',p.stdout);self.assertFalse(profile.exists())
  p=subprocess.run(RUNNER+[str(BIN/'bitskim-cli'),'--help'],capture_output=True,text=True);self.assertEqual(p.returncode,0);self.assertIn('receive',p.stdout)
 def test_unicode_and_empty_payload_with_tls_verification(self):
  for index,data in enumerate([b'',os.urandom(300001)]):
   with self.subTest(size=len(data)):
    receiver,port,directory,_=self.receiver();source=self.root/('Möte-'+str(index)+'.bin');source.write_bytes(data)
    sent=self.run_cli('sender',self.port(),'send','127.0.0.1:'+str(port),str(source),'--json');received=self.finish(receiver)
    self.assertEqual((directory/source.name).read_bytes(),data);self.assertTrue(received[-1]['encrypted']);self.assertTrue(received[-1]['verified']);self.assertEqual(json.loads(sent.stdout.splitlines()[-1])['result'],1)
 def test_folder_and_empty_directory_use_one_approval(self):
  source=self.root/'Tree';(source/'Empty').mkdir(parents=True);(source/'Sub').mkdir();(source/'Sub'/'a.txt').write_bytes(b'folder bytes')
  receiver,port,directory,_=self.receiver();self.run_cli('sender',self.port(),'send','127.0.0.1:'+str(port),str(source));self.finish(receiver)
  self.assertEqual((directory/'Tree'/'Sub'/'a.txt').read_bytes(),b'folder bytes');self.assertTrue((directory/'Tree'/'Empty').is_dir())
 def test_collisions_preserve_existing_destination(self):
  source=self.root/'keep.txt';source.write_bytes(b'new');directory=self.root/'received';directory.mkdir();(directory/'keep.txt').write_bytes(b'original')
  receiver,port,_,_=self.receiver();self.run_cli('sender',self.port(),'send','127.0.0.1:'+str(port),str(source));self.finish(receiver)
  self.assertEqual((directory/'keep.txt').read_bytes(),b'original');self.assertTrue(any(p.read_bytes()==b'new' for p in directory.iterdir() if p.is_file()))
 def test_failed_queue_is_saved_paused_and_explicitly_continued(self):
  source=self.root/'retry.txt';source.write_bytes(b'saved retry');port=self.port();sender_port=self.port()
  failed=self.run_cli('sender',sender_port,'send','127.0.0.1:'+str(port),str(source),check=False);self.assertEqual(failed.returncode,1)
  queue=json.loads(self.run_cli('sender',sender_port,'queue','--json').stdout);self.assertEqual(len(queue),1);self.assertTrue(queue[0]['paused'])
  receiver,_,directory,_=self.receiver(port=port);self.run_cli('sender',sender_port,'continue','--json');self.finish(receiver);self.assertEqual((directory/source.name).read_bytes(),source.read_bytes())
 def test_history_persistence_clear_and_private_permissions(self):
  source=self.root/'history';source.write_bytes(b'history');receiver,port,_,_=self.receiver();self.run_cli('sender',self.port(),'send','127.0.0.1:'+str(port),str(source));self.finish(receiver)
  self.assertTrue(json.loads(self.run_cli('sender',self.port(),'history','--json').stdout))
  self.run_cli('sender',self.port(),'clear-history');self.assertEqual(json.loads(self.run_cli('sender',self.port(),'history','--json').stdout),[])
  for file in [self.root/'sender/config/settings.ini',self.root/'sender/data/trust/identity.pem',self.root/'sender/state/transfers.dat']:self.assertEqual(stat.S_IMODE(file.stat().st_mode),0o600)
 def test_profile_lock_prevents_concurrent_clients(self):
  self.receiver();p=self.run_cli('receiver',self.port(),'history',check=False);self.assertEqual(p.returncode,2);self.assertIn('already open',p.stderr)
 def test_receiving_without_terminal_needs_explicit_acceptance(self):
  p=self.run_cli('receiver',self.port(),'receive',check=False);self.assertEqual(p.returncode,2);self.assertIn('--accept',p.stderr)
 def test_symlink_source_is_not_queued(self):
  source=self.root/'source';source.write_bytes(b'private');link=self.root/'link';link.symlink_to(source);p=self.run_cli('sender',self.port(),'send','127.0.0.1:3',str(link),check=False);self.assertEqual(p.returncode,2);self.assertIn('regular',p.stderr)
 def test_invitation_checks_real_receiving_identity(self):
  receiver,port,directory,log=self.receiver(invitation=True);uri=log.split('): ',1)[1].splitlines()[0];source=self.root/'invite';source.write_bytes(b'qr invitation')
  self.run_cli('sender',self.port(),'send','--invitation',uri,str(source));self.finish(receiver);self.assertEqual((directory/source.name).read_bytes(),source.read_bytes())
 def test_metadata_and_adjacent_icon_round_trip(self):
  data=b'Amiga payload';source=self.root/'AmigaFile';source.write_bytes(data);(self.root/'AmigaFile.info').write_bytes(b'companion icon')
  wire=struct.pack('>4sB3xIIII160s',b'BSM1',1,16,0,zlib.crc32(data),len(data),b'File comment');self.assertEqual(len(wire),184);(self.root/'AmigaFile.bskmeta').write_bytes(wire)
  receiver,port,directory,_=self.receiver(metadata=True);self.run_cli('sender',self.port(),'send','127.0.0.1:'+str(port),str(source),'--metadata');self.finish(receiver)
  self.assertEqual((directory/source.name).read_bytes(),data);self.assertEqual((directory/'AmigaFile.bskmeta').read_bytes(),wire);self.assertEqual((directory/'AmigaFile.info').read_bytes(),b'companion icon')
 def test_native_gtk_smoke(self):
  if not (BIN/'bitskim').is_file() or not shutil.which('xvfb-run') or not shutil.which('dbus-run-session'):self.skipTest('GTK and display test tools required')
  directory=self.root/'gui-receive';directory.mkdir();command=['dbus-run-session','--','xvfb-run','-a',*RUNNER,str(BIN/'bitskim'),'--profile',str(self.root/'desktop'),'--directory',str(directory),'--port',str(self.port()),'--smoke-test']
  p=subprocess.run(command,capture_output=True,text=True,timeout=20,env=dict(os.environ,G_DEBUG='fatal-criticals',GSK_RENDERER='cairo'));self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('PASS Linux GTK',p.stdout);self.assertNotIn('CRITICAL',p.stderr)

 def test_native_gtk_accepts_and_declines_real_tls_transfers(self):
  if not (BIN/'bitskim').is_file() or any(not shutil.which(tool) for tool in ('xvfb-run','dbus-run-session','xdotool')):self.skipTest('GTK and display test tools required')
  available=subprocess.run([sys.executable,'-c',"import gi;gi.require_version('Atspi','2.0')"],capture_output=True).returncode==0
  if not available:self.skipTest('Python GI and Atspi introspection required for desktop control review')
  command=['dbus-run-session','--','xvfb-run','-a',sys.executable,str(ROOT/'tests/linux_desktop_review.py'),str(BIN)]
  p=subprocess.run(command,capture_output=True,text=True,timeout=70,env=dict(os.environ,G_DEBUG='fatal-criticals',GSK_RENDERER='cairo'));self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('PASS GTK TLS',p.stdout)

 def test_discovery_reports_real_receiving_endpoint(self):
  directory=self.root/'broadcast-received';directory.mkdir();port=self.port()
  command=self.command('broadcast-receiver',port,'receive','--directory',str(directory),'--accept','--timeout','12')
  command.remove('--no-discovery');command[command.index('--bind')+1]='0.0.0.0'
  receiver=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);self.processes.append(receiver)
  ready='';deadline=time.monotonic()+8
  while 'Listening on TCP' not in ready and time.monotonic()<deadline:
   if select.select([receiver.stderr],[],[],.1)[0]:ready+=os.read(receiver.stderr.fileno(),4096).decode()
  self.assertIn('Listening on TCP',ready)
  command=self.command('search',self.port(),'discover','--timeout','5','--json')
  command.remove('--no-discovery');command[command.index('--bind')+1]='0.0.0.0'
  result=subprocess.run(command,capture_output=True,text=True,timeout=8);self.assertEqual(result.returncode,0,result.stderr)
  identity=(self.root/'broadcast-receiver/config/settings.ini').read_text().split('ID=',1)[1].splitlines()[0]
  matches=[peer for peer in json.loads(result.stdout) if peer['id']==identity]
  self.assertTrue(matches,result.stdout);self.assertTrue(all(peer['port']==port and peer['platform']=='Linux' for peer in matches))

 def test_manual_mode_never_sends_initial_discovery_probe(self):
  with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as listener:
   listener.bind(('0.0.0.0',46440));listener.settimeout(.2)
   self.run_cli('manual',self.port(),'diagnostics')
   with self.assertRaises(socket.timeout):listener.recvfrom(2048)
