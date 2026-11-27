#!/usr/bin/env python3
"""Generate current compatibility summaries from docs/compatibility.json."""
import argparse
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]

def outputs(root=ROOT):
    source=root/'docs/compatibility.json';raw=source.read_text();data=json.loads(raw)
    if data.get('schema')!=1 or data.get('version')!=(root/'VERSION').read_text().strip():raise ValueError('Compatibility version does not match the app')
    ids=[row['id'] for row in data['clients']]
    if len(ids)!=len(set(ids)) or not ids:raise ValueError('Invalid compatibility client inventory')
    result={root/'public-web/includes/compatibility.json':raw}
    for name,prefix in [('README.MD','docs/'),('docs/VALIDATION.md','')]:
        compact=name=='README.MD'
        lines=['<!-- compatibility:start -->',*(['| Client | Target |','| --- | --- |'] if compact else ['| Client / target | Checks completed for '+data['version']+' | Still unverified |','| --- | --- | --- |'])]
        for row in data['clients']:
            if not (root/'docs'/row['guide']).is_file():raise ValueError('Missing platform guide')
            label='['+row['name']+']('+prefix+row['guide']+')'
            values=[label,row['target']] if compact else [label+' — '+row['target'],row['verified'],row['pending']]
            lines.append('| '+' | '.join(value.replace('|',r'\|') for value in values)+' |')
        if not compact:lines.extend(['',data['scope']])
        lines.append('<!-- compatibility:end -->')
        path=root/name;text=path.read_text()
        if text.count('<!-- compatibility:start -->')!=1 or text.count('<!-- compatibility:end -->')!=1:raise ValueError('Missing compatibility markers')
        result[path]=re.sub(r'<!-- compatibility:start -->.*?<!-- compatibility:end -->','\n'.join(lines),text,flags=re.S)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    expected=outputs();stale=[str(p.relative_to(ROOT)) for p,text in expected.items() if not p.exists() or p.read_text()!=text]
    if args.check and stale:raise SystemExit('Refresh compatibility summaries: '+', '.join(stale))
    if not args.check:
        for path,text in expected.items():path.write_text(text)
    print('Compatibility summaries match docs/compatibility.json')
if __name__=='__main__':main()
