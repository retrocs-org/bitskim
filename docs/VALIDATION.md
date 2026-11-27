# Validation

Version 0.2 is checked through host regression tests, memory sanitizers, target builds and application startup checks. See [RELEASE-READINESS.md](RELEASE-READINESS.md) for the current release status.

<!-- compatibility:start -->
| Client / target | Checks completed for 0.2 | Still unverified |
| --- | --- | --- |
| [macOS](MACOS.md) — Apple silicon; macOS 13+ | Native startup/UI, all themes, Mac/Windows encrypted transfers and mounted/copied notarized app checks. | Human installation review and broader physical-network coverage. |
| [Windows](WINDOWS.md) — Windows 10/11 x64; unsigned | Windows 11 native engine, high-DPI UI, install/update/uninstall and setup-wizard checks. | Windows 10 GUI/runtime review. |
| [AmigaOS](AMIGA.md) — AmigaOS 3.x; 68000+; TCP/IP stack | Owner-confirmed native installation, app/Preferences startup and bridged discovery; host LHa/OLHa extraction. | Exact native OLHa/LHa archive check pending; broader hardware and TCP/IP stacks. |
| [Linux](LINUX.md) — GTK desktop and CLI; arm64/x86-64; Ubuntu 24.04 baseline | arm64 native process/GTK tests; x86-64 process tests through emulation; TLS, folders and result notifications. | Physical x86-64, real Wayland sessions and other distributions. |
| [DOS / mTCP](DOS.md) — Experimental; 8086; packet driver | DOS runtime transfers in both directions, discovery, queues, folders and receive decisions. | Physical network cards and older DOS versions. |
| [Atari TOS / MiNTNet](ATARI.md) — Experimental STinG and MiNTNet builds | Target builds; STinG app startup and Transfer tools navigation. | Native transfers and physical hardware. |
| [RISC OS](RISCOS.md) — Experimental ARM2 and StrongARM builds | Compile/link and package checks for both CPU variants. | Native startup, transfers and physical hardware. |
| [Windows Classic](WINDOWS-LEGACY.md) — Experimental x86; Windows 95/98/Me/XP targets | PE32/subsystem/import compatibility checks. | Native runtime and hardware. |
| [iPhone / iPad](IOS.md) — Experimental source-only; iOS 16+ | Local runtime TLS transfers, file/folder review, file management, themes and accessibility layout checks. | Physical devices, authenticated cloud providers and VoiceOver; local Files runtime issue remains documented. |
| [Android](ANDROID.md) — Experimental source-only; 64-bit Android 8+ target | Android 17 local runtime/JNI TLS transfers, folder staging, minimized-transfer cancellation and accessibility layout checks. | Physical devices, Wi-Fi discovery, older Android versions, authenticated providers and TalkBack. |
| [Windows 3.x](WINDOWS16.md) — Development-only; Win16; installed Winsock 1.1 | 8086 large-model build; NE format and Win16 import checks. | Native startup, Winsock stacks, transfers, cancellation and keyboard review. |
| [Classic Mac OS 8/9](MAC-CLASSIC.md) — Port preparation; PowerPC/Open Transport initially | Shared pinned Retro68 PowerPC compiler build. | Networking SDK, safe HFS/Open Transport adapters and all native app/transfer checks; no client binary. |

These are observed 0.2 checks, with limitations retained explicitly. The new file-date extension has host socket coverage; native date preservation still needs review on each target. Release candidates require their own artifact acceptance checks.
<!-- compatibility:end -->

Run `python3 -m unittest discover -s tests` for the host suite. Build and packaging checks are described in [RELEASING.md](RELEASING.md). Passing builds do not establish compatibility with untested operating-system or hardware configurations.


## 0.2 cross-release and failure checks

`python3 -m unittest discover -s tests -p test_release_interoperability.py -v` compiles the actual tagged `0.1` core and current core separately and connects them over loopback sockets. It checks both transfer directions with plaintext, automatic TLS and paired TLS, empty files, 32 MiB binary files, nested folders/empty directories and cancellation from either endpoint. It requires a Git checkout retaining tag `0.1` and OpenSSL development files; an extracted source tree without that history reports a skip. Set `BITSKIM_BASELINE_REF` for another explicitly chosen baseline.

`test_transfer_tools.py` injects ENOSPC-style short writes and final-close errors on the receiver's temporary file. Existing destinations must remain unchanged, partial files must be removed and neither endpoint may report successful delivery. Existing readback-corruption checks exercise CRC verification failures. This is deterministic fault injection, not a claim that every filesystem or full target machine has been tested.

The Mac and Windows Transfer tools present nearby compatibility details directly, including unknown profiles, rather than requiring hover tooltips. These are discovery hints and do not replace live negotiation. Native and target-system checks remain distinct from the host engine tests above.


## Native Windows 0.2 review — 14 November 2026

The Windows 11 run of `BitSkim-CoreCheck.exe` passed all 12 checks with zero failures: GUI image initialization, native socket handles, exclusive creation, device-name rejection, Winsock/Unicode binary transfer, automatic TLS, reverse empty-file transfer, collision-safe finalization, preserved destination size, a 24-file collection, native free-space queries, and nested Unicode/empty-directory transfer. This is a native Windows runtime result, distinct from host-only interoperability tests.

The visual review found the startup window extending beyond the desktop at high DPI and unscaled/clipped Transfer tools controls. Window placement now fits the monitor work area; Transfer tools uses DPI-scaled controls with report and action rows anchored to the actual client size. `test_windows_window_fit.py` exercises the production placement/layout functions across DPI values and monitor work areas. Windows 10 and interactive setup-wizard navigation remain unverified.

A native GUI transfer from the installed Mac 0.1 app to Windows 0.2 also completed successfully. Windows reported all 60 test bytes received, `written file verified (CRC32)`, and a matching history entry with no error. The high-DPI tools window was visually rechecked after the layout fix: labels and footer controls fit the desktop. The extra disk-space confirmation seen during that run is removed for known-sufficient space; unknown/insufficient outcomes have dedicated regression coverage.


The native Windows GUI initialization check also creates an actual multiline edit control, scrolls into its diagnostic text, updates it twice, and checks that the first visible line and text selection remain unchanged. It verifies report text/background brush colours for all eight application palettes using native GDI. `test_windows_tools_refresh.py` additionally covers unchanged updates and clamping when a report becomes shorter. The check is part of `BitSkim-CoreCheck.exe` through the sibling GUI executable’s `--check-startup` mode.


## Native transfer, trust and installer follow-up — 14 November 2026

Windows 0.2 sent a 38-byte file to the installed Mac 0.1 application with exact byte comparison and receiver readback verification. A second transfer used automatic TLS with matching verification codes; its received bytes also matched. The Mac remembered the Windows identity after restart. This check exposed Windows configuring persistent identity storage only when preferences were saved: startup now configures it before security initialization and fails closed if storage cannot be opened. Existing pins are retained.

The actual tagged 0.1 and current cores additionally pass `test_cross_release_trust.py`: mutual code verification and saved pins, fresh-process restart in both directions, plaintext downgrade rejection, and changed-identity rejection. A subsequent native check restarted both Mac 0.2 and Windows 0.2, established a fresh encrypted connection, and displayed `Saved device identity matches` on the Mac and `trusted identity matches` on Windows for the same connection code. Neither side needed re-pairing. This confirms mutual native trust persistence separately from the engine tests. The inspection offer was then cancelled; it is not counted as another completed file transfer.

The updated Mac 0.2 received a Windows 0.2 folder containing three files and three directories, including an empty file, an empty directory and a Unicode filename. All file hashes matched and every file reported CRC32 readback verification. A separate encrypted 1 GiB transfer was cancelled after 1,855,488 received bytes: both clients recorded cancellation, the Mac reported incomplete integrity, and the destination was absent. Declining an offer before receiving bytes was checked separately. Finder-generated metadata is excluded from source/destination comparisons.

Mac 0.2 attaches incoming approvals to the main window after dismissing Transfer tools or manual-connection sheets, rather than choosing an arbitrary visible/key window. `test_macos_approval_presentation.py` exercises dismissal scheduling and expired-offer guards without opening UI; `test_macos_offer_lifetime.py` retains the real-engine stale-offer checks.

The isolated native Windows installer lifecycle passed: actual 0.1 payload install, upgrade to 0.2, refusal while old/current application mutexes are held, repeated installation, uninstall, fresh 0.2 install and uninstall, shortcut removal, and preservation of unknown files, settings and received data. The live application's settings hashes were unchanged. This is silent installer testing under the distinct `BitSkimInstallerTest` product, not navigation of the production setup wizard. Prepare it with `python3 scripts/prepare-windows-installer-review.py --makensis /path/to/makensis`; run `build/windows-installer-review/Run-installer-review.cmd` on Windows. Administrator approval is required for the isolated all-users test. It stages files locally before elevation and copies the report back.


## Experimental iPhone/iPad 0.2

The source-only SwiftUI app and embedded Share extension compile for arm64 device and local iOS test SDKs. Seven iOS runtime unit tests passed: selected advertised endpoint with identity checking, automatic TLS and Unicode binary readback, an empty file, cancellation after data begins, nested folders/empty directories, collision-safe shared-inbox staging including symbolic-link rejection, and oversized-file/folder rejection with no partial inbox commit. The host suite includes executable manual IPv4/parser and no-broadcast transport checks. These results do not establish physical-device UI, Local Network permission, discovery entitlement, Files provider, App Group or Share-extension runtime behavior. See [IOS.md](IOS.md) for manual builds, signing and remaining device checks. No iOS release asset is published.


The native iOS discovery review identified duplicate SwiftUI row identities when one installation advertised several interfaces. The list now groups one computer with explicit address choices. `bs_send_peer_address` and `bs_queue_peer_address` check the selected catalog endpoint and preserve the expected HELLO identity. Host regression tests cover successful selected-address queueing, changed identity rejection and stale/unadvertised endpoint rejection. The grouped row/address menu was checked in Device Hub. The launcher also separates an iOS boot data-migration message from successful app installation/launch; failed boot commands stop before installation and no automatic erase/reset is attempted.


Files-provider integration adds an explicit copy-mode export picker for received files and folders and setup guidance for enabled Files providers. Eight iOS runtime unit tests pass, including export cancellation and original-file preservation. The native Save to Files destination screen opened for a known empty test file; cancelling returned to Received with the original still listed. Both arm64 device and local iOS builds passed bundle checks; the host suite passed 480 tests with five skips. Authenticated iCloud Drive, Dropbox and OneDrive transfers on physical devices remain unverified.


The iOS Settings update uses the committed shared palette catalog, with a separate theme page and Diagnostics navigation. The Files tab stages provider selections in the shared local inbox before recipient selection; the former Computers tab is named Clients. Ten iOS runtime tests pass, including native import initial-directory/cancellation checks and an actual idle IOSClient subscription that must not republish empty offers. The native picker was observed opening Received with the known test file visible. The host suite passed 480 tests with five skips; palette checks verify all eight iOS catalogs/colors against the shared source and test text contrast. A user screenshot confirmed themed Settings surfaces; interactive review of the latest theme-page responsiveness remains pending because automated input stopped reaching Device Hub.

The Files-tab follow-up launches the standard Apple document picker when the tab is selected, without specifying an initial directory. The picker still supports multiple selections and cancellation; all ten iOS runtime tests pass. The local 0.2 app was rebuilt and launched.


The standard importer now requests regular data files in copy mode. Ten iOS runtime tests pass, including import mode, multiple selection, cancellation and preservation of originals. The reported app-folder loop remains reproducible after a Files-service refresh and device restart, and on a separate clean iOS 27.2 test device. It also reproduces directly in Apple’s standalone Files app, outside BitSkim’s picker. DocumentManager logs show failed cached-domain lookup and `Cannot find source for location` for the correct Documents container; the actual Received folder and its original test file remain intact. Both devices report a boot data-migration failure. This supports a development-runtime limitation, but physical-device validation is still required; no folder-navigation fix is claimed.


## iOS send review, progress and received-file management

Nineteen iOS runtime tests pass, including a real socket transfer that stays idle until explicit review confirmation, review cancellation, rejection of files whose size changes before confirmation, and a rate-limited two-file transfer with monotonic overall progress and exact verified destination bytes. File-management tests cover search, multiple files/folders, rename collision and invalid-name rejection, scoped deletion, symbolic-link escape rejection and canonical directory aliases. ETA tests cover unknown/zero speed, rounding and hour boundaries. The host suite passes 480 tests with five skips. Both unsigned arm64 device and local iOS builds pass bundle validation.

Native UI checks confirmed the recipient/file-size review and cancellation, received text-file preview, visible search, two-file selection with reachable Share/Save to Files/Delete actions, and the rename prompt. Only isolated test fixtures were used, and unchanged fixtures were removed afterward. No native share or cloud upload was submitted, and permanent deletion was exercised only by unit tests in their temporary folders. Physical-device/iPad and authenticated provider checks remain pending; the previously documented local Apple Files folder-navigation loop is unchanged. Version remains 0.2.

## Experimental Android 0.2

The arm64 development APK builds against API 37 with minSdk 26, NDK r28c and pinned OpenSSL 3.5.9. Package checks passed for version 0.2, signature, permission declaration, resources, ABI and 16 KiB native/ZIP alignment. On a local arm64 Android 17 runtime, debug instrumentation passed an encrypted TLS transfer through the JNI bridge, Unicode filename handling, exact file readback, written-file CRC32 verification and received-file rename. Host Java checks exercise manual IPv4/port validation, UTF-8 filename bounds, collision preservation and traversal/symbolic-link rejection.

The running Android UI also accepted a 41-byte Mac-to-Android TLS transfer through a manual forwarded address; the sender reported written-file verification and private received bytes matched. The reverse UI workflow also passed: Android Files selected the local Downloads document, private staging completed, outgoing review required confirmation, and the Mac received identical bytes over TLS with written-file verification. Mocha Clients, Files, review and incoming-approval screens were inspected. Physical-device permission/network behavior, Wi-Fi discovery, Android versions below 17, real document providers and complete share/import/export flows remain pending. This is source-only development work; no Android public release asset is published. See [ANDROID.md](ANDROID.md).

## Linux desktop and CLI 0.2

The Linux clients use the production shared engine, GLib/GIO and system OpenSSL 3. Ubuntu 24.04 arm64 is the native build/test baseline. x86-64 is cross-compiled on that baseline and its client-process tests run through qemu-x86_64; this does not establish physical x86-64 desktop/driver compatibility.

`test_linux_clients.py` connects independent processes with temporary private profiles. It exercises TLS and CRC32 readback with exact destination bytes, Unicode/zero-byte files, nested and empty folders, preserved collision destinations, paused saved queues and explicit retry, history/clear, private file modes, exclusive profiles, explicit unattended approval, symlink-source rejection, identity-checked invitations and content-bound metadata/.info companions. A UDP listener verifies manual-only startup sends no discovery probe. Run architecture suites sequentially when they share a machine’s UDP discovery port. GTK tests run the actual window/settings/tools/QR controls and receive approval under a virtual X display. These checks are separate from real Wayland sessions, desktop file-provider integration, multiple physical computers, packaging on other distributions and older glibc/GTK versions, which remain pending.

## Mobile folders and Linux result notifications — 0.2

The 24-test iOS runtime suite passes, including a reviewed TLS folder send, Unicode payload, nested/empty folders, unchanged original bytes, change detection, queue-owned staging and native folder-picker configuration. Android debug instrumentation stages a real document tree through a read-only debug DocumentsProvider, transfers 96 files through JNI/TLS, compares the complete destination tree, and checks bounded status and safe staging cleanup. Host Java tests cover denied/truncated reads, cycles, traversal, oversize hints, cancellation, same-size edits and symlinks. The existing Android minimized-transfer/notification Cancel check also passes. Authenticated iCloud/Dropbox/OneDrive and physical mobile devices remain pending; the known local Apple Files navigation issue is unchanged.

Linux's native GTK review uses an isolated D-Bus notification service and an actual native folder handler to check grouped file/folder results and Open folder. The failure path is exercised through the window controls, and a restart with notifications disabled verifies saved opt-out and prevents old-result replay. This verifies the application's notification requests/actions; real desktop notification styles and Do Not Disturb behavior vary by session.


## Release polish and accessibility — 18 November 2026

The isolated iOS UI harness renders the production folder review with synthetic rows at the largest Dynamic Type size in Mocha and Latte. Its native UI tests verify full long-path labels, wrapping, reachable Send/Cancel after scrolling, disabled Send while checking and cancellation, retaining screenshots. Android instrumentation checks the production review dialog at normal and 200% text in light/dark themes, complete path wrapping, visible 48dp actions, scrolling and keyboard cancellation. Neither layout harness starts a network listener or sends files. Physical VoiceOver/TalkBack and mobile provider/device coverage remain separate.

The Mac packaged application has an isolated native startup/view check that uses a temporary profile and preference domain, tests native listener/TLS initialization and renders the real main/preferences/tools views in all eight themes. The Windows isolated acceptance runner now has a wizard mode for actual Welcome, License, Components, Directory and Finish navigation, cancellation, upgrade, installed CoreCheck and uninstall. Latest package-specific reports and captures are retained locally by these runners; preparation of a runner is not a native pass. Windows 10 and a human Mac installation review still require the relevant native environment.


The 18 November Windows 11 wizard run passed actual Welcome/License/Components/Directory/Finish navigation, cancelled setup with no registration, 0.1 installation and 0.2 upgrade, refusal while a client mutex is held, repeated/fresh installation, installed native CoreCheck with zero failures, uninstall and shortcut cleanup. Unknown files, received data and all live settings hashes were preserved. The runner also safely recovered its interrupted private test installation. Windows 10 remains unverified.

Mounted/copy testing found the HFS hybrid image added nonzero FinderInfo inside the signed Mac bundle. Native APFS image creation preserves both mounted and copied signatures; a real disk-image regression test verifies the valid copy and rejects forbidden metadata. The release pipeline now validates the mounted/copied app and requires stapled tickets and Gatekeeper assessment after notarization. Candidate-specific hashes/results stay in the local package-review report.


## Amiga installer confirmation — 19 November 2026

The project owner confirmed installation/update from the refreshed package in the usual bridged Workbench session. BitSkim and BitSkimPrefs opened as version 0.2, and nearby discovery worked. This completes the pending native Amiga installation/startup/discovery review. The existing session and network configuration were preserved. This is a user-reported result; it does not add new transfer, filesystem, physical-hardware or TCP/IP-stack coverage.


## LHA path compatibility — 22 November 2026

The existing 30-file BitSkim package reproduced the reported level-0 filename mismatch: Unix LHa extracted its nested tree, while a macOS build of OLHa 0.4.4 stopped after the outer drawer icon. On a temporary copy, changing only filename-field `0xFF` separators to `/` and recalculating the affected header sums let both readers test and extract all 30 files with identical content hashes. Compressed data, file CRCs and DOS timestamps were retained. No released archive was changed.

New Amiga packaging performs that normalization before its existing integrity, full-payload and date checks. `test_lha_compatibility.py` covers nested and empty files, binary payload bytes containing `0xFF`, idempotence, and rejection of malformed headers, checksums, unsupported levels, truncated streams and mismatched/duplicate member inventories without rewriting the input. Set `OLHA=/path/to/a/host/olha` to enable the independent-reader integration test; it reports a skip when OLHa is unavailable. The host OLHa source commit used here was `bb13fd2c0659a5ec4ce18932ffbf8c27f387dc4d` from `https://codeberg.org/MastaTabs/olha`.

The exact native Amiga OLHa 0.4.4 binary test remains pending. These are macOS host results, not a native Amiga compatibility pass or an assignment of fault exclusively to either archiver. The running Amiga sessions and their networking were not changed.

## Connection, key-storage and folder recovery checks

`test_security_deadlines.py` exercises partial-header and partial-manifest activity over real sockets with a controlled clock. `test_collection_sessions.py` checks actual TLS reuse, old-peer fallback, pause/expiry and recipient changes. `test_folder_recovery.py` verifies matching completed files, a mismatched existing file, interrupted partial data after sender restart, unpaired rejection and destination links. Host Java staging checks include a provider-supplied original modification date.

`python3 scripts/fuzz-protocol.py --seconds 30` runs production name, greeting, capability, invitation and frame/manifest decoders under libFuzzer, AddressSanitizer and UBSan. It never accepts destination writes. Install Homebrew LLVM on macOS if Apple's clang lacks libFuzzer; use `CC` for another compatible compiler. This bounded parser run does not establish exhaustive protocol/security coverage.

`python3 scripts/check-key-stores.py macos` checks round-trip/update/delete against a disposable Keychain account; this passed on the build host. The Windows variant compiles an isolated DPAPI migration/update/tamper/delete executable; it must be run under the test Windows user. No production key is inspected. A new Windows 10 runtime, exact native Amiga LHa/OLHa test and physical mobile/provider-date checks are still pending. No emulator profile or bridge configuration was changed.

`python3 scripts/benchmark-core.py --tls --mib 1 --batch-files 64` compares verified 1 KiB TLS files with the same 20 ms host loopback service cadence. The measured development run used 64 connections/36.7 seconds without reuse and one connection/8.1 seconds with reuse, retaining exact payload checks. These are host engine measurements, not native UI, physical-LAN or low-end-hardware throughput claims.

## Build identity, recovery progress and retained-file review

`test_build_identity.py` checks that identical source contents produce identical IDs in different directories, generated outputs do not feed back into the hash, source edits change the ID, and the compiled engine returns the generated value. The ordinary platform build scripts inject the same source fingerprint into their C engine.

`test_release_polish.py` exercises actual cleanup against private temporary files: owned checkpoint pairs, completed lookalikes without a local marker, final-file preservation, changed content, locked files, linked/replaced directories, obsolete confirmation tokens, busy transfers and bounded scans. It also checks all-existing and mixed verified/resumed/new folder jobs. Cleanup never runs automatically or without confirmation. Windows uses handle-based file operations; POSIX targets use checked directory descriptors and exclusive quarantine renames. Native Windows cleanup acceptance is still required.

The iOS runtime suite includes a real interrupted checkpoint followed by confirmed cleanup while retaining an existing completed destination. Android JNI runtime checks exercise locally marked checkpoint cleanup alongside existing TLS and file-provider tests. The native Mac startup renderer includes the actual retained-files view in every shared theme. These local runtime checks remain separate from physical-device, cloud-provider and native Windows acceptance.
