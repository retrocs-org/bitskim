# Windows Classic 0.2 (experimental)

Target: 32-bit Windows 95, 98, Me and XP. Cross-compilation and executable
inspection pass; none of these operating systems has been runtime tested.
This is a separate native ANSI Win32 client, not the Windows 10 executable.

Extract the entire portable ZIP to a writable directory, then run BitSkim.exe.
TCP/IP must already be installed and configured. The client uses Winsock 1.1
(WSOCK32.DLL), TCP/UDP 46440, and the Microsoft MSVCRT.DLL runtime. Very early
Windows installations may need an appropriate licensed Microsoft runtime
installed first; no system DLLs are bundled or replaced by this package.
No .NET, UCRT, Winsock 2, browser runtime or installation service is required.

Select a nearby computer and use Send file, or drop one file into the main
window after selecting a computer. Alternatively enter a numeric IPv4 address,
optionally followed by :port. Directories and multiple files are rejected.
Discovery uses limited broadcast; manual IP is available where routers,
firewalls or multiple network interfaces prevent discovery.

Preferences include client name, visibility, incoming transfers and automatic
offer acceptance. Incoming files always prompt for a save location, including
when auto-accept is enabled. Existing destination files are never replaced.
Settings and the persistent peer ID are in the per-user AppData/BitSkim Classic/preferences.ini, separate from Program Files. Existing portable BitSkim.ini is migrated once when present.
Delete that file to reset preferences. Quit and delete the extracted directory
to uninstall. The portable ZIP creates no registry entries or boot-start items.

The interface uses classic controls and eight themes shared with the modern
Windows client, defaulting to Catppuccin Mocha. Theme selection previews colours
immediately; Save persists the choice and Cancel restores the previous theme.
Window frames, system menus and file/message dialogs retain Windows styling.
Remembered save folders are not implemented. Progress reports bytes transferred, total,
remaining bytes and recent bytes/s. Transfers are unencrypted and names are
unverified. The protocol and 2,147,483,647-byte per-file limit match other
clients. Filesystem paths are shorter than MAX_PATH and must round-trip through
the system ANSI code page; unrepresentable names are rejected rather than
silently changed. Prefer simple ASCII names during initial testing.

## Build

On a host with 32-bit MinGW-w64 GCC/binutils and Python 3:

```
./scripts/build-windows-legacy.sh
python3 scripts/package-windows-legacy.py
```

Overrides: LEGACY_WINDOWS_CC and LEGACY_WINDOWS_RC. The import check currently
uses i686-w64-mingw32-objdump. The build selects i486 instructions, PE32 GUI
subsystem 4.0, the old MSVCRT import library, 32-bit time_t, and a small explicit
entry point. It intentionally omits modern MinGW CRT startup and static TLS.
Do not replace this with the default compiler startup: the host toolchain
otherwise introduces UCRT dependencies even with a low WINVER.

The import gate checks architecture, subsystem, TLS and DLL dependencies. It
is a regression check, not proof that every function works on each OS.

## Required runtime checks

For each OS separately: launch with TCP/IP available/unavailable; test
preferences persistence; discovery and manual connections; send and receive
with current macOS/Amiga clients; compare file checksums; decline and cancel;
confirm no overwrite and removal of partial files; unplug a peer; test local
non-ASCII names, long paths, a busy receiver, multiple drops and folders.
Check UI at 640x480 and 800x600, and verify displayed icon transparency.

Compatibility references: Microsoft's description of
[Windows 95 Unicode limitations](https://www.bitsavers.org/pdf/microsoft/windows_95/Programmers_Guide_to_Microsoft_Windows_95_1995.pdf)
and [Win32 ANSI/Unicode APIs](https://learn.microsoft.com/en-us/windows/win32/learnwin32/working-with-strings).

Project: https://github.com/retrocs-org/bitskim
Website: https://bitskim.retrocs.org

## Installer

Build the application first, then run
`./scripts/build-windows-legacy-installer.sh` (requires NSIS 3 with ANSI stubs;
set MAKENSIS to override the compiler). Output:
`outputs/BitSkim-0.2-Windows-Classic-x86-Setup.exe`.

The installer uses ANSI x86 output because [NSIS Unicode installers cannot run
on Windows 95/98/Me](https://nsis.sourceforge.io/Docs/Chapter1.html).
It offers a destination chooser and optional desktop shortcut, adds Start menu
shortcuts and a machine-wide Add/Remove Programs entry named BitSkim Classic.
The default folder is Program Files/BitSkim Classic (Program Files (x86) on
64-bit Windows). Setup requests administrator approval where supported. Settings
are stored in per-user Shell AppData/BitSkim Classic/preferences.ini; unprofiled
Windows 95 falls back to Windows/BitSkim Classic/preferences.ini. Existing old
BitSkim.ini files are copied once, never overwritten. Older registered per-user
installs must be uninstalled before this installer proceeds.

Close the client before updating or uninstalling. Updates reuse the recorded
installation folder. Uninstall removes only the packaged files and its shortcuts
and registry entry, preserving BitSkim.ini, received files and unknown contents.
No boot-start entry or system DLL is installed. The setup is unsigned. Setup
and uninstaller are cross-built and payload-checked; native installation,
update and uninstall on Windows 95, 98, Me and XP remain untested.

## System tray

The tray icon is added at startup. Double-click it to restore the main window;
right-click for Show, Preferences, Refresh computers, Cancel transfer or Quit.
The main window also has a To tray button. Preferences includes Start hidden in
the system tray (off by default) and Close window to the system tray (on by
default). Start hidden controls the next launch; it does not add a boot-start item.
Discovery and file reception continue while hidden. Incoming offers restore the
window for approval/save selection. Use Quit in the tray menu to exit.

The client uses the Windows 95 V1 notification structure, restores its tray icon
when Explorer announces TaskbarCreated, and keeps/restores the window if adding
the tray icon fails. Host mock tests exercise the failure fallback; native tray,
keyboard navigation and theme rendering need verification on each target OS.

## File queue

Multiple Explorer drops or repeated sends append to a disk-backed sequential queue. The Queue button offers continue/retry, pause-after-current, skip-idle and clear-waiting. Failed or declined items pause. This build remains plaintext/non-resumable and does not need OpenSSL.

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls and Atari its existing queue page, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

All current builds advertise a versioned BXC1 network capability profile in discovery and confirm directional features/limits in the live handshake. This build reports plaintext-only transport, its implemented filename policy/limits, 2 GiB-minus-one file limit, and available collection approval. Unsupported TLS, resume, directory transfer and active-file pause are not advertised. Capability exchange does not replace receiving approval. See PROTOCOL.md.
