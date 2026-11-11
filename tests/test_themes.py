#!/usr/bin/env python3
"""Check shared palette contracts and text contrast on each themed surface."""
import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def luminance(value):
    rgb=[int(value[i:i+2],16)/255 for i in (0,2,4)]
    linear=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
    return sum(v*w for v,w in zip(linear,(.2126,.7152,.0722)))
def contrast(a,b):
    a,b=sorted((luminance(a),luminance(b)))
    return (b+.05)/(a+.05)
class ThemeTests(unittest.TestCase):
    def test_readable_text_on_both_surfaces(self):
        for t in json.loads((ROOT/'assets/themes/palettes.json').read_text()):
            for foreground in ('text','muted'):
                for background in ('base','surface'):
                    self.assertGreaterEqual(contrast(t[foreground],t[background]),4.5,(t['name'],foreground,background))
    def test_selected_button_text_is_readable(self):
        for t in json.loads((ROOT/'assets/themes/palettes.json').read_text()):
            foreground=t['shine'] if t['id']=='classic' else t['base']
            self.assertGreaterEqual(contrast(foreground,t['accent']),4.5,t['name'])
    def test_same_catalog_in_both_clients(self):
        palettes=json.loads((ROOT/'assets/themes/palettes.json').read_text())
        ids=[t['id'] for t in palettes]; self.assertEqual(len(set(ids)),8)
        swift=(ROOT/'clients/macos/Themes.swift').read_text()
        amiga=(ROOT/'clients/amiga/themes.h').read_text()
        self.assertEqual(re.findall(r'AppTheme\(id: "([^"]+)"',swift),ids)
        self.assertEqual(re.findall(r'\{"([^"]+)"',amiga),ids)
        windows=(ROOT/'clients/windows/themes.h').read_text()
        self.assertEqual(re.findall(r'\{"([^"]+)"',windows),ids)
    def test_classic_fallback_is_valid(self):
        palettes=json.loads((ROOT/'assets/themes/palettes.json').read_text())
        header=(ROOT/'clients/amiga/themes.h').read_text()
        i=int(re.search(r'#define BS_THEME_CLASSIC (\d+)',header)[1])
        self.assertEqual(palettes[i]['id'],'classic')
if __name__=='__main__':unittest.main(verbosity=2)
