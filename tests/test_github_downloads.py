"""Offline GitHub catalog tests; no credentials or network requests."""
import json,os,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PHP=os.environ.get('PHP','php')
MODULE=ROOT/'public-web/includes/releases.php'
def asset(name,tag='0.2'):
 return {'name':name,'browser_download_url':'https://github.com/retrocs-org/bitskim/releases/download/'+tag+'/'+name,'state':'uploaded'}
def release(tag,date,names):
 return {'tag_name':tag,'published_at':date,'draft':False,'prerelease':False,'assets':[asset(n,tag) for n in names]}
@unittest.skipUnless(shutil.which(PHP),'PHP CLI not configured')
class GitHubDownloads(unittest.TestCase):
 def php(self,code,data=None):
  r=subprocess.run([PHP,'-r','require '+json.dumps(str(MODULE))+';'+code],input=json.dumps(data),capture_output=True,text=True,check=True)
  self.assertEqual(r.stderr,'');return r.stdout
 def select(self,data):return json.loads(self.php('echo json_encode(bitskim_select_releases(json_decode(stream_get_contents(STDIN),true)));',data))
 def test_latest_per_client_retains_older_missing_platform(self):
  data=[release('0.1','2026-01-01T00:00:00Z',['BitSkim-0.1-AmigaOS.zip']),release('0.2','2026-02-01T00:00:00Z',['BitSkim-0.2-macOS-arm64.dmg','BitSkim-0.2-macOS-arm64.dmg.sha256'])]
  result=self.select(data)
  self.assertEqual(result['clients']['mac-dmg']['version'],'0.2');self.assertEqual(result['clients']['amiga-zip']['version'],'0.1')
  self.assertTrue(result['clients']['mac-dmg']['checksum'].endswith('.sha256'))
  self.assertEqual(result['source'],'https://github.com/retrocs-org/bitskim/archive/refs/tags/0.2.zip')
 def test_drafts_and_prereleases_are_excluded(self):
  data=[release('0.1','2026-01-01',['BitSkim-0.1.lha']),release('0.2','2026-03-01',['BitSkim-0.2.lha']),release('0.3','2026-04-01',['BitSkim-0.3.lha'])]
  data[1]['draft']=True;data[2]['prerelease']=True
  self.assertEqual(self.select(data)['clients']['amiga-lha']['version'],'0.1')
 def test_external_or_wrong_repository_asset_url_rejected(self):
  r=release('0.1','2026-01-01',['BitSkim-0.1.lha']);r['assets'][0]['browser_download_url']='https://evil.example/BitSkim-0.1.lha'
  self.assertEqual(self.select([r])['clients'],[])
  r['assets'][0]['browser_download_url']='https://github.com/other/project/releases/download/0.1/BitSkim-0.1.lha'
  self.assertEqual(self.select([r])['clients'],[])
 def test_empty_or_malformed_catalog_falls_back(self):
  result=self.select([None,{},'bad']);self.assertIsNone(result['source']);self.assertIsNone(result['release_version'])
 def test_version_prefix_removed_from_release_label(self):
  result=self.select([release('v0.2','2026-01-01',[])])
  self.assertEqual(result['release_version'],'0.2');self.assertTrue(result['source'].endswith('/v0.2.zip'))
 def test_all_client_asset_families(self):
  names=['macOS-arm64.dmg','macOS-arm64.zip','Windows10-x64-Setup.exe','Windows10-x64.zip','AmigaOS.zip','DOS-mTCP.zip','RISCOS3-ARM2.zip','RISCOS3-StrongARM.zip','Atari-TOS-STinG-68000.zip','Atari-MiNTNet-68000.zip','Atari-MiNTNet-68060.zip','Windows-Classic-x86.zip']
  result=self.select([release('0.1','2026-01-01',['BitSkim-0.1-'+n for n in names]+['BitSkim-0.1.lha'])])
  self.assertEqual(len(result['clients']),13)
 def test_cache_prevents_repeated_fetch_and_retains_last_known_on_error(self):
  data=[release('0.1','2026-01-01',['BitSkim-0.1.lha'])]
  with tempfile.TemporaryDirectory() as tmp:
   code='$d=json_decode(stream_get_contents(STDIN),true);$p='+json.dumps(tmp+'/cache.json')+';$n=0;$fetch=function()use(&$n,$d){$n++;return $d;};$a=bitskim_load_releases($fetch,$p,1000);$b=bitskim_load_releases($fetch,$p,1100);$c=bitskim_load_releases(function(){return null;},$p,2000);echo json_encode([$n,$a,$b,$c]);'
   n,a,b,c=json.loads(self.php(code,data));self.assertEqual(n,1);self.assertEqual(a,b);self.assertEqual(a,c)
 def test_render_uses_github_links_and_checksum(self):
  data=[release('0.2','2026-01-01',['BitSkim-0.2.lha','BitSkim-0.2.lha.sha256'])]
  html=self.php('$releaseCatalog=bitskim_select_releases(json_decode(stream_get_contents(STDIN),true));bitskim_client_card("AmigaOS","Available","Description",[["amiga-lha","LHA","downloads/BitSkim-0.1.lha"]]);',data)
  self.assertIn('Version 0.2',html);self.assertIn('SHA-256',html);self.assertNotIn('href="downloads/',html)
 def test_offline_fallback_links_directly_to_github(self):
  html=self.php('$releaseCatalog=bitskim_select_releases([]);bitskim_client_card("AmigaOS","Available","Description",[["amiga-lha","LHA","downloads/BitSkim-0.1.lha"]]);')
  self.assertIn('fallback',html);self.assertIn('https://github.com/retrocs-org/bitskim/releases/download/0.1/BitSkim-0.1.lha',html);self.assertNotIn('href="downloads/',html)
  self.assertEqual(self.php('$releaseCatalog=bitskim_select_releases([]);echo bitskim_source_url();'),'https://github.com/retrocs-org/bitskim/archive/refs/tags/0.1.zip')
