"""RISC OS packages with Acorn filetypes, matching source and relink material."""
from pathlib import Path
from zipfile import ZipFile,ZipInfo,ZIP_DEFLATED
import os,time,struct
from build_provenance import verify

def acorn_add(z,source,name,filetype):
    info=ZipInfo(name,time.localtime(source.stat().st_mtime)[:6])
    info.compress_type=ZIP_DEFLATED
    centiseconds=int((source.stat().st_mtime+2208988800)*100)
    load=0xfff00000 | (filetype<<8) | ((centiseconds>>32)&255)
    info.extra=struct.pack('<HH4sIIII',0x4341,20,b'ARC0',load,centiseconds&0xffffffff,0x33,0)
    z.writestr(info,source.read_bytes())

def write_riscos(out,sourcezip,root):
    for cpu in ('arm2','strongarm'):
        if (root/'build/riscos'/cpu/'!BitSkim/!RunImage').is_file():verify('riscos-'+cpu,root)
    riscoszips=[]
    for cpu,label in [('arm2','ARM2'),('strongarm','StrongARM')]:
        folder=root/'build/riscos'/cpu
        if not (folder/'!BitSkim/!RunImage').is_file(): continue
        sdk=Path(os.environ.get('RISCOS_SDK_SOURCE',str(root/'work/riscos/gccsdk295.tar.gz')))
        if not sdk.is_file(): raise SystemExit('RISC OS packaging requires RISCOS_SDK_SOURCE (the original GCCSDK source tar.gz)')
        image=(folder/'!BitSkim/!RunImage').read_bytes()
        if len(image)<128 or struct.unpack_from('<I',image,48)[0]!=26:
            raise SystemExit('Expected APCS-26 AIF image for '+cpu)
        archive=out/('BitSkim-0.1-RISCOS3-'+label+'.zip')
        with ZipFile(archive,'w',ZIP_DEFLATED) as z:
            acorn_add(z,root/'build/provenance'/('riscos-'+cpu+'.json'),'BuildInfo',0xfff)
            for path in sorted((folder/'!BitSkim').iterdir()):
                if path.is_file() and path.name!='Choices':
                    kind={'!RunImage':0xff8,'!Run':0xffb,'!Boot':0xffb,'!Sprites':0xff9}.get(path.name,0xfff)
                    acorn_add(z,path,'!BitSkim/'+path.name,kind)
            for src,dest in [('docs/TRANSFER-TOOLS.md','Tools'),('docs/RISCOS.md','README'),('LICENSE','Licence'),('THIRD-PARTY-NOTICES.MD','Notices'),('assets/themes/CATPPUCCIN-LICENSE','PaletteLic'),('assets/licenses/UNIXLIB-COPYRIGHT','UnixCopy'),('assets/licenses/UNIXLIB-LGPL','UnixLGPL'),('assets/licenses/GCC-GPL','GCCGPL'),('assets/licenses/LIBGCC-NOTICE','LibGCC')]:
                acorn_add(z,root/src,dest,0xfff)
            for src,dest in [('main.o','Client'),('bitskim.o','CoreObj'),('platform.o','Platform'),('riscos-file.o','NativeFS')]:
                acorn_add(z,folder/src,'Relink/'+dest,0xffd)
            acorn_add(z,sourcezip,'Relink/ProjectSrc',0xffd)
            acorn_add(z,sdk,'Relink/SDKSrc',0xffd)
            acorn_add(z,root/'scripts/gccsdk-2.95-host.patch','Relink/HostPatch',0xfff)
        riscoszips.append(archive)
    return riscoszips
