"""Current packaging dates without changing source or compiled build timestamps."""
import copy
import os
from pathlib import Path
import shutil
import time
import zipfile


def current_timestamp():
    timestamp = int(time.time())
    if not 1980 <= time.localtime(timestamp).tm_year <= 2107:
        raise ValueError('Build clock is outside the ZIP/LHA date range; correct the host clock before packaging')
    return timestamp


def stamp_tree(folder, timestamp=None):
    """Stamp a disposable package staging tree, never following symbolic links."""
    timestamp = current_timestamp() if timestamp is None else timestamp
    folder = Path(folder)
    for path in [*folder.rglob('*'), folder]:
        if not path.is_symlink():
            os.utime(path, (timestamp, timestamp))


class PackageZipFile(zipfile.ZipFile):
    """Keep file bytes, permissions and extra fields; use the packaging date."""
    def __init__(self, *args, **kwargs):
        self.timestamp = current_timestamp()
        self.entry_date_time = time.localtime(self.timestamp)[:6]
        super().__init__(*args, **kwargs)

    def write(self, filename, arcname=None, compress_type=None, compresslevel=None):
        info = zipfile.ZipInfo.from_file(filename, arcname, strict_timestamps=False)
        info.date_time = self.entry_date_time
        info.compress_type = self.compression if compress_type is None else compress_type
        info._compresslevel = self.compresslevel if compresslevel is None else compresslevel
        if info.is_dir():
            return self.writestr(info, b'')
        with open(filename, 'rb') as source, self.open(info, 'w') as destination:
            shutil.copyfileobj(source, destination)

    def writestr(self, zinfo_or_arcname, data, compress_type=None, compresslevel=None):
        if isinstance(zinfo_or_arcname, zipfile.ZipInfo):
            info = copy.copy(zinfo_or_arcname)
        else:
            info = zipfile.ZipInfo(zinfo_or_arcname, self.entry_date_time)
            info.compress_type = self.compression
            info._compresslevel = self.compresslevel
            info.external_attr = (0o40775 << 16) | 0x10 if info.is_dir() else 0o600 << 16
        info.date_time = self.entry_date_time
        return super().writestr(info, data, compress_type, compresslevel)
