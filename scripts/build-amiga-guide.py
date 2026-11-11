#!/usr/bin/env python3
"""Build the local AmigaGuide manual and original classic book project icon."""
from pathlib import Path
import shutil, struct
ROOT=Path(__file__).resolve().parents[1]
from amiga_package_icons import build
build(ROOT)
w,h=40,28
pixels=[[0]*w for _ in range(h)]
def box(x,y,r,b,c):
    for yy in range(y,b+1):
        for xx in range(x,r+1): pixels[yy][xx]=c
box(3,4,36,24,1);box(5,2,19,21,1);box(20,2,34,21,1)
box(6,3,18,20,2);box(21,3,33,20,2)
box(18,4,21,22,3)
for y in (7,11,15):
    box(8,y,15,y,3);box(24,y,31,y,3)
def planar(selected):
    data=bytearray()
    for plane in range(2):
        for row in pixels:
            line=bytearray(6)
            for x,c in enumerate(row):
                if selected and c in (2,3):c=5-c
                if c&(1<<plane):line[x//8]|=128>>(x%8)
            data.extend(line)
    return struct.pack('>hhhhhIBBI',0,0,w,h,2,1,3,0,0)+data
gadget=struct.pack('>IhhhhHHHIIIIIHI',0,0,0,w,h,6,1,1,1,1,0,0,0,0,0)
tool=b'SYS:Utilities/MultiView\0'
header=struct.pack('>HH',0xe310,1)+gadget+struct.pack('>BBIIiiIII',4,0,1,0,-2147483648,-2147483648,0,0,65536)
icon=ROOT/'assets/icons/BitSkimGuide.info'
icon.write_bytes(header+planar(False)+planar(True)+struct.pack('>I',len(tool))+tool)
out=ROOT/'build/amiga';out.mkdir(parents=True,exist_ok=True)
shutil.copyfile(ROOT/'docs/BitSkim.guide',out/'BitSkim.guide')
shutil.copyfile(icon,out/'BitSkim.guide.info')
# Installation-only icon templates. These do not add an icon to shared data.
from amiga_package_icons import images
art=ROOT/'assets/icons';templates=out/'InstallAssets';templates.mkdir(exist_ok=True)
base=(art/'BitSkimDrawer.info').read_bytes()
header=bytearray((art/'BitSkimGuide.info').read_bytes()[:78]);header[48]=2
for offset in (50,54,70,74):struct.pack_into('>I',header,offset,0)
struct.pack_into('>I',header,66,1)
help_icon=bytes(header)+base[78:134]+images((art/'BitSkimGuide.info').read_bytes())[0]
settings=(art/'BitSkimPrefs.info').read_bytes();header=bytearray(settings[:78]);header[48]=4
for offset in (54,66,70):struct.pack_into('>I',header,offset,0)
struct.pack_into('>I',header,50,1);struct.pack_into('>I',header,74,65536)
tool=b'SYS:Prefs/BitSkimPrefs\0'
settings_icon=bytes(header)+images(settings)[0]+struct.pack('>I',len(tool))+tool
for name,data in [('SettingsDrawer.info',base),('BitSkim.prefs.info',settings_icon),('Startup.info',(art/'BitSkim.info').read_bytes())]:
    (art/name).write_bytes(data);(templates/name).write_bytes(data)

# A single guide is installed directly in HELP:, without an app-specific drawer.
(templates/"HelpDrawer.info").unlink(missing_ok=True)
