# BitSkim

**Different generations. Same network.**

Send files and folders directly between modern desktops and classic computers. BitSkim uses native interfaces, discovers nearby computers through local-network broadcasts, and needs no account or cloud service.

[Website and downloads](https://bitskim.retrocs.org) · [Public repository](https://github.com/retrocs-org/bitskim) · [What's new in 0.2](CHANGELOG.MD)

## Clients

| Client | Version 0.2 |
| --- | --- |
| macOS | Apple silicon, macOS 13+; public downloads signed and notarized |
| Windows | Windows 10/11 x64 target; unsigned; Windows 10 GUI validation pending |
| AmigaOS | AmigaOS 3.x, 68000-compatible, Workbench and TCP/IP stack |
| DOS / mTCP | Experimental; packet driver required |
| Atari TOS / MiNTNet | Experimental STinG and MiNTNet builds; transfers unverified |
| RISC OS | Experimental ARM2 and StrongARM builds; runtime validation pending |
| Windows Classic | Experimental x86 build for Windows 95/98/Me/XP targets; runtime validation pending |
| iPhone / iPad | [Experimental source-only client](docs/IOS.md); iOS 16+ target; device validation pending |

See [validation coverage](docs/VALIDATION.md) for the checks completed on each platform.

## Start sharing

1. Install and open BitSkim on both computers on a trusted local network.
2. Choose a nearby computer, or connect using its IPv4 address.
3. Select files or folders to send, then accept the offer and choose a destination on the receiving computer.

Transfers include progress, cancellation, queues, conflict choices and speed limits. Individual files can be up to 2 GiB minus one byte; filesystem metadata is not preserved. Available features depend on the client.

Modern Mac and Windows clients prefer TLS encryption. Retro transfers use plaintext. Compare verification codes before remembering a modern device's identity. Keep BitSkim's listening port off the public Internet. BitSkim uses its own protocol and does not interoperate with AirDrop.

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
