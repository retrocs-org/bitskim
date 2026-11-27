"""Generated build identity: source content, independent of checkout path or Git."""
from pathlib import Path
from build_provenance import source_hash

def write_identity(root):
 root=Path(root).resolve();identity=source_hash(root)[:12];path=root/'build/generated/build-id.h';path.parent.mkdir(parents=True,exist_ok=True)
 text='#define BS_BUILD_ID "'+identity+'"\n'
 if not path.is_file() or path.read_text()!=text:path.write_text(text)
 return path
