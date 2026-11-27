# Windows 3.x development client

The experimental client targets Windows 3.1/3.11 with an installed Winsock 1.1 TCP/IP stack. It is a real 16-bit NE executable, built for 8086 large model; it does not require Win32s. It has nearby discovery, manual IPv4, file selection, receive approval, queue/cancel, progress and diagnostics. It uses plaintext and ASCII DOS 8.3 filenames; long or unrepresentable paths are refused. Folder-batch approval, TLS, pairing and resume are not offered by this port.

Build on Apple silicon using the shared Open Watcom installation:

```sh
python3 scripts/build-windows16.py
```

Use `--watcom PATH` for another installed Open Watcom tree. The script compiles the current shared core and checks NE format and allowed Win16 imports. It writes `build/windows16/BITSKIM.EXE`. Copy it to an isolated test Windows directory; select a nearby client or enter its IPv4 address, then choose Send file. Incoming files require approval and an unused ASCII/8.3 destination.

The compile/link/import checks passed on 2026-11-26. Native Windows 3.1/3.11 startup, a real Winsock stack, discovery, bidirectional empty/binary transfers, cancellation, disk-full behavior and keyboard navigation remain unverified. This executable is a development artifact and is not included in normal release assets yet.
