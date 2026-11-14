# BitSkim 0.2 for Windows

Project repository: [retrocs-org/bitskim](https://github.com/retrocs-org/bitskim). Website: [bitskim.retrocs.org](https://bitskim.retrocs.org).

Portable x64 build targeting Windows 10. Extract the entire ZIP, then run `BitSkim.exe`. The portable build requires no installation or separate MinGW runtime download; an all-users Program Files setup installer is also provided. Setup requests administrator rights; the app runs as an ordinary user. The executable is unsigned; distribution signing remains a release task.

## Sending and receiving

Run BitSkim on computers on the same trusted IPv4 LAN. The main window shows each computer as a rounded tile with its name, platform and address. Drop one ordinary file directly onto a tile to send to that recipient; the tile highlights during the drag. Prior selection is not required. The grid scrolls for more computers and supports keyboard selection. Alternatively, select a tile and choose **Send file**, or enter an IPv4 address (optionally `:port`). Dropping outside the tile grid uses the selected computer or manual address; blank spaces inside the grid do not choose a recipient. Transfers use TCP 46440; discovery uses UDP 46440. Allow BitSkim through Windows Firewall on your private network when prompted. The app does not change firewall rules itself. Manual IP requires a working route and permitted incoming connections.

During a transfer, the panel shows the filename, transferred/total size, percentage, remaining data and recent payload speed. Binary units use KiB/MiB/GiB. Speed samples update about once per second and become zero during a stall. The sender counts complete payload frames written to its socket; final delivery still requires receiver confirmation.

Incoming files require approval and a save location by default. A successful free-space check continues without an extra confirmation; insufficient space blocks the transfer, while an unavailable free-space query asks before continuing interactively. Preferences can enable automatic acceptance and a remembered folder. Auto-accept still asks for a location when **Always ask where to save** is enabled. Files are never opened automatically. Existing destinations are preserved; unattended transfers choose an unused numbered filename.

TLS defaults to Prefer encryption. Preferences offer Disable encryption, Prefer encryption and Force encryption. Automatic TLS needs no copied key; an optional matching pairing key authenticates peers. Sender names remain unverified. Disk-backed queues transfer one file at a time, maximum 2 GiB minus one byte each. Resume is negotiated with capable clients; folder collections are negotiated with updated clients. See [Transfer tools](TRANSFER-TOOLS.md) for folder selection and disk-space checks. See [TRANSFERS.md](TRANSFERS.md). Amiga receivers require Latin-1 filenames of at most 30 characters. Windows receivers reject reserved device names, invalid Windows characters and trailing dots/spaces; rename such files on the sender first.

## Transfer tools

Transfer tools uses the selected application palette for its background, diagnostic report, inputs, buttons and conflict list. Saved theme changes apply while it is open. Live diagnostic updates preserve the report’s scroll position and text selection; unchanged text is not replaced or repainted. Standard window/control chrome remains provided by Windows.

## Tray, preferences and appearance

Closing the main window keeps discovery and reception running when **Keep running in the tray** is enabled (the default). Use the tray icon to show the window, open preferences or quit. **Start minimized to the tray** controls future launches; it does not register a Windows login item. Incoming requests that need your decision bring up a prompt.

Preferences has a compact sidebar with General, Appearance, Receiving, Startup and About sections. Start minimized requires tray mode; disabling tray mode clears that option. Save commits all sections; Cancel discards the draft; Revert restores the opening settings. Eight shared themes default to Catppuccin Mocha. The appearance preview shows the draft theme before saving. Standard Windows menus, file dialogs and consent prompts retain system styling.

Settings and the generated installation ID are saved per user in `%APPDATA%\BitSkim\preferences.ini`. Unicode file paths use Windows wide-character APIs. Core paths are limited to 1,023 UTF-8 bytes, regardless of Windows long-path settings. Only one app instance runs per user session.

## Building

### Prerequisites and application

Use Python 3, a POSIX shell and **MinGW-w64 GCC plus windres targeting x86_64 Windows**. The default commands are `x86_64-w64-mingw32-gcc` and `x86_64-w64-mingw32-windres`. Cross-building on macOS/Linux is the recorded workflow. Windows itself needs a suitable environment such as an MSYS2 shell with the x64 MinGW tools; invoking this .sh script directly in PowerShell/Command Prompt is not supported, and a native Windows-host build has not been separately validated.

From the repository root:

```sh
./scripts/build-windows.sh
# equivalent: make windows
```

For differently named tools, provide both overrides. For example, in a POSIX environment where the active gcc/windres already target x64 Windows:

```sh
WINDOWS_CC=gcc WINDOWS_RC=windres ./scripts/build-windows.sh
```

Do not point WINDOWS_CC at a host Linux/macOS compiler. The script sets Windows 10 API definitions, compiles the committed resource/icon file with windres and builds with warnings as errors. Its static-runtime options avoid shipping separate MinGW helper DLLs; normal Windows system DLLs are still used. It is not a 32-bit x86 or native Windows ARM64 build.

Outputs:

- `build/windows/BitSkim.exe`: native Win32 GUI application targeting Windows 10 x64.
- `build/windows/BitSkim-CoreCheck.exe`: console startup/network/filesystem self-check.
- `build/windows/resources.o`: intermediate resource object.

Generated ICO/theme headers are committed. Ordinary builds need no Pillow or icon-generation service. The C application has no .NET, Electron, CMake or npm dependency.

### Per-user setup

Build the Windows applications first, then provide **NSIS/makensis** and its standard includes/plugins:

```sh
./scripts/build-windows-installer.sh
```

For an isolated NSIS installation:

```sh
NSISDIR=/path/to/nsis/share/nsis MAKENSIS=/path/to/makensis ./scripts/build-windows-installer.sh
```

Set NSISDIR only when your makensis installation needs that data-directory override. Output: `outputs/BitSkim-0.2-Windows10-x64-Setup.exe`. Setup installs in Program Files for all users, unsigned and has an uninstaller; it does not install a login item or firewall rule. NSIS 3.13 is the recorded packaging tool, not a promise that every NSIS version has been tested.

Portable ZIP creation is part of `python3 scripts/package.py`, which currently requires all primary platform builds, not only Windows. See [BUILDING.md](BUILDING.md) for source fingerprints and existing setup/DMG dependencies.

### Runtime self-check

On Windows, run `BitSkim-CoreCheck.exe` from build/windows with BitSkim.exe beside it, or use the matching Tools executable inside the complete portable package. Keep the package layout intact so it can find the GUI for its startup check. The test starts short-lived local listeners and temporary-file transfers; it is separate from manual UI/firewall/installer testing. See the validation section below and [VALIDATION.md](VALIDATION.md).

## Validation

The x64 executables cross-build with MinGW-w64 GCC 15.2, `-Wall -Wextra -Werror`. The shared-engine host tests pass. `Tools\BitSkim-CoreCheck.exe` is a console self-check using temporary files and two local clients. It first launches the actual GUI executable in `--check-startup` mode, verifying its image loads and initializes Windows controls/networking without opening a window or saving settings. It then verifies Winsock transfers, Unicode paths, an empty file, exclusive creation, reserved filenames and collision preservation. It removes its test files on success.

Native 0.2 self-check: all 12 checks passed on Windows 11 ARM running the x64 executable on 14 November 2026, including actual GUI initialization, Winsock/automatic TLS, Unicode paths, zero-byte files, a 24-file collection, folder/empty-directory transfer, disk-space queries and collision preservation. The native visual review identified high-DPI startup-window overflow and clipped Transfer tools controls; startup placement now fits the monitor work area and Transfer tools scales its controls and anchors the report/footer to the client size. Windows 10 runtime/UI, installer interaction, cross-platform Windows-to-Mac/Amiga transfers, firewall prompts and broader real-network testing remain separate checks. Other Windows architectures and older Windows versions are not included in this package.

## Setup installer

Setup.exe installs under %ProgramFiles%/BitSkim by default, provides Start Menu and optional desktop shortcuts, and registers an uninstaller. AppData settings and received files are preserved on removal. The portable ZIP remains supported. Close the tray app before updating. See [INSTALLATION.md](INSTALLATION.md).

The first modern build bootstraps pinned OpenSSL 3.5.9 static libraries using `scripts/build-openssl.sh windows` (Perl, make, curl and the x64 MinGW compiler required). No OpenSSL DLL installation is needed. The Windows Classic build does not link OpenSSL. Use the Queue button for pause/retry/skip/clear; multi-select in the send picker or drop multiple files onto a recipient tile.

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls and Atari its existing queue page, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

Settings remain in `%APPDATA%\BitSkim\preferences.ini` (Roaming AppData), independent of the program folder. Older registered per-user installations must be uninstalled before the all-users installer proceeds; settings are preserved.
