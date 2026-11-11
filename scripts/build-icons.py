#!/usr/bin/env python3
"""Convert committed master art to macOS ICNS and classic Workbench planar icon."""
from pathlib import Path
from PIL import Image
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'assets/icons'
iconset=ROOT/'build/BitSkim.iconset'
iconset.mkdir(parents=True,exist_ok=True)
# The shared mark is drawn on the Amiga's native 56x44 grid. A folded file
# and one broad transfer arrow remain readable without fine shading or trails.
from PIL import ImageDraw
w,h=56,44
mark=Image.new('P',(w,h),0); d=ImageDraw.Draw(mark)
d.polygon([(7,4),(25,4),(35,14),(35,39),(7,39)],fill=1)
d.polygon([(9,6),(23,6),(23,16),(33,16),(33,37),(9,37)],fill=2)
d.polygon([(25,7),(31,13),(25,13)],fill=3)
d.polygon([(20,20),(37,20),(37,14),(53,27),(37,40),(37,34),(20,34)],fill=1)
d.polygon([(22,22),(39,22),(39,19),(49,27),(39,35),(39,32),(22,32)],fill=3)
indices=list(mark.getdata())
menu=Image.new('RGBA',(w,h))
menu.putdata([(0,0,0,255) if value in (1,3) else (0,0,0,0) for value in indices])
menu.save(ART/'BitSkim-menu.png')
palette=[(170,170,170),(0,0,0),(255,255,255),(65,100,220)]
amiga=Image.new('RGB',(w,h)); amiga.putdata([palette[i] for i in indices])
amiga.save(ART/'bitskim-amiga-master.png')
# The Mac uses exactly the same pixel geometry, with Mocha colors on a tile.
mac=Image.new('RGBA',(1024,1024),(0,0,0,0)); d=ImageDraw.Draw(mac)
d.rounded_rectangle((48,48,976,976),radius=200,fill='#1e1e2e',outline='#45475a',width=12)
mac_palette=[(0,0,0,0),(17,17,27,255),(205,214,244,255),(203,166,247,255)]
symbol=Image.new('RGBA',(w,h)); symbol.putdata([mac_palette[i] for i in indices])
symbol=symbol.resize((w*16,h*16),Image.Resampling.NEAREST)
mac.alpha_composite(symbol,((1024-symbol.width)//2,(1024-symbol.height)//2))
mac.save(ART/'bitskim-macos-master.png')
for size in (16,32,128,256,512):
    for scale in (1,2):
        suffix='@2x' if scale==2 else ''
        mac.resize((size*scale,size*scale),Image.Resampling.LANCZOS).save(iconset/f'icon_{size}x{size}{suffix}.png')
mac.save(ART/'BitSkim.icns',format='ICNS')
mac.save(ART/'BitSkim.ico',format='ICO',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
mac.resize((256,256),Image.Resampling.LANCZOS).save(ROOT/'public-web/assets/bitskim.png')
mac.resize((64,64),Image.Resampling.LANCZOS).save(ROOT/'public-web/assets/favicon.png')
def planar(selected=False):
    data=bytearray(); stride=((w+15)//16)*2
    for plane in range(2):
        for y in range(h):
            row=bytearray(stride)
            for x in range(w):
                v=indices[y*w+x]
                if selected and v==3: v=2
                elif selected and v==2: v=3
                if v&(1<<plane): row[x//8]|=0x80>>(x%8)
            data+=row
    return bytes(data)
def image(data):
    return struct.pack('>hhhhhIBBI',0,0,w,h,2,1,3,0,0)+data
# Gadget flags: GFLG_GADGIMAGE | GFLG_GADGHIMAGE (normal + selected images).
gadget=struct.pack('>IhhhhHHHIIIIIHI',0,0,0,w,h,0x0006,0x0001,0x0001,1,1,0,0,0,0,0)
assert len(gadget)==44
# DiskObject type WBTOOL=3, no pointer strings or drawer data, 64 KiB stack.
obj=struct.pack('>HH',0xe310,1)+gadget+struct.pack('>BBIIiiIII',3,0,0,0,-2147483648,-2147483648,0,0,65536)
assert len(obj)==78
(ART/'BitSkim.info').write_bytes(obj+image(planar())+image(planar(True)))
preview=Image.new('RGB',(w,h)); palette=[(170,170,170),(0,0,0),(255,255,255),(65,100,220)]
preview.putdata([palette[i] for i in indices]); preview.resize((w*6,h*6),Image.Resampling.NEAREST).save(ART/'bitskim-amiga-preview.png')
print('Built ICNS, website icons, and two-state classic Workbench .info')

# Companion icons extend the existing four-pen Workbench icon family.
# Draw natively at the target size, preserving crisp 1-pixel borders.
from PIL import ImageDraw

def write_companion(name, raster, kind=3, drawer=b''):
    global indices
    original=indices
    indices=list(raster.getdata())
    header=struct.pack('>HH',0xe310,1)+gadget+struct.pack('>BBIIiiIII',kind,0,0,0,-2147483648,-2147483648,1 if drawer else 0,0,65536 if kind==3 else 0)
    (ART/(name+'.info')).write_bytes(header+drawer+image(planar())+image(planar(True)))
    view=Image.new('RGB',(w,h)); view.putdata([palette[i] for i in indices])
    view.resize((w*6,h*6),Image.Resampling.NEAREST).save(ART/(name+'-preview.png'))
    indices=original

prefs=Image.new('P',(w,h),0); d=ImageDraw.Draw(prefs)
d.rectangle((5,5,50,37),fill=1); d.rectangle((7,7,48,35),fill=2)
d.rectangle((7,7,48,12),fill=3)
for y,x in ((18,20),(24,35),(30,26)):
    d.line((12,y,43,y),fill=1,width=2)
    d.rectangle((x-3,y-3,x+3,y+3),fill=1)
    d.rectangle((x-2,y-2,x+2,y+2),fill=3)
write_companion('BitSkimPrefs',prefs)

# Package/installer artwork is supplied by the owner, not the generated app mark.
from amiga_package_icons import build
build(ROOT)
print('Built distinct preferences tool and supplied package/installer icons')
