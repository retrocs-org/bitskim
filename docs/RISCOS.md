# RISC OS 3.x experimental port — BitSkim 0.1

Project repository: [retrocs-org/bitskim](https://github.com/retrocs-org/bitskim). Website: [bitskim.retrocs.org](https://bitskim.retrocs.org).

Two build targets are included: **ARM2/APCS-26** for an expanded A3000-class machine with RISC OS 3.1, and **StrongARM/APCS-26** for a RiscPC with a StrongARM-aware OS (normally RISC OS 3.7). These are intended targets, not certified compatibility. Both binaries compile and link; neither has been run on RISC OS. The project owner currently has no RISC OS test system. Do not infer runtime support from the successful builds.

## Requirements and installation

A full Internet-module-compatible IPv4 TCP/IP stack and a working network driver are required. RISC OS 3.1 does not supply this by itself. The partial Internet module shipped in RISC OS 3.5 is insufficient; the full stack arrived in 3.6. Third-party/older stacks need individual compatibility checks. See the [Internet module PRM](https://www.riscos.com/support/developers/prm/internet.html) and [StrongARM-aware OS release information](https://www.riscos.com/ftp_space/index.htm).

The launcher requests a 1024 KiB Wimp slot. Start with an expanded machine (4 MiB or more is a sensible testing configuration); minimum memory and performance have not been measured. An unexpanded A3000 is not a validated target. The UI uses native Wimp controls and a scrollable work area rather than modern toolkit libraries.

Choose the appropriate archive, extract using a RISC OS-aware ZIP utility that preserves Acorn filetype extra fields, and copy **!BitSkim** to a writable disc. Do not merge the ARM2 and StrongARM applications. Double-click !BitSkim. !RunImage must have filetype **Absolute (&FF8)**; !Run/!Boot are **Obey (&FFB)** and !Sprites is **Sprite (&FF9)**. Ordinary host ZIP extraction can lose these filetypes; restore them before launching if needed.

Select a peer and drop one existing disc file from the Filer into the main window, or type a full native path in **Send path** and press **Send file**. Alternatively enter an IPv4 address with optional :port. Discovery uses UDP 46440; transfers use TCP 46440. Directories and application-generated scrap/RAM transfers are not implemented. There is no separate native file-picker dialog in this initial port.

Incoming offers show the original filename and an editable full native save path. Accept or decline them. FileCore-compatible proposed leafnames are shortened to ten Latin-1 characters; dots become native slashes. Collisions get a short numeric suffix. Names containing characters outside Latin-1 are declined, with a request to rename on the sender. Edit the proposed path if the shortened name is unsuitable. File contents are preserved; RISC OS filetypes/load/exec metadata are not transferred.

Preferences are saved as **Choices inside !BitSkim**, including the persistent peer ID. Install on writable storage, save Preferences and avoid copying an already-configured application to another machine without removing its Choices file. The default incoming folder is **<Wimp$ScrapDir>**: choose a persistent folder before using unattended auto-accept. Always ask takes precedence over auto-accept.

Select the icon-bar icon to restore the main window; Menu opens Preferences. Start on the icon bar and Close window to icon bar are optional. Use the main window's Quit button to exit. Hidden mode pauses outgoing discovery searches and display work, while retaining incoming transfers and discovery replies. Eight theme choices approximate the current desktop's sixteen Wimp colours; they do not reproduce the full RGB palettes or change other applications' colours. Mocha is the default.

Transferred/total bytes, percentage, remaining bytes and recent payload bytes/second appear during sending and receiving. One transfer is active at a time. Transfers are unencrypted and peer names are not authenticated. The shared protocol's size ceiling is 2 GiB minus one byte; old filesystem and memory limits have not been validated at that boundary.

## Building

These commands require a POSIX host, Python 3 and a previously configured legacy ARM2/APCS-26 cross-toolchain. Toolchain installation is separate from the application build.


Use a **working legacy GCCSDK 2.95.4 AOF/UnixLib toolchain** with ARM2 and StrongARM APCS-26 support. A current ELF/EABI/32-bit RISC OS compiler is not an interchangeable replacement for these old machines. [GCCSDK source](https://github.com/riscos-dot-info/gccsdk/tree/release_2_95_4_v3) is pinned at tag release_2_95_4_v3 (tree 5afa678).

```sh
RISCOS_CC=/path/to/legacy/gcc scripts/build-riscos.sh
# or: RISCOS_CC=/path/to/legacy/gcc make riscos
```

Each target uses -mapcs-26 -mcpu=arm2 or -mcpu=strongarm, -munixlib, -Os and -D__riscos__. Output is build/riscos/<target>/!BitSkim/!RunImage, plus four relinkable AOF objects. Sources are clients/riscos/main.c, core/bitskim.c, core/platform.c and core/riscos-file.c. The last file uses native OS_FSControl 25 to commit without replacing an existing destination; UnixLib rename is unsuitable because it can remove the destination first.

The recorded macOS builds use the historical SDK with local host adaptations in scripts/gccsdk-2.95-host.patch. The compiler runs as x86_64 through Rosetta. AOF assembly and linking preserve the target APCS-26 format. GCC’s generated c-parse.h requires removal of its yytokentype enum while retaining the token defines.

For a configured SDK, the normal sequence is configure, make setup, build the C compiler/AOF tools, then make libgcc unixlib. Historical configure needs Autoconf 2.13. The host patch limits the runtime build to C support, omitting unused C++ exception/allocation objects. Do not use a 64-bit-host Drlink simply because it compiles successfully.

### Outputs, packaging and host limitations

The script builds both CPU variants on every invocation. Outputs are `build/riscos/arm2/!BitSkim/` and `build/riscos/strongarm/!BitSkim/`, with `main.o`, `bitskim.o`, `platform.o` and `riscos-file.o` alongside each application. It preserves the committed !Run/!Boot/!Sprites files and normalizes a compiler-produced !RunImage,ff8 filename to !RunImage.

Building an AIF image does not attach the native filetype to an ordinary host filesystem file. Use the project's ZIP packaging to add Acorn filetype metadata. After building both RISC OS variants and refreshing the Amiga/source release, run `python3 scripts/package-riscos.py` with `RISCOS_SDK_SOURCE=/path/to/gccsdk295.tar.gz` if needed. This standalone packager does not require rebuilding unrelated clients. The combined `scripts/package.py` also uses the same RISC OS writer. It produces `outputs/BitSkim-0.1-RISCOS3-ARM2.zip` and `outputs/BitSkim-0.1-RISCOS3-StrongARM.zip`, including notices, source/relink material and build records. See [BUILDING.md](BUILDING.md) for optional-target freshness checks.

### Source and relinking contents

Each experimental binary ZIP contains **Relink/ProjectSrc** (the complete BitSkim source ZIP), **Relink/SDKSrc** (the original GCCSDK source tar.gz), **Relink/HostPatch**, the four application AOF objects, and runtime license texts. On a build host, give the source archives .zip and .tar.gz extensions before extracting. Build the SDK's UnixLib/libgcc, replacing or modifying its sources as desired, then link the supplied objects or rebuild the application:

```sh
/path/to/legacy/gcc -mapcs-26 -mcpu=arm2 -munixlib \
  Client CoreObj Platform NativeFS -o '!RunImage'
# Use -mcpu=strongarm for the StrongARM package.
```

The objects correspond to main.o, bitskim.o, platform.o and riscos-file.o respectively. Use the same APCS-26 ABI for all objects and runtime replacements. Restore the Absolute filetype after transferring the linked image to RISC OS.

Packaging includes Acorn ZIP filetype metadata and requires the original SDK archive. Set RISCOS_SDK_SOURCE to its path if it is not at work/riscos/gccsdk295.tar.gz. Recorded source archive SHA-256: afe2f5cbf5c199288b6c548c125c9534aa04eaf82efe9384d6a76c01b28e3292. Regenerate the sprite with Python + Pillow using scripts/build-riscos-icon.py; committed sprites mean ordinary builds do not require Pillow.

## What remains to test

Native startup on both CPU classes; Wimp rendering, caret/editing and mouse actions; icon-bar lifecycle; preferences persistence; Filer drops; actual Internet module and driver behavior; discovery across real interfaces; accept/decline/cancel and checksum-confirmed transfers in both directions with Mac, Windows and Amiga; no-replace native rename behavior; short-name collisions and full discs; low memory and extended transfers. The host tests exercise the real engine's RISC OS encoding/short-temp-name policy, with a host substitute for FileSwitch commit. They do not execute Wimp, UnixLib sockets or a native FileSwitch SWI.

The experimental client now queues repeated file paths/Filer drops, with Retry, Pause, Skip and Clear controls. Queue recipients use stable IDs. TLS and resume are not built for this target. These changes require native ARM2/StrongARM runtime validation.

## Resumed queue builds — 2026-11-02

Both ARM2 and StrongARM APCS-26 binaries have now been rebuilt using the restored Windows linker connection. Those earlier release ZIPs included the initial eight-file queue (superseded below) and Continue/Retry, Pause, Skip and Clear controls. A host test checks dispatch with non-sequential Wimp icon handles; queue controls use internal indexes consistently. Native RISC OS execution remains untested.

```sh
RISCOS_CC=/path/to/legacy/gcc scripts/build-riscos.sh
scripts/release-amiga.sh
python3 scripts/package-riscos.py
python3 scripts/sync-amiga-bin.py
```

The standalone packager verifies both build records, preserves Acorn/SparkFS filetypes, includes source/relink material and refreshes source/download checksums. It does not publish anything.

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls and Atari its existing queue page, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

All current builds advertise a versioned BXC1 network capability profile in discovery and confirm directional features/limits in the live handshake. This build reports plaintext-only transport, its implemented filename policy/limits, 2 GiB-minus-one file limit, and available collection approval. Unsupported TLS, resume, directory transfer and active-file pause are not advertised. Capability exchange does not replace receiving approval. See PROTOCOL.md.

## Transfer tools

Open Tools... in the main window for diagnostics, recent session history and the Rename/Skip/Replace conflict policy. See [Transfer tools](TRANSFER-TOOLS.md) for filename mapping, verification and legacy-peer behavior. Native runtime validation is recorded separately.

## Filer integration

Drop a durable Filer file or directory onto a peer row to select that recipient
and queue it, or onto the icon bar with a recipient already selected. Dropping
an existing directory on the Preferences Folder field or on an incoming
collection destination field fills that path; it does not approve the offer.
BitSkim acknowledges only successfully queued or selected Filer paths. It
ignores temporary scrap and other application DataSave transfers whose source
could disappear before the network queue reads it. See the [Wimp data transfer
protocol](https://www.riscos.com/support/developers/prm/wimp.html).

Nearby Filer drops are collected briefly before starting the queue, unless it
was already paused. Completion sound is optional in Preferences. Both CPU
builds compile/link; native Wimp/Filer/audio testing remains pending.

## Release rebuild on this host — 11 November 2026

Both variants were rebuilt with the historical SDK source from the official
`release_2_95_4_v3` tag. The freshly downloaded codeload archive has SHA-256
`f9c63a4e57519586624deea48739d51ef396a5316115472a1e36936bf19ae7d6`.
The earlier archive hash above identifies the earlier archive, not this new
compressed container. The complete matching SDK source is included in Relink/SDKSrc.

The host compiler runs under Rosetta as x86_64. Additional modern-host fixes
for fortified stpcpy and the C++ namespace/library are included in the refreshed
host patch. Generate c-parse.h with the historical build, then remove its
`enum yytokentype` declaration while keeping token defines, as noted above.
Build `libiberty-stg1`, `riscos-aof-stg1`, `gcc-stg1`, `libgcc`, and `unixlib` in
that order; do not run the full stage-2/native toolset build just to link this app.

The current development linker uses wasm32-WASI with 32-bit pointers, preserving its on-disc structure assumptions. The WASI runner needs access to the project and temporary response files. It does not execute RISC OS applications.