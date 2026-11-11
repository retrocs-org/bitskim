#!/usr/bin/env python3
"""Derive classic 16-colour DIB icons from the existing owner-approved master.
Requires Pillow only when regenerating the checked-in asset, not when building.
"""
from pathlib import Path
import struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
master=Image.open(ROOT/'assets/icons/bitskim-macos-master.png').convert('RGBA')
images=[]
for size in (16,32,48):
    rgba=master.resize((size,size),Image.Resampling.LANCZOS)
    indexed=rgba.convert('RGB').quantize(colors=16)
    palette=indexed.getpalette(); colors=b''.join(bytes((palette[i+2],palette[i+1],palette[i],0)) for i in range(0,48,3))
    rowbytes=((size*4+31)//32)*4; maskbytes=((size+31)//32)*4
    pixels=bytearray(); mask=bytearray()
    for y in reversed(range(size)):
        row=bytearray(rowbytes); alpha=bytearray(maskbytes)
        for x in range(size):
            row[x//2]|=indexed.getpixel((x,y))<<(4 if x%2==0 else 0)
            if rgba.getpixel((x,y))[3]<128: alpha[x//8]|=128>>(x%8)
        pixels.extend(row); mask.extend(alpha)
    dib=struct.pack('<IiiHHIIiiII',40,size,size*2,1,4,0,len(pixels),0,0,16,16)+colors+pixels+mask
    images.append((size,dib))
header=struct.pack('<HHH',0,1,len(images)); entries=b''; payload=b''; offset=6+16*len(images)
for size,dib in images:
    entries+=struct.pack('<BBBBHHII',size,size,16,0,1,4,len(dib),offset);payload+=dib;offset+=len(dib)
(ROOT/'assets/icons/BitSkim-Windows-Classic.ico').write_bytes(header+entries+payload)
