#!/usr/bin/env python3
"""Package the experimental portable Windows Classic client."""
from build_provenance import ROOT
from windows_legacy_release import write_windows_legacy
write_windows_legacy(ROOT/'outputs/BitSkim-0.2-Windows-Classic-x86.zip')

import subprocess,sys
from pathlib import Path
subprocess.run([sys.executable,str(Path(__file__).resolve().parent/"sync-builds.py")],check=True)
