#!/usr/bin/env python3
"""Build a drag-to-Applications HFS+ DMG without mounting or automating Finder."""
from pathlib import Path
import importlib.util,os,shutil,subprocess
from ds_store import DSStore
from build_provenance import begin,finish
ROOT=Path(__file__).resolve().parents[1]
assert (ROOT/'VERSION').read_text().strip()=='0.1'
begin('dmg',ROOT)
app=ROOT/'build/macos/BitSkim.app'
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
spec=importlib.util.spec_from_file_location('bitskim_dmg',ROOT/'installers/macos/settings.py')
layout=importlib.util.module_from_spec(spec);spec.loader.exec_module(layout)
stage=ROOT/'build/macos-dmg-stage'
if stage.exists():shutil.rmtree(stage)
stage.mkdir();shutil.copytree(app,stage/'BitSkim.app',symlinks=True)
(stage/'Applications').symlink_to('/Applications',target_is_directory=True)
docs=stage/'Read Me';docs.mkdir()
for source,name in [('docs/TRANSFER-TOOLS.md','Transfer tools.txt'),('build/provenance/macos.json','Build provenance.json'),('docs/INSTALLATION.md','Installation.txt'),('docs/TRANSFERS.md','Transfer options.txt'),('assets/licenses/OpenSSL-LICENSE','OpenSSL license.txt'),('LICENSE','LICENSE'),('THIRD-PARTY-NOTICES.MD','Third-party notices.txt'),('assets/themes/CATPPUCCIN-LICENSE','Catppuccin license.txt')]:shutil.copy2(ROOT/source,docs/name)
shutil.copy2(ROOT/'assets/icons/BitSkim.icns',stage/'.VolumeIcon.icns')
# kHasCustomIcon on the source root is carried into the HFS image.
finder_info=bytearray(32);finder_info[8:10]=(0x0400).to_bytes(2,'big')
subprocess.run(['xattr','-wx','com.apple.FinderInfo',finder_info.hex(),str(stage)],check=True)
with DSStore.open(str(stage/'.DS_Store'),'w+') as d:
 d['.']['vSrn']=('long',1)
 d['.']['bwsp']={'WindowBounds':layout.BOUNDS,'ShowToolbar':False,'ShowStatusBar':False,'ShowSidebar':False,'ShowPathbar':False,'ShowTabView':False,'ContainerShowSidebar':False,'SidebarWidth':0}
 d['.']['icvl']=(b'type',b'icnv')
 d['.']['icvp']={'viewOptionsVersion':1,'backgroundType':1,'backgroundColorRed':layout.BACKGROUND[0],'backgroundColorGreen':layout.BACKGROUND[1],'backgroundColorBlue':layout.BACKGROUND[2],'iconSize':96.0,'textSize':13.0,'gridOffsetX':0.0,'gridOffsetY':0.0,'gridSpacing':100.0,'arrangeBy':'none','showIconPreview':False,'showItemInfo':False,'labelOnBottom':True,'scrollPositionX':0.0,'scrollPositionY':0.0}
 for name,location in layout.LOCATIONS.items():d[name]['Iloc']=location
# makehybrid creates HFS metadata in user space; no virtual device mount is needed.
raw=ROOT/'build/BitSkim-0.1-macOS-uncompressed.dmg';out=ROOT/'outputs/BitSkim-0.1-macOS-arm64.dmg';out.parent.mkdir(exist_ok=True)
subprocess.run(['hdiutil','makehybrid','-hfs','-hfs-volume-name',layout.VOLUME,'-hfs-openfolder',str(stage),'-ov','-o',str(raw),str(stage)],check=True)
subprocess.run(['hdiutil','convert',str(raw),'-format','UDZO','-ov','-o',str(out)],check=True)
subprocess.run(['hdiutil','verify',str(out)],check=True)
identity=os.environ.get('BITSKIM_SIGN_IDENTITY')
if identity:
    subprocess.run(['codesign','--force','--timestamp','--sign',identity,str(out)],check=True)
    subprocess.run(['codesign','--verify','--strict',str(out)],check=True)
finish('dmg',ROOT,tool='hdiutil',version_arg='help')
print('Built',out)

# Refresh the configured local compiled-package mirror after successful packaging.
import subprocess,sys
subprocess.run([sys.executable,str(Path(__file__).resolve().parent/'sync-builds.py')],check=True)
