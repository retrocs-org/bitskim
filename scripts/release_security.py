"""Fail on common credential files and token/key formats without printing values."""
import re
from pathlib import Path
from build_provenance import SOURCE_ROOTS
from source_release import private_release_file
PATTERNS=[re.compile(r'gh[pousr]_[A-Za-z0-9]{30,}'),re.compile(r'github_pat_[A-Za-z0-9_]{40,}'),re.compile(r'glpat-[A-Za-z0-9_-]{20,}'),re.compile(r'AKIA[A-Z0-9]{16}'),re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----[\r\n]+[A-Za-z0-9+/=\r\n]{40,}-----END ')]
def redact(text,values=()):
    for value in sorted(set(values),key=len,reverse=True):
        if len(value)>=4:text=text.replace(value,'[REDACTED]')
    for pattern in PATTERNS:text=pattern.sub('[REDACTED]',text)
    return text

def scan(root):
    root=Path(root);count=0
    for name in SOURCE_ROOTS:
        base=root/name
        for path in (base.rglob('*') if base.is_dir() else [base]):
            rel=path.relative_to(root)
            if '__pycache__' in rel.parts or rel.parts[:2]==('public-web','downloads'):continue
            if path.is_symlink():raise ValueError('Release inputs cannot be symbolic links: '+str(rel))
            if not path.is_file():continue
            if private_release_file(path):raise ValueError('Private credential file in release inputs: '+str(rel))
            data=path.read_bytes()
            if b'\0' in data:continue
            text=data.decode('utf-8',errors='ignore')
            if any(pattern.search(text) for pattern in PATTERNS):raise ValueError('Possible secret in release input: '+str(rel)+' (value withheld)')
            count+=1
    return count
