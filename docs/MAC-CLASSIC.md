# Classic Mac OS 8/9 port preparation

This port is not yet a usable transfer client. The initial target is PowerPC Mac OS 8/9 with Open Transport. The shared Retro68 PowerPC compiler was built successfully using its open-source Multiversal Interfaces. Those interfaces do not include MacTCP/Open Transport, so networking requires a separately supplied classic Mac SDK. No proprietary Apple SDK is bundled or downloaded automatically.

Install ordinary host dependencies through Homebrew, then build the pinned compiler outside the repository:

```sh
brew install boost cmake gmp mpfr libmpc bison texinfo flex
python3 scripts/bootstrap-classic-mac.py
python3 scripts/check-classic-mac.py --interfaces /path/to/Universal/Interfaces/CIncludes
```

Custom tools live under `~/Development/tools/classic-mac`; `--directory` changes that root. The bootstrap pins Retro68 revision `a4a389c41e90b1095a40ccf13f28678b6cfbba86` and builds PowerPC, excluding Carbon and 68K. It refuses to overwrite an existing checkout at a different revision. SDK checks compile declarations only and do not establish ABI/runtime compatibility.

Remaining implementation includes nonblocking Open Transport TCP/UDP adapters, cooperative event-loop integration, safe HFS path mapping, exclusive creation and non-replacing commits, catalog/alias checks, native file dialogs, and resource-fork handling. Retro68's default newlib file wrapper ignores exclusive-create flags, so it must not be used unchanged for receiving files. The first implementation must preserve existing destinations and explain data-fork/resource-fork behavior explicitly. TLS is not planned for the initial classic target.

Mac OS 8.1 on 68K requires a separate 68K build and acceptance pass. No classic Mac binary or release asset is currently advertised. See the [upstream Retro68 interfaces documentation](https://github.com/autc04/Retro68#interfaces-and-libraries).
