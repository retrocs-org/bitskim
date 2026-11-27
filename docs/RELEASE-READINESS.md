# BitSkim 0.2 release validation

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

Version 0.2 remains fixed until explicitly changed by the project owner. This document records the validation and package requirements for its normal release. Published 0.1 downloads and screenshots remain historical 0.1 artifacts. Do not relabel those binaries as 0.2.

Version 0.2 adds saved-file verification, discovery/compatibility diagnostics, saved queues/history, optional dates/retro metadata, Linux desktop/CLI clients, source-only mobile clients with folder review, protected desktop pairing keys, bounded negotiation, explicit folder recovery and reviewed checkpoint cleanup. Rebuild and validate every client and package against the current source before releasing. Experimental clients retain their runtime-validation limitations; see [VALIDATION.md](VALIDATION.md).


A complete repository release contains 17 compiled packages plus 17 individual checksum files on each host. Linux desktop/CLI archives for arm64 and x86_64 are mandatory. Mobile development apps, local source archives and website archives remain outside that public binary inventory.

Before publication, review the exact rebuilt Mac, Windows and Amiga packages. Windows setup-wizard navigation and Windows 10 runtime coverage remain separate from silent installer tests. Preserve the currently running Amiga session and its network configuration; a non-networked or unrelated profile does not establish the requested bridged-network check. Keep pending native checks visible in [VALIDATION.md](VALIDATION.md).


## Completed native acceptance

Windows 11 installer wizard/runtime checks and the corrected Mac DMG copy/signature/notarization/Gatekeeper checks passed. On 19 November 2026, the project owner also confirmed the refreshed Amiga installer, BitSkim/BitSkimPrefs 0.2 startup and discovery in the usual bridged Workbench session. The native Amiga check is now complete within that scope; see [VALIDATION.md](VALIDATION.md).

These results describe the earlier reviewed packages; rebuilt candidates with the latest changes still need their own native acceptance. They do not certify untested platforms or hardware. Windows 10, physical mobile/provider coverage and the other documented validation limits remain. Updating these validation notes changes fingerprinted release inputs, so perform the required fresh full release build from the final committed source before publication. Do not relabel existing build fingerprints.
