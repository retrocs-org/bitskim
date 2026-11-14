# Validation

Version 0.2 is checked through host regression tests, memory sanitizers, target builds and application startup checks. See [RELEASE-READINESS.md](RELEASE-READINESS.md) for the current release status.

- Mac: application startup, preferences, transfer tools and receive-decision lifetime checks.
- Windows: x64 build and startup/engine checks. Silent installer lifecycle checks passed on Windows 11; wizard interaction and Windows 10 GUI coverage remain pending.
- AmigaOS: 68000 build, application startup, preferences and discovery. Broader hardware coverage remains pending.
- DOS: 8086 build; binary payloads in both directions, discovery, tagged sends, queues, folders and incoming decisions. Physical network cards and older DOS versions remain unverified.
- Atari: STinG and MiNTNet builds; STinG application startup and transfer tools navigation. Transfers and hardware coverage remain unverified.
- RISC OS: ARM2 and StrongARM compile/link checks. Application startup and transfers remain unverified.

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
