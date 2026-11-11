#!/usr/bin/env python3
"""Adapt the owner's classic package/installer icons without altering their images."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
def images(data):
    if len(data)<78 or struct.unpack_from('>HH',data)!=(0xe310,1):raise ValueError('Expected classic DiskObject v1')
    at=78+(56 if struct.unpack_from('>I',data,66)[0] else 0);start=at
    for pointer in (22,26):
        if not struct.unpack_from('>I',data,pointer)[0]:continue
        left,top,w,h,depth,pixels,pick,on,next_image=struct.unpack_from('>hhhhhIBBI',data,at)
        if w<1 or h<1 or depth<1 or depth>8 or next_image:raise ValueError('Unsupported image geometry')
        at+=20+(((w+15)//16)*2*h*depth if pixels else 0)
        if at>len(data):raise ValueError('Truncated planar image')
    return data[start:at],at

def build(root=ROOT):
    art=root/'assets/icons';templates=art/'generic-amiga-installer'
    src=(templates/'SoftwarePackage.info').read_bytes();payload,_=images(src)
    header=bytearray(src[:78]);header[48]=2
    for offset in (50,54,70,74):struct.pack_into('>I',header,offset,0)
    struct.pack_into('>I',header,66,1)
    # Revision 1 adds DrawerData2 after all image/string data. Explicitly
    # choose icons-only/icon view rather than inheriting Show All from a parent.
    revision=struct.unpack_from('>I',header,44)[0]
    struct.pack_into('>I',header,44,(revision&~255)|1)
    # OldDrawerData: ordinary resizable Workbench drawer, no private screen.
    drawer=struct.pack('>hhhhBBIIIIIIIhhHHHii',40,30,480,180,0,1,0,0x240027,0,0,0,0,0,200,100,65535,65535,1,0,0)
    (art/'BitSkimDrawer.info').write_bytes(header+drawer+payload+struct.pack('>IH',1,1))
    src=(templates/'Install.info').read_bytes();payload,_=images(src)
    header=bytearray(src[:78]);header[48]=4
    struct.pack_into('>I',header,50,1);struct.pack_into('>I',header,54,0);struct.pack_into('>I',header,66,0);struct.pack_into('>I',header,70,0);struct.pack_into('>I',header,74,65536)
    tool=b'StartInstaller\0'
    (art/'Install-BitSkim.info').write_bytes(header+payload+struct.pack('>I',len(tool))+tool)
    return art/'BitSkimDrawer.info',art/'Install-BitSkim.info'
if __name__=='__main__':
    for path in build():print(path)
