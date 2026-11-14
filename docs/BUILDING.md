# Build BitSkim from source

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

These are standalone compilation instructions. Run commands from the BitSkim source root; no release coordinator, repository tokens or 1Password configuration is needed. Version 0.2 remains the current application version.

## Before you start

- Install Python 3.9 or later and the compiler/toolchain for the target below.
- Run `.sh` scripts in a POSIX shell. On Windows use a suitable POSIX shell for those scripts; the native DOS command below uses Windows Python instead.
- macOS and modern Windows builds need Perl, make, curl and download access on the first build. Their scripts build pinned OpenSSL 3.5.9 locally under `work/`.
- Generated icons, themes and sprites are included. Ordinary app compilation does not require Node, npm, Cargo, Pillow or asset regeneration. The legacy RISC OS SDK may have its own host dependencies.
- Standalone commands do not load the external release configuration. Export the tool variables shown below in your shell; settings used by `scripts/release.py` do not automatically apply to a separate command.
- Keep installed compilers outside disposable source folders. Use environment variables to select them; never copy a machine-specific settings file into the public source tree.

Get the source by extracting the source archive or cloning:

```sh
git clone https://github.com/retrocs-org/bitskim.git
cd bitskim
```

## macOS — Apple silicon

Use an Apple silicon Mac with Xcode command-line tools/macOS SDK, Clang and Swift. The application targets macOS 13 or later.

```sh
./scripts/build-macos.sh
```

Output: `build/macos/BitSkim.app`. A local build defaults to ad-hoc signing. An installed Developer ID identity can be selected through `BITSKIM_SIGN_IDENTITY`; signing alone does not notarize an app. See [MACOS.md](MACOS.md).

## Android — experimental

On Apple silicon macOS, use JDK 21, Python, Perl, make and Xcode command-line tools. SDK/NDK, Gradle, their caches and custom OpenSSL builds remain in `~/Development/tools/android`. No release credentials are used.

```sh
brew install openjdk@21
python3 scripts/bootstrap-android.py --install-sdk
./scripts/build-android.sh
```

Output: `build/android/BitSkim-0.2-Android-debug.apk`, development-signed with an external debug key. `--discovery off` builds manual connections only. For local testing, install the runtime with `python3 scripts/bootstrap-android.py --runtime`, then use `./scripts/build-android.sh --run --avd BitSkim_API_37`. Use `--list-devices` and `--run --device SERIAL` for a connected device. See [ANDROID.md](ANDROID.md) for limits, permissions and the standalone run command.

## iPhone / iPad — experimental

Use an Apple silicon Mac with full Xcode/iOS SDKs, XcodeGen, Perl, make and curl. Targets iOS 16+; App Group provisioning and development signing are needed to install on a device. Default builds use manual connections; automatic broadcast discovery requires Apple's approved entitlement.

```sh
./scripts/build-ios.sh iphoneos --multicast=off
```

Output: unsigned `build/ios-derived/Build/Products/Debug-iphoneos/BitSkim.app` with embedded Share extension. This is source-only development work, outside the desktop/retro release pipeline. Run `./scripts/build-ios.sh --run` to build and launch a local iOS test device; use `--list-devices` to choose one. See [IOS.md](IOS.md) for signing, discovery and unit tests.

## Windows — x64

Provide MinGW-w64 GCC and windres targeting x86_64 Windows. On the first build, the OpenSSL dependency also invokes `x86_64-w64-mingw32-*` tools directly, so the complete MinGW bin directory must be in PATH even when `WINDOWS_CC` and `WINDOWS_RC` are set. For example:

```sh
export PATH="/path/to/mingw/bin:$PATH"
./scripts/build-windows.sh
```

Outputs: `build/windows/BitSkim.exe` and `BitSkim-CoreCheck.exe`. With the default compiler names in PATH, simply run `./scripts/build-windows.sh`. This is the Windows 10/11 x64 target; see [WINDOWS.md](WINDOWS.md) for runtime validation limits.

## AmigaOS — 68000

Provide AmigaPorts GCC, NDK and libnix with `-mcrt=nix20` support:

```sh
AMIGA_CC=/path/to/m68k-amigaos-gcc ./scripts/build-amiga.sh
```

Outputs under `build/amiga/` include `BitSkim`, `BitSkimPrefs`, the Workbench installer and icons. Target: AmigaOS 3.x, Workbench and an AmiTCP-compatible stack. See [AMIGA.md](AMIGA.md).

For LHA packaging, `./scripts/release-amiga.sh` reuses the installed compiler and writer. Standalone packaging and Aminet commands also discover the writer in `~/Development/tools/amiga/lha/bin/lha` or alongside the shared Amiga compiler. If needed, run `./scripts/bootstrap-lha.sh` to download and build the pinned writer under `~/Development/tools/amiga/lha/`, outside the checkout. Set `LHA_PREFIX` to change that installation directory, or `LHA` to select an existing executable. Lhasa is extraction-only.

## Atari — TOS/STinG and MiNTNet (experimental)

Use a compatible m68k-atari-mint GCC SDK with MiNTLib and GEMlib, plus the STinG SDK headers:

```sh
ATARI_CC=/path/to/m68k-atari-mint-gcc \
STING_INCLUDE=/path/to/sting-sdk/include \
./scripts/build-atari.sh
```

Alternatively, on macOS prepare the project's pinned local SDK first, then build with its defaults:

```sh
python3 scripts/bootstrap-atari.py
./scripts/build-atari.sh
```

The supplied macOS SDK has x86_64 host tools and needs Rosetta on Apple silicon. Outputs are `build/atari/sting/BITSKIM.PRG`, `build/atari/mintnet/BITSKIM.PRG` and `build/atari/mintnet060/BITSKIM.PRG`. These are separate TOS/STinG 68000, MiNTNet 68000 and MiNTNet 68060 builds. See [ATARI.md](ATARI.md).

## RISC OS — ARM2 and StrongARM (experimental)

Provide a working legacy GCCSDK 2.95.4 AOF/UnixLib toolchain with APCS-26 support. A modern ELF/EABI compiler is not a replacement for these targets.

```sh
RISCOS_CC=/path/to/legacy/gcc ./scripts/build-riscos.sh
```

Outputs: `build/riscos/arm2/!BitSkim/!RunImage` and `build/riscos/strongarm/!BitSkim/!RunImage`. Both CPU variants are built. SDK host setup, source, host patches, filetypes and relinking are described in [RISCOS.md](RISCOS.md).

Use a toolchain installed for its actual location. Relocating an old SDK can leave compiler/linker/header paths embedded in its host tools. The legacy StrongARM link can report object-area attribute warnings; successful linking does not establish runtime compatibility. Do not suppress them to claim validation.

## DOS — 8086 / mTCP (experimental)

Provide the Open Watcom C/C++ 16-bit DOS toolchain, including wcc, wpp, wasm and wlink. mTCP source is included.

On a POSIX build host, select the compiler installation and DOS build runner:

```sh
python3 scripts/build-dos.py \
  --watcom /path/to/open-watcom \
  --runner /path/to/dos-runner
```

The Windows-native route is also provided, but has not been validated:

```text
python scripts/build-dos.py --native --watcom C:\WATCOM
```

Output: `build/dos/BITSKIM.EXE`. Networking on the target needs a packet driver; see [DOS.md](DOS.md) for toolchain layout, runtime checks and GPL source requirements.

## Windows Classic — x86 (experimental)

Provide i686 MinGW-w64 GCC and windres, including the `libmsvcrt-os.a` import library required by the legacy link. A toolchain lacking that library will not link this client:

```sh
LEGACY_WINDOWS_CC=/path/to/i686-w64-mingw32-gcc \
LEGACY_WINDOWS_RC=/path/to/i686-w64-mingw32-windres \
./scripts/build-windows-legacy.sh
```

Output: `build/windows-legacy/BitSkim.exe`. The script checks the PE32 format, subsystem version and legacy DLL imports. Windows 95/98/Me/XP are targets; those binary checks do not prove runtime compatibility. See [WINDOWS-LEGACY.md](WINDOWS-LEGACY.md).

## Run the shared tests

On macOS/Linux, provide a host C compiler named `cc` and allow local socket access:

```sh
python3 -m unittest discover -s tests -b
```

`-b` buffers successful-test output; failing tests still show diagnostics. Some sanitizer, platform and optional-tool tests can skip when unsupported. Tests do not submit live releases or Aminet uploads. Native UI, installer, network-stack and hardware validation must be performed separately; see [VALIDATION.md](VALIDATION.md).

## Create local installers and archives

Build the corresponding application first and complete source edits before compiling. Packaging checks that build records match the current source. The fingerprint includes scripts, tests, documentation and website files, not only client code. Finish those edits before building; if they change afterward, rebuild every target you intend to package. Committing unchanged file contents does not require a rebuild.

### Windows installer

Install NSIS 3 and its matching data directory. Either put `makensis` in PATH, or provide both paths:

```sh
MAKENSIS=/path/to/nsis/bin/makensis \
NSISDIR=/path/to/nsis/share/nsis \
./scripts/build-windows-installer.sh
```

Output: `outputs/BitSkim-0.2-Windows10-x64-Setup.exe`. The optional classic installer uses the same NSIS settings with `./scripts/build-windows-legacy-installer.sh`; see its platform guide for ANSI-output requirements. A bare compiler binary without its Stubs/Include/Plugins data is insufficient.

### Mac disk image

The disk image builder needs Python's `ds-store` package. Use a Python environment with that dependency installed, then:

```sh
python3 scripts/build-macos-dmg.py
```

Output: `outputs/BitSkim-0.2-macOS-arm64.dmg`. See [MACOS.md](MACOS.md) for signing/notarization prerequisites; building a DMG does not publish it.

### Combined client, source and website archives

After all required application builds are current, run the standalone packager. Provide an LHA writer and the original RISC OS SDK source when those packages are included:

```sh
LHA=/path/to/lha \
RISCOS_SDK_SOURCE=/path/to/gccsdk295.tar.gz \
python3 scripts/package.py
```

It requires macOS, Windows and Amiga builds, verifies any present optional builds/installers, and writes portable packages, source ZIP/tar.gz, checksums and the PHP website ZIP under `outputs/`. DOS, Atari, RISC OS and Windows Classic packages include their required licenses and corresponding-source/relink material. Local packaging does not upload anything. To deliberately omit the optional RISC OS, Atari and Windows Classic packages, use `python3 scripts/package.py --primary-only`; a present DOS build is still included. Existing setup/DMG artifacts must be current too.

Standalone Atari and Windows Classic packagers are `python3 scripts/package-atari.py` and `python3 scripts/package-windows-legacy.py`. RISC OS and DOS packagers are documented in their platform guides and require matching source archives. Do not distribute an executable without its required notices and source/relink material.

## Website and edited artwork

The website is PHP 8+, needs no database or JavaScript build, and lives in `public-web/`. Automatic GitHub download links require PHP cURL or OpenSSL-enabled HTTPS streams with `allow_url_fopen`; no API token is used. For a local preview:

```sh
php -S 127.0.0.1:8765 -t public-web
```

Validate the packaged links/assets with `python3 scripts/check-release-site.py` and a PHP CLI in PATH (or selected with `PHP`).

Only regenerate assets when editing their sources. `python3 scripts/generate-themes.py` uses the standard library; `python3 scripts/build-icons.py` and `python3 scripts/build-riscos-icon.py` need Pillow. Generated assets are already included for ordinary builds.
