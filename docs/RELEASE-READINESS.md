# BitSkim 0.1 release status

Updated 11 November 2026. Website: https://bitskim.retrocs.org. Public source:
https://github.com/retrocs-org/bitskim.

## Validation

- Host regression tests and memory sanitizers cover filename bounds, transfer-decision lifetime, queue preservation and packaging.
- Mac application startup, preferences and transfer tools are checked. The public app and disk image require Developer ID signing, accepted notarization and validated stapled tickets.
- Windows x64 startup and engine checks pass. The unsigned installer compiles; interactive installation and Windows 10 GUI testing remain pending.
- AmigaOS 68000 builds, application startup, preferences and nearby-computer discovery are checked. Broader hardware coverage remains pending.
- DOS 8086 builds, matching 65,792-byte payloads in both directions, discovery, tagged sends, queues, folder collections and incoming decisions have automated checks. Physical networking hardware and individual DOS-version compatibility remain unverified.
- Atari STinG and MiNTNet builds succeed. STinG application startup and tools navigation are checked; network transfers remain unverified.
- RISC OS ARM2 and StrongARM APCS-26 builds compile and link. Startup, networking and hardware compatibility remain unverified.

## Distribution

The unified release tool builds and packages the current download set, verifies
checksums, includes required source/relink material and prepares a tagged release
manifest. Repository and Aminet publication require the explicit publish action.
The PHP website bundle is prepared for manual upload.

The gallery contains ten application-window captures. Appearance previews are
labelled within the application. Screenshots illustrate the UI and do not certify
untested platforms. See [RELEASING.md](RELEASING.md) for the release procedure and
[VALIDATION.md](VALIDATION.md) for the validation overview.
