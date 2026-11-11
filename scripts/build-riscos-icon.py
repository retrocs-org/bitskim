#!/usr/bin/env python3
"""Classic 4bpp RISC OS 3 sprite derived from the committed shared icon."""
from pathlib import Path
import struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
colours=[(255,255,255),(221,221,221),(187,187,187),(153,153,153),(119,119,119),(85,85,85),(51,51,51),(0,0,0),(0,68,153),(255,255,0),(0,204,0),(221,0,0),(238,238,187),(0,119,0),(255,187,0),(0,187,255)]
master=Image.open(ROOT/'assets/icons/bitskim-amiga-master.png').convert('RGBA')
master.thumbnail((32,26),Image.Resampling.NEAREST)
canvas=Image.new('RGBA',(32,32));canvas.alpha_composite(master,((32-master.width)//2,(32-master.height)//2))
image=bytearray();mask=bytearray()
for y in range(32):
 for x in range(0,32,2):
  pixel=0;opacity=0
  for n in (0,1):
   r,g,b,a=canvas.getpixel((x+n,y));index=min(range(16),key=lambda i:sum((c-v)**2 for c,v in zip(colours[i],(r,g,b))))
   pixel|=index<<(4*n)
   if a>=128: opacity|=15<<(4*n)
  image.append(pixel);mask.append(opacity)
palette=b''.join(struct.pack('<II',r<<8|g<<16|b<<24,r<<8|g<<16|b<<24) for r,g,b in colours)
offset=44+len(palette);size=offset+len(image)+len(mask)
header=struct.pack('<I12s7I',size,b'!bitskim',3,31,0,31,offset,offset+len(image),20)
data=struct.pack('<III',1,16,16+size)+header+palette+image+mask
out=ROOT/'assets/icons/BitSkim-RISCOS.sprites';out.write_bytes(data)
(ROOT/'clients/riscos/app/!BitSkim/!Sprites').write_bytes(data)
print('Built classic RISC OS !bitskim sprite with transparency')
