"""One complete version changelog for repository notes and Aminet text."""
import re,unicodedata
from pathlib import Path
GROUPS=('Features','Enhancements','Bugs')

def version_changes(root,version='0.2'):
    text=(Path(root)/'CHANGELOG.MD').read_text();match=re.search(r'^## '+re.escape(version)+r'(?:[ \t].*)?$',text,re.M)
    if not match:raise ValueError('Missing current version in CHANGELOG.MD')
    section=text[match.end():];section=re.split(r'^## ',section,1,flags=re.M)[0]
    result={name:[] for name in GROUPS};group=None;current=None
    for line in section.splitlines():
        if line.startswith('### '):
            group=line[4:].strip()
            if group not in result:raise ValueError('Use Features, Enhancements and Bugs in the version changelog')
            current=None
        elif line.startswith('- '):
            if group is None:
                if version=='0.1':group='Features'
                else:raise ValueError('Group current-version changes under Features, Enhancements and Bugs')
            result[group].append(line[2:].strip());current=len(result[group])-1
        elif line.strip():
            if current is None:raise ValueError('Unexpected current-version changelog text')
            result[group][current]+=' '+line.strip()
    if version!='0.1' and any(not result[name] for name in GROUPS):raise ValueError('Every current-version changelog group needs entries')
    return result

def markdown_changes(root,version='0.2'):
    return '\n\n'.join('#### '+name+'\n\n'+'\n'.join('- '+item for item in items) for name,items in version_changes(root,version).items())

def validate_notes(text,root,version='0.2'):
    if not text.startswith('# BitSkim '+version+'\n'):raise ValueError('Release notes must start with the current BitSkim version')
    if re.search(r'(?i)in development|TODO|TBD|placeholder',text):raise ValueError('Finish release notes before preparing publication')
    for heading in ('Features','Installation','Source and Licence','Version Changelog'):
        if not re.search(r'^## '+re.escape(heading)+r'$',text,re.M):raise ValueError('Missing release notes section: '+heading)
    expected='### '+version+'\n\n'+markdown_changes(root,version)
    actual=text.split('## Version Changelog\n',1)[1].strip()
    if actual!=expected:raise ValueError('Release notes must contain the complete matching current-version changelog')
    return text.strip()

def plain(text):
    text=re.sub(r'\[([^]]+)\]\(([^)]+)\)',r'\1 (\2)',text).replace('`','')
    for a,b in [('—','-'),('–','-'),('’',"'"),('→','->'),('↗',''),('×','x'),('…','...')]:text=text.replace(a,b)
    return unicodedata.normalize('NFKD',text).encode('ascii','ignore').decode()
