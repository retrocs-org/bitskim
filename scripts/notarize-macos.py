#!/usr/bin/env python3
"""Notarize/staple a verified local Mac build using a saved keychain profile."""
import argparse,json,subprocess,sys
from pathlib import Path
from build_provenance import ROOT,ARTIFACTS,verify,source_hash,tree_hash,record_path,write_json

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('target',choices=('macos','dmg'));parser.add_argument('--profile',default='bitskim');args=parser.parse_args()
 record=verify(args.target)
 path=ROOT/('build/macos/BitSkim.app' if args.target=='macos' else 'outputs/BitSkim-0.1-macOS-arm64.dmg')
 subprocess.run(['codesign','--verify','--deep','--strict',str(path)],check=True)
 signature=subprocess.run(['codesign','-dvv',str(path)],capture_output=True,text=True,check=True)
 if 'Authority=Developer ID Application:' not in signature.stderr:raise RuntimeError('Developer ID signing is required before notarization')
 work=ROOT/'work/notarization';work.mkdir(parents=True,exist_ok=True)
 upload=path
 if args.target=='macos':
  upload=work/'BitSkim-notarization.zip'
  subprocess.run(['ditto','-c','-k','--keepParent',str(path),str(upload)],check=True)
 result=subprocess.run(['xcrun','notarytool','submit',str(upload),'--keychain-profile',args.profile,'--wait','--output-format','json'],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stderr.strip() or result.stdout.strip() or 'Notarization submission failed')
 response=json.loads(result.stdout);write_json(work/(args.target+'-submission.json'),response)
 if response.get('status')!='Accepted':raise RuntimeError('Apple notarization status: '+str(response.get('status'))+'; see work/notarization/'+args.target+'-submission.json')
 # Preserve compiler identity while recording the post-build stapled artifact.
 if source_hash(ROOT)!=record['source_sha256']:raise RuntimeError('Source changed during notarization; rebuild before release')
 subprocess.run(['xcrun','stapler','staple',str(path)],check=True)
 subprocess.run(['xcrun','stapler','validate',str(path)],check=True)
 record['artifacts']={name:tree_hash(ROOT/name) for name in ARTIFACTS[args.target]}
 record['notarization']={'status':'Accepted','submission_id':response['id'],'ticket_stapled':True}
 write_json(record_path(ROOT,args.target),record)
 verify(args.target)
 print('PASS: Developer ID signature, Apple acceptance and stapled ticket: '+str(path))
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as error:sys.exit(str(error))
