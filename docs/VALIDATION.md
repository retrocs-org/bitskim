# Validation

Version 0.2 is checked through host regression tests, memory sanitizers, target builds and application startup checks. See [RELEASE-READINESS.md](RELEASE-READINESS.md) for the current release status.

- Mac: application startup, preferences, transfer tools and receive-decision lifetime checks.
- Windows: x64 build and startup/engine checks. Interactive setup and Windows 10 GUI coverage remain pending.
- AmigaOS: 68000 build, application startup, preferences and discovery. Broader hardware coverage remains pending.
- DOS: 8086 build; binary payloads in both directions, discovery, tagged sends, queues, folders and incoming decisions. Physical network cards and older DOS versions remain unverified.
- Atari: STinG and MiNTNet builds; STinG application startup and transfer tools navigation. Transfers and hardware coverage remain unverified.
- RISC OS: ARM2 and StrongARM compile/link checks. Application startup and transfers remain unverified.

Run `python3 -m unittest discover -s tests` for the host suite. Build and packaging checks are described in [RELEASING.md](RELEASING.md). Passing builds do not establish compatibility with untested operating-system or hardware configurations.
