# BitSkim 0.2 development status

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

Version 0.2 is in development and remains fixed until explicitly changed by the project owner. Published 0.1 downloads and screenshots remain historical 0.1 artifacts. Do not relabel those binaries as 0.2.

Version 0.2 adds saved-file verification, discovery/compatibility diagnostics, saved queues/history, optional retro metadata, Linux desktop/CLI clients, and source-only mobile clients with folder review. Rebuild and validate every client and package against the current source before releasing. Experimental clients retain their runtime-validation limitations; see [VALIDATION.md](VALIDATION.md).


A complete repository release contains 17 compiled packages plus 17 individual checksum files on each host. Linux desktop/CLI archives for arm64 and x86_64 are mandatory. Mobile development apps, local source archives and website archives remain outside that public binary inventory.

Before publication, review the exact rebuilt Mac, Windows and Amiga packages. Windows setup-wizard navigation and Windows 10 runtime coverage remain separate from silent installer tests. Preserve the currently running Amiga session and its network configuration; a non-networked or unrelated profile does not establish the requested bridged-network check. Keep pending native checks visible in [VALIDATION.md](VALIDATION.md).
