"""Shared artifact directory selection, confined to ignored outputs/."""
import os
from pathlib import Path

def output_dir(root):
    root=Path(root).resolve();relative=Path(os.environ.get('BITSKIM_RELEASE_OUTPUT','outputs'))
    if relative.is_absolute() or '..' in relative.parts:raise ValueError('Release output must be under outputs/')
    path=root/relative
    if relative.parts[:1]!=('outputs',) or relative.parts[:2]==('outputs','release'):raise ValueError('Choose outputs/ or a directory under outputs/, excluding outputs/release/')
    cursor=root
    for part in relative.parts:
        cursor=cursor/part
        if cursor.is_symlink():raise ValueError('Release output cannot traverse symbolic links')
    return path

def select_output(root,value):
    path=Path(value).expanduser()
    if path.is_absolute():
        relative=None
        for base in (Path(root).absolute(),Path(root).resolve()):
            try:relative=path.relative_to(base);break
            except ValueError:pass
        if relative is None:raise ValueError('Release output must be inside this checkout outputs/')
    else:relative=path
    old=os.environ.get('BITSKIM_RELEASE_OUTPUT');os.environ['BITSKIM_RELEASE_OUTPUT']=str(relative)
    try:return output_dir(root)
    except Exception:
        if old is None:os.environ.pop('BITSKIM_RELEASE_OUTPUT',None)
        else:os.environ['BITSKIM_RELEASE_OUTPUT']=old
        raise
