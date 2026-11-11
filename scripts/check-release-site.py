#!/usr/bin/env python3
"""Fail closed on missing website assets/downloads, stale links or bad checksums."""
import hashlib,re,sys,os,subprocess,shutil
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote,urlsplit
ROOT=Path(__file__).resolve().parents[1]
DOMAIN='https://bitskim.retrocs.org'
REPOSITORY='https://github.com/retrocs-org/bitskim'
class Page(HTMLParser):
 def __init__(self):super().__init__();self.links=[];self.ids=set();self.canonicals=[];self.images=[]
 def handle_starttag(self,tag,attrs):
  values=dict(attrs)
  if 'id' in values:self.ids.add(values['id'])
  for key in ('href','src'):
   if key in values:self.links.append(values[key])
  if tag=='link' and values.get('rel')=='canonical':self.canonicals.append(values.get('href'))
  if tag=='img':self.images.append(values)
def validate(root):
 root=Path(root);errors=[];pages={}
 for include in root.rglob('*.php'):
  php=os.environ.get('PHP','php')
  if not shutil.which(php):raise RuntimeError('PHP CLI is required to validate the PHP website; set PHP to its executable')
  subprocess.run([php,'-l',str(include)],check=True,capture_output=True,text=True)
 for path in sorted(list(root.glob('*.html'))+list(root.glob('*.php'))):
  text=subprocess.run([os.environ.get('PHP','php'),str(path.resolve())],check=True,capture_output=True,text=True,cwd=root,env=dict(os.environ,BITSKIM_RELEASES_OFFLINE='1')).stdout if path.suffix=='.php' else path.read_text()
  if '<?php' in text or 'Warning:' in text or 'Fatal error:' in text:raise RuntimeError('Invalid PHP rendering: '+path.name)
  page=Page();page.feed(text);pages[path.resolve()]=page
  expected=DOMAIN+('/' if path.name in ('index.html','index.php') else '/'+path.name)
  if page.canonicals!=[expected]:errors.append(path.name+': incorrect canonical URL')
  if 'kladhest-se/bitskim' in text:errors.append(path.name+': obsolete repository URL')
  if REPOSITORY not in text:errors.append(path.name+': missing public repository link')
  for image in page.images:
   if 'alt' not in image:errors.append(path.name+': image without alt attribute')
 for path,page in pages.items():
  for link in page.links:
   parts=urlsplit(link)
   if parts.scheme or parts.netloc:continue
   target=((root if parts.path.startswith('/') else path.parent)/unquote(parts.path.lstrip('/'))).resolve() if parts.path else path
   if not target.is_file():errors.append(path.name+': missing '+link);continue
   if parts.fragment and target in pages and parts.fragment not in pages[target].ids:errors.append(path.name+': missing anchor '+link)
 sums=root/'downloads/SHA256SUMS';known={}
 if not sums.exists():errors.append('Missing download SHA256SUMS')
 else:
  for line in sums.read_text().splitlines():
   fields=line.split()
   if len(fields)==2:known[fields[1].lstrip('*')]=fields[0]
 for path,page in pages.items():
  for link in page.links:
   if not link.startswith('downloads/') or link.endswith('SHA256SUMS'):continue
   name=Path(urlsplit(link).path).name;file=root/'downloads'/name
   if name not in known:errors.append('Missing checksum for '+name)
   elif file.is_file() and hashlib.sha256(file.read_bytes()).hexdigest()!=known[name]:errors.append('Checksum mismatch: '+name)
 if not pages:errors.append('No website entry pages found')
 if errors:raise RuntimeError('\n'.join(sorted(set(errors))))
 return len(pages)
if __name__=='__main__':
 try:print('PASS: '+str(validate(ROOT/'public-web'))+' pages, local links/assets, public URLs and downloadable package checksums')
 except (OSError,RuntimeError,subprocess.CalledProcessError) as error:sys.exit(str(error))
