# Linux desktop and CLI

BitSkim 0.2 includes a GTK desktop application and a separate command-line client for Linux arm64 and x86-64. Both use the same transfer engine as the other clients. No account, server or repository credentials are needed.

## Use a compiled archive

Extract the archive for your architecture and enter its top-level directory. Run `./bin/bitskim` for the desktop or `./bin/bitskim-cli --help` for the terminal. Install that archive with `./install.sh "$HOME/.local"`, or `sudo ./install.sh /usr/local` for all users. The CLI-only archive provides `bitskim-cli` without the desktop. These commands use the included binaries; the build commands below apply to the separate full source archive.

## Build

Use Linux with a C11 compiler, Python 3.9+, pkg-config, GLib/GIO 2.66+ and OpenSSL 3+. The desktop also requires GTK 4.12+ and glib-compile-resources. Ubuntu 24.04 is the current build baseline; older distributions may need newer libraries or a local rebuild. System libraries are linked dynamically and are not bundled.

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install build-essential python3 pkg-config libglib2.0-dev libssl-dev libgtk-4-dev
./scripts/build-linux.sh
```

Outputs are `build/linux/arm64/bitskim` and `bitskim-cli`, or the corresponding `x86_64` directory. Architecture is detected on the Linux host. The CLI needs no GTK/display:

```sh
sudo apt-get install build-essential python3 pkg-config libglib2.0-dev libssl-dev
./scripts/build-linux.sh --cli-only
```

For cross-compilation on Linux, provide the target GCC toolchain and target GLib, OpenSSL and GTK development files, then pass `--arch arm64` or `--arch x86_64`. Use `--cc` or `CC` for a different compiler. `PKG_CONFIG_LIBDIR` selects target metadata; optional `BITSKIM_LINUX_PKG_CONFIG_PATH` adds isolated target package metadata. Do not mix host and target headers/libraries.

From macOS, an existing running Lima Linux instance can build an isolated source snapshot:

```sh
python3 scripts/build-linux-lima.py --instance YOUR_LINUX_INSTANCE --arch arm64 --test
```

Install the same Linux dependencies in that instance first. `BITSKIM_LIMA_HOME` defaults to `~/Development/tools/linux/lima`; downloaded tools and instance disks stay outside the checkout. The helper copies source without Git history, local release settings or credentials, runs the build/tests, verifies returned artifacts, then removes its temporary source copy. It does not create, reset or start an instance. x86-64 builds on arm64 additionally require a cross-toolchain, target dependencies and qemu-x86_64 for the process tests.

## Run and install

Run `build/linux/arm64/bitskim` (or `build/linux/x86_64/bitskim`) on a desktop. Choose files or a folder, select a discovered client/address or enter an IPv4 address, then click **Send selected items**. Dragging files selects them for review. Incoming offers show the sender, transport, collection entries and receive folder; accept or decline explicitly. Existing destinations are renamed safely. Closing an approval declines it.

Settings include the receive folder, name, visibility, receiving, encryption, speed limits, optional retro metadata and eight shared palettes. Mocha is the default. Transfer tools contains diagnostics, pairing, recent results and retry. Reports refresh on request, so scrolling stays stable. Connection QR creates a five-minute invitation for a selected local address; the scanning client checks the TLS identity and receiver token. Invitations still require receive approval.

Install a local build:

```sh
python3 scripts/install-linux.py --prefix "$HOME/.local"
# Or a system installation:
sudo python3 scripts/install-linux.py --prefix /usr/local
```

Use `--cli-only` to omit the desktop; `--arch` selects a built architecture. Packaging systems can add `--destdir /absolute/staging/root`. Add the prefix's `bin` directory to PATH. The desktop entry and icon install under its `share` directory. A desktop session may need to refresh its application list.

To uninstall, remove `bin/bitskim`, `bin/bitskim-cli`, `share/applications/org.bitskim.BitSkim.desktop`, `share/icons/hicolor/512x512/apps/org.bitskim.BitSkim.png` and `share/doc/bitskim` from that installation prefix. User data is kept separately.

## Transfer notifications

The desktop sends one grouped result notification after a collection finishes or its queue pauses on failure, cancellation or decline. Counts report the actual completed and unsuccessful items. Written-file verification is claimed only when confirmed by the engine. **Open folder** opens the receiving directory or outgoing source folder in the system file manager; clicking the notification opens Transfer tools. These actions work while that BitSkim process is running.

**Settings → Notify when transfers finish or fail** turns alerts on or off and persists per profile; new profiles enable them. The desktop notification service and its Do Not Disturb settings control display. The CLI does not send desktop notifications. Closing BitSkim withdraws its current result notification. Full local paths are kept out of notification text, although a single-file result can show its filename.

## Command line

Examples assume `bitskim-cli` is in PATH. Use the other device's reachable LAN address; 127.0.0.1 is only for processes on the same machine.

```sh
bitskim-cli discover --timeout 5 --json
bitskim-cli send 192.168.1.20 report.pdf Pictures/
bitskim-cli receive --directory "$HOME/Downloads"
```

The interactive receiver accepts `y`, declines `n`, and supports `trust XXXX-XXXX-XXXX` after comparing the code on both screens. Outgoing interactive transfers also display their code and accept `trust CODE`. Remembering an identity does not accept an incoming file automatically.

For an explicitly unattended receiver:

```sh
bitskim-cli receive --directory /absolute/receive-folder --accept --once --timeout 120 --json
```

`--accept` requires an explicit existing directory. Without an interactive terminal, receiving refuses to start unless that option is supplied. `--once` means one file or one complete collection. Accepting when destination space cannot be queried additionally requires `--allow-unknown-space`; known-insufficient space is refused. Binding to a trusted LAN interface and firewalling the port is recommended for unattended receiving.

Additional commands/options:

```sh
bitskim-cli queue --json
bitskim-cli continue
bitskim-cli clear-queue
bitskim-cli history --json
bitskim-cli clear-history
bitskim-cli diagnostics --json
bitskim-cli send --peer DEVICE_ID document.txt
bitskim-cli receive --directory "$HOME/Downloads" --invite-address 192.168.1.10
bitskim-cli send --invitation 'bitskim://INVITATION_FROM_RECEIVER' document.txt
```

Copy the complete actual invitation URI from the receiver; the placeholder above is not a usable invitation. Discovered IDs can have several endpoints; use an explicit address when the core reports ambiguity. The desktop groups those endpoints under one device and lets you select the address.

`--security off|prefer|require` controls transport. Prefer encryption is the default; it allows plaintext for compatible legacy peers. Use Require to refuse plaintext. Compare codes before remembering a new identity; saved identity mismatches stop the connection. `--metadata` enables the shared opt-in retro metadata/adjacent-icon behavior. See [TRANSFERS.md](TRANSFERS.md) for its scope and limits.

`--timeout` bounds the command lifetime (zero means no limit). Exit codes are 0 for completion, 1 for transfer/state failure, 2 for invalid arguments/setup, 124 for timeout and 130 for interruption. A failed/timed-out outgoing queue is saved paused. `continue` explicitly retries it; Ctrl-C cancels the active transfer and discards its remaining collection as described by the shared engine. JSON command results go to stdout; approval, pairing and startup messages go to stderr. Do not log invitation URIs or private settings in public build logs.

## Storage and simultaneous clients

The desktop and CLI share a default profile:

| Data | Default location |
| --- | --- |
| Settings and installation ID | `$XDG_CONFIG_HOME/bitskim/settings.ini`, fallback `~/.config/bitskim` |
| Paused queue and eight recent results | `$XDG_STATE_HOME/bitskim`, fallback `~/.local/state/bitskim` |
| TLS private identity and remembered peers | `$XDG_DATA_HOME/bitskim`, fallback `~/.local/share/bitskim` |
| Incoming files | Configured Downloads directory; choose an existing folder before accepting |

Only absolute XDG environment paths are used. BitSkim directories are private (0700), settings/identity/state files are private (0600), and a profile lock prevents two processes writing the same state. Restored work stays paused until **Continue**; receive approval is never restored automatically. Keep the private identity safe when moving a profile. Deleting it changes the device identity and breaks existing trust.

To run desktop and CLI together, give the second client its own profile and TCP port:

```sh
bitskim-cli --profile "$HOME/.local/share/bitskim-terminal" --port 46442 receive --directory "$HOME/Downloads"
```

`--profile` must be absolute and creates separate config/state/data subdirectories. Discovery-enabled instances use UDP 46440 and advertise the selected TCP port. Only one process on a machine can own that discovery listener; use a manual address for additional same-host profiles. `--no-discovery` uses manual-only startup with no initial probe or well-known UDP listener and disables broadcasts/search; manual connections remain available. Firewall TCP 46440 (or the selected port) and UDP 46440 on the trusted local network. IPv6, DNS hostnames, automatic startup and a background service are not implemented.

## Packages and checks

After all source/documentation edits are complete, build and create checked archives:

```sh
./scripts/build-linux.sh
python3 scripts/package-linux.py --arch arm64
python3 scripts/package-linux.py --arch arm64 --cli-only
```

Select `x86_64` on that build. Outputs are `BitSkim-0.2-Linux-ARCH.tar.gz` (desktop + CLI) and `BitSkim-0.2-Linux-CLI-ARCH.tar.gz` (CLI only), with SHA-256 sidecars. They include an `install.sh`, documentation, notices and build provenance, without source copies or private state. They run against the distribution's libraries. A full project source archive is distributed separately. `BITSKIM_RELEASE_OUTPUT=outputs/linux` selects an isolated directory beneath the checkout’s ignored `outputs/`. The combined packager includes present, current Linux builds, and a full 0.2 release requires all four checked Linux archives with matching checksums.

Run the actual process tests after compiling:

```sh
sudo apt-get install xvfb xauth xdotool dbus-x11 python3-gi gir1.2-atspi-2.0
python3 -m unittest discover -s tests -p test_linux_clients.py -v
```

The tests use separate temporary profiles, loopback ports and actual binaries, including TLS, byte comparisons, empty files/folders, collisions, saved retries, metadata companions, invitation checks, permissions and GTK approval controls. Test tools are optional for normal use. See [VALIDATION.md](VALIDATION.md) for completed checks and remaining desktop/distribution coverage.
