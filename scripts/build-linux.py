#!/usr/bin/env python3
"""Build Linux desktop/CLI clients with the distribution's GTK, GLib and OpenSSL."""
import argparse,html,json,os,platform,shlex,shutil,struct,subprocess,sys
from pathlib import Path
from build_provenance import begin,finish,ROOT
ARCHES={'aarch64':'arm64','arm64':'arm64','x86_64':'x86_64','amd64':'x86_64'}
def native_arch():
 try:return ARCHES[platform.machine().lower()]
 except KeyError:raise RuntimeError('Supported Linux architectures: arm64 and x86_64')
def check_elf(path,arch):
 header=path.read_bytes()[:20]
 if len(header)!=20 or header[:6]!=b'\x7fELF\x02\x01' or struct.unpack('<H',header[18:20])[0]!={'arm64':183,'x86_64':62}[arch]:raise RuntimeError('Wrong ELF architecture: '+str(path))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--arch',choices=('arm64','x86_64'));p.add_argument('--cli-only',action='store_true');p.add_argument('--cc',default=os.environ.get('CC'));args=p.parse_args()
 if sys.platform!='linux':p.error('Run on Linux. From macOS use scripts/build-linux-lima.py --instance NAME.')
 arch=args.arch or native_arch();cross=arch!=native_arch();triplet={'arm64':'aarch64-linux-gnu','x86_64':'x86_64-linux-gnu'}[arch]
 compiler=shlex.split(args.cc or (triplet+'-gcc' if cross else 'cc'));env=os.environ.copy()
 env.setdefault('PKG_CONFIG_LIBDIR','/usr/lib/'+triplet+'/pkgconfig:/usr/share/pkgconfig')
 if cross and env.get('BITSKIM_LINUX_PKG_CONFIG_PATH'):env['PKG_CONFIG_PATH']=env['BITSKIM_LINUX_PKG_CONFIG_PATH']
 def pkg(*arguments):return subprocess.check_output(['pkg-config',*arguments],env=env,text=True).strip()
 for module,minimum in [('gio-unix-2.0','2.66'),('openssl','3.0')]+([] if args.cli_only else [('gtk4','4.12')]):
  if subprocess.run(['pkg-config','--atleast-version='+minimum,module],env=env).returncode:raise RuntimeError('Install '+module+' development files, version '+minimum+' or newer, for '+arch)
 out=ROOT/'build/linux'/arch;out.mkdir(parents=True,exist_ok=True)
 versions={module:pkg('--modversion',module) for module in (['gio-unix-2.0','openssl']+([] if args.cli_only else ['gtk4']))};(out/'DEPENDENCIES.json').write_text(json.dumps({'architecture':arch,'system':platform.platform(),'packages':versions},indent=2)+'\n')
 common=['core/bitskim.c','core/platform.c','clients/linux/common.c'];flags=['-std=c11','-D_GNU_SOURCE','-DBS_OPENSSL','-O2','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fstack-protector-strong','-D_FORTIFY_SOURCE=2','-fPIE','-ffile-prefix-map='+str(ROOT)+'=.','-Icore','-Iclients/linux','-I'+str(out),'-Wl,-z,relro,-z,now','-pie']
 def build(target,source,modules,extras=()):
  begin(target);command=compiler+flags+shlex.split(pkg('--cflags',*modules))+common+[source,*extras,'-o',str(out/('bitskim-cli' if target.endswith('-cli') else 'bitskim'))]+shlex.split(pkg('--libs',*modules));subprocess.run(command,cwd=ROOT,env=env,check=True);check_elf(out/('bitskim-cli' if target.endswith('-cli') else 'bitskim'),arch);finish(target,tool=compiler[0]);print('Built '+str(out/('bitskim-cli' if target.endswith('-cli') else 'bitskim')),flush=True)
 build('linux-'+arch+'-cli','clients/linux/cli.c',['gio-unix-2.0','openssl'])
 if not args.cli_only:
  palettes=json.loads((ROOT/'assets/themes/palettes.json').read_text());header='typedef struct {const char *id,*name,*base,*surface,*text,*muted,*accent,*shadow;int dark;} LinuxTheme;\nstatic const LinuxTheme linux_themes[]={\n'
  for theme in palettes:header+='{'+','.join(json.dumps(theme[key]) for key in ('id','name','base','surface','text','muted','accent','shadow'))+','+str(int(theme['dark']))+'},\n'
  (out/'linux-themes.h').write_text(header+'};\n')
  resource=out/'resources.xml';resource.write_text('<gresources><gresource prefix="/org/bitskim"><file alias="icon.png">'+html.escape(str(ROOT/'public-web/assets/bitskim.png'))+'</file></gresource></gresources>')
  subprocess.run(['glib-compile-resources',str(resource),'--generate-source','--target='+str(out/'resources.c')],check=True)
  build('linux-'+arch,'clients/linux/desktop.c',['gtk4','gio-unix-2.0','openssl'],['third_party/qrcodegen/qrcodegen.c',str(out/'resources.c')])
if __name__=='__main__':
 try:main()
 except (RuntimeError,OSError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
