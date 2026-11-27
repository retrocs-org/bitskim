#!/usr/bin/env python3
"""Reuse AmiRocket's drawer/installer artwork, adapting only the installer launch metadata."""
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
    src=(templates/'SoftwarePackage.info').read_bytes()
    images(src)
    if src[48]!=2:raise ValueError('Expected the AmiRocket drawer icon')
    (art/'BitSkimDrawer.info').write_bytes(src)
    src=(templates/'Install.info').read_bytes();payload,_=images(src)
    header=bytearray(src[:78]);header[48]=4
    struct.pack_into('>I',header,50,1);struct.pack_into('>I',header,54,0);struct.pack_into('>I',header,66,0);struct.pack_into('>I',header,70,0);struct.pack_into('>I',header,74,65536)
    tool=b'StartInstaller\0'
    (art/'Install.info').write_bytes(header+payload+struct.pack('>I',len(tool))+tool)
    return art/'BitSkimDrawer.info',art/'Install.info'
if __name__=='__main__':
    for path in build():print(path)
