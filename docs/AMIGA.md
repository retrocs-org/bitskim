# AmigaOS build and release — BitSkim 0.2

## Target and toolchain

The executable instruction target is **68000**, for AmigaOS 3.x/Workbench. The networked main app requires an AmiTCP-compatible bsdsocket.library v4 provider; standalone Preferences needs no TCP/IP stack. Real-hardware CPU/RAM/performance validation remains separate from cross-compilation.

Use a POSIX build host with Python 3 and an **AmigaPorts m68k-amigaos GCC toolchain** containing the Amiga NDK, network headers, libamiga and libnix's nix20 runtime. The script explicitly uses `-m68000 -mcrt=nix20`; an arbitrary m68k/ELF compiler or compiler without that runtime is not interchangeable. The project has recorded AmigaPorts 16.2-rc22 builds; toolchains are external and not included in the source archive.

```sh
AMIGA_CC=/path/to/m68k-amigaos-gcc ./scripts/build-amiga.sh
# If the compiler is on PATH:
make amiga
```

This compiles the C engine/client twice, once for BitSkim and once with BITSKIM_PREFS for BitSkimPrefs. It also builds the 68000 StartInstaller launcher, generates the Installer script, copies native Workbench icons and builds the AmigaGuide/installation icon templates. Python standard-library tools suffice for normal builds.

Outputs under `build/amiga/`:

- BitSkim and BitSkimPrefs, with their distinct application .info files.
- Install-BitSkim, its project icon, StartInstaller and the small StartBitSkim boot launcher.
- BitSkim.guide and its MultiView project icon.
- InstallAssets settings-drawer, settings-project and Startup.info templates.
- BitSkimDrawer.info, used as the distribution drawer icon.

The build creates a distribution without installing it or changing network settings. Network setup tools are maintained separately.

## Compile and package a release

```sh
AMIGA_CC=/path/to/m68k-amigaos-gcc ./scripts/release-amiga.sh
# equivalent make target, with AMIGA_CC exported or on PATH:
make amiga-release
```

The release script can also recognize this development workspace's local toolchain path; that is a convenience, not a dependency shipped in source. It builds only Amiga, verifies the payload and creates:

- outputs/BitSkim-0.2.lha
- outputs/BitSkim-0.2-AmigaOS.zip
- outputs/BitSkim-0.2.lha.sha256 and updated SHA256SUMS
- outputs/aminet/bitskim.readme.template, a non-submittable draft

The ZIP and LHA share one manifest in scripts/amiga_release.py. Both contain the complete distribution, including installer, manual, licenses, ARexx examples, InstallAssets and the outer drawer icon. The LHA uses generic level-0 headers and LH5 compression (LH0 for files that do not compress). CRC testing, extraction and a full path/hash comparison run before accepting the output. Version remains 0.2.

### LHA tool requirements

The writer must be LHa-for-UNIX. Lhasa, often installed as `lha`, is extraction-only.

```sh
LHA=/path/to/writer/lha AMIGA_CC=/path/to/m68k-amigaos-gcc ./scripts/release-amiga.sh
```

If no writer is available, release-amiga.sh invokes scripts/bootstrap-lha.sh. This downloads a pinned upstream source archive, checks SHA-256 and builds into work/lha-tool/. It requires network access, curl, tar, Python 3, host cc, make and GNU autoreconf/autoconf/automake. It does not install or replace system commands. The pin/checksum and upstream URL are in the bootstrap script. LHA_READER can select a separate reader for extraction validation; otherwise the installed lha reader or the writer is used.

## Installed layout and migration

Novice uses standard paths, enables its owned WBStartup launcher and starts the app after installation. Average/Intermediate selects the two executable directories; Expert also selects the shared-data parent. Both ask about boot startup. Fresh installs also ask about immediate launch; updates restart automatically. The three selectable locations are the exact main executable drawer, exact Preferences executable drawer and shared parent. Defaults place executable/icon pairs directly in SYS:Utilities and SYS:Prefs, while the shared parent SYS: gets an iconless BitSkim subdrawer. The guide uses fixed HELP:BitSkim.guide and .info; live/saved settings use ENV:BitSkim/BitSkim.prefs and ENVARC:BitSkim/BitSkim.prefs.

Use writes live settings; Save writes live and permanent settings. Existing installation records restrict the installer to Update/Uninstall. See [INSTALLATION.md](INSTALLATION.md) for directory conflicts, old-layout migration, icons, preserving user files and native validation limits.

## Optional Aminet publication

Preparation, validation and the separately confirmed future upload are documented in [AMINET.md](AMINET.md). Normal release builds only create a draft template. They never upload, install into AmigaOS or modify the test system. Public upload requires a real uploader email, a prepared matching pair and `--aminet-upload --confirm-upload`.

## Runtime checks

Extract the complete archive on Amiga, verify both app icons and the guide, exercise install/update/uninstall and test Use versus Save across a reboot. Verify networking, iconify, section switching and cross-client transfers separately. Host compiler/tests and host LHA extraction are not substitutes for these checks. See [VALIDATION.md](VALIDATION.md) for exact recorded coverage.

Installer levels: Novice uses default paths and enables boot/immediate startup automatically. Average (Intermediate) chooses the main app and Preferences drawers; Expert additionally chooses the shared-data parent. Help and configuration paths remain fixed. Existing registered installations reuse their directories. Intermediate and Expert get boot-start questions during install/update and a launch-now question on a fresh install; updates always restart the installed app. Uninstall does not ask these questions. Immediate launch uses a temporary helper and works independently of the boot-start choice.

## Development Bin synchronization

The release script also creates the complete source ZIP and `BitSkim-src.tar.gz`. Set `AMIGA_DEV_BIN` to the host directory mounted as Bin: to automatically refresh `Bin:BitSkim/`, `Bin:BitSkim.info`, `Bin:BitSkim-0.2.lha` and `Bin:BitSkim-src.tar.gz` after a release. A workspace-local `work/amiga-dev-bin.txt` can store the same path without committing a personal host path. This workspace uses that setting as the owner's requested after-each-change workflow.

To sync already-current releases independently, run `python3 scripts/sync-amiga-bin.py --bin-dir '/path/to/AMIGA_DEV_BIN'`. It checks source/build provenance, archive checksums, ZIP contents and the LHA's embedded build record before copying, verifies written bytes and backs up replaced files under work/bin-sync-backups/. Existing application/drawer icon positions are retained while artwork and view flags update. Unknown Bin files/projects and system configuration are untouched. This updates the development distribution only; it does not install into the guest's SYS: directories or change WBStartup.

## File queues

Drop several files or add individual send paths; the local queue uses temporary-file storage. Queue controls provide explicit retry, pause, skip and clear-waiting actions. Updated peers can approve an entire displayed batch once; older peers ask per file. TLS and resume are limited to capable modern builds; Amiga keeps plaintext interoperability. See [TRANSFERS.md](TRANSFERS.md).

### Compact transfer display

The main window shows a single progress bar with the current filename, transferred/total size, percentage, remaining size and speed. It starts afresh for each file. The outgoing waiting count excludes the active send. Outgoing FIFO transfers also show the current file ordinal (for example 2/3); skipped files count as processed and a new queue after the previous one drains starts at 1 again. Negotiated incoming batches show the current filename, file position and remaining count. Older single-file offers have no remote queue count.

There is no separate queue window or clickable progress row. Less-used outgoing controls are in the main window's native Queue menu (hold the right mouse button): Continue/Retry, Pause after file, Skip idle file and Clear waiting. Pause finishes the current file before holding the next. Cancel remains in the main window. The display retains the last result while idle, uses fixed memory and avoids repaints while iconified.

### Amiga update lifecycle

Close BitSkimPrefs before running the installer. After update selection, confirmation and path checks, the package-only StopBitSkim helper requests normal Ctrl-C shutdown of all registered BITSKIM/BITSKIM.1 through BITSKIM.99 clients. Active transfers are cancelled and the in-memory waiting queues are discarded. The updater waits up to ten seconds for both message ports and owning tasks to disappear; it aborts before replacing program files if shutdown fails. It does not forcibly terminate processes. After successful copying and recording of installation paths, updates always launch one copy of the newly installed application, using its saved window/iconify preferences. Fresh installations retain the existing startup choices. A failed or aborted update does not start a partly updated application. StopBitSkim stays in the distribution and is not installed into Utilities, Prefs or the shared drawer.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

All current builds advertise a versioned BXC1 network capability profile in discovery and confirm directional features/limits in the live handshake. This build reports plaintext-only transport, its implemented filename policy/limits, 2 GiB-minus-one file limit, and available collection approval. Unsupported TLS, resume, directory transfer and active-file pause are not advertised. Capability exchange does not replace receiving approval. See PROTOCOL.md.

## Preferences sections

General contains the client name, optional Start iconified and Close window to
iconify settings, Automatically accept offers, Always ask where to save and
Folder. Both iconify options default off; explicit saved choices are respected.
Missing iconify keys reset to off on reload. Network contains Visible on network
and Allow incoming transfers. Appearance and About retain their existing roles.
For an existing installation with either iconify option saved on, uncheck it in
General and Save to change the permanent preference (Use changes this session).

## Transfer tools

Open Tools → Transfers / Diagnostics for diagnostics, recent session history and the Rename/Skip/Replace conflict policy. See [Transfer tools](TRANSFER-TOOLS.md) for filename mapping, verification and legacy-peer behavior. Native runtime validation is recorded separately.
