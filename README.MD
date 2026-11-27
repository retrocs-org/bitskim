# BitSkim

**Different generations. Same network.**

Send files and folders directly between modern desktops and classic computers. BitSkim uses native interfaces, discovers nearby computers through local-network broadcasts, and needs no account or cloud service.

[Website and downloads](https://bitskim.retrocs.org) · [Public repository](https://github.com/retrocs-org/bitskim) · [What's new in 0.2](CHANGELOG.MD)

## Clients

<!-- compatibility:start -->
| Client | Target |
| --- | --- |
| [macOS](docs/MACOS.md) | Apple silicon; macOS 13+ |
| [Windows](docs/WINDOWS.md) | Windows 10/11 x64; unsigned |
| [AmigaOS](docs/AMIGA.md) | AmigaOS 3.x; 68000+; TCP/IP stack |
| [Linux](docs/LINUX.md) | GTK desktop and CLI; arm64/x86-64; Ubuntu 24.04 baseline |
| [DOS / mTCP](docs/DOS.md) | Experimental; 8086; packet driver |
| [Atari TOS / MiNTNet](docs/ATARI.md) | Experimental STinG and MiNTNet builds |
| [RISC OS](docs/RISCOS.md) | Experimental ARM2 and StrongARM builds |
| [Windows Classic](docs/WINDOWS-LEGACY.md) | Experimental x86; Windows 95/98/Me/XP targets |
| [iPhone / iPad](docs/IOS.md) | Experimental source-only; iOS 16+ |
| [Android](docs/ANDROID.md) | Experimental source-only; 64-bit Android 8+ target |
| [Windows 3.x](docs/WINDOWS16.md) | Development-only; Win16; installed Winsock 1.1 |
| [Classic Mac OS 8/9](docs/MAC-CLASSIC.md) | Port preparation; PowerPC/Open Transport initially |
<!-- compatibility:end -->

Compatibility summaries are generated from [one maintained record](docs/compatibility.json). See [validation coverage](docs/VALIDATION.md) for completed checks and remaining limits.

## Start sharing

1. Install and open BitSkim on both computers on a trusted local network.
2. Choose a nearby computer, or connect using its IPv4 address.
3. Select files or folders to send, then accept the offer and choose a destination on the receiving computer.

Transfers include progress, cancellation, queues, conflict choices and speed limits. Individual files can be up to 2 GiB minus one byte; file modification-date and retro metadata preservation are optional. The iOS and Android development clients can select provider folders and review their contents before sending; Linux desktop results include optional notifications with Open folder. Available features depend on the client.

Modern Mac, Windows and Linux clients prefer TLS encryption. Retro transfers use plaintext. Compare verification codes before remembering a modern device's identity. Keep BitSkim's listening port off the public Internet. BitSkim uses its own protocol and does not interoperate with AirDrop.

## Documentation

- [Install, update and uninstall](docs/INSTALLATION.md)
- [Using the apps and preferences](docs/USAGE.md)
- [Transfers, encryption and resume](docs/TRANSFERS.md)
- [Build from source](docs/BUILDING.md)
- [Build, package and publish releases](docs/RELEASING.md)
- [Platform validation and limitations](docs/VALIDATION.md)
- [Protocol specification](docs/PROTOCOL.md)

The PHP website source is in [public-web](public-web/). Project release automation is included in [scripts/release.py](scripts/release.py); its settings live outside the checkout in `~/Development/tools/git-release/projects/bitskim.json`.

## License

BitSkim's own code is MIT licensed. Dependencies and packaged components retain their own licenses; see [LICENSE.md](LICENSE.md) and [third-party notices](THIRD-PARTY-NOTICES.MD).
