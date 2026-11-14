# Validation

Version 0.2 is checked through host regression tests, memory sanitizers, target builds and application startup checks. See [RELEASE-READINESS.md](RELEASE-READINESS.md) for the current release status.

- Mac: application startup, preferences, transfer tools and receive-decision lifetime checks.
- Windows: x64 build and startup/engine checks. Interactive setup and Windows 10 GUI coverage remain pending.
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

The visual review found the startup window extending beyond the desktop at high DPI and unscaled/clipped Transfer tools controls. Window placement now fits the monitor work area; Transfer tools uses DPI-scaled controls with report and action rows anchored to the actual client size. `test_windows_window_fit.py` exercises the production placement/layout functions across DPI values and monitor work areas. Windows 10 and setup-installer interaction remain unverified.

A native GUI transfer from the installed Mac 0.1 app to Windows 0.2 also completed successfully. Windows reported all 60 test bytes received, `written file verified (CRC32)`, and a matching history entry with no error. The high-DPI tools window was visually rechecked after the layout fix: labels and footer controls fit the desktop. The extra disk-space confirmation seen during that run is removed for known-sufficient space; unknown/insufficient outcomes have dedicated regression coverage.


The native Windows GUI initialization check also creates an actual multiline edit control, scrolls into its diagnostic text, updates it twice, and checks that the first visible line and text selection remain unchanged. It verifies report text/background brush colours for all eight application palettes using native GDI. `test_windows_tools_refresh.py` additionally covers unchanged updates and clamping when a report becomes shorter. The check is part of `BitSkim-CoreCheck.exe` through the sibling GUI executable’s `--check-startup` mode.
