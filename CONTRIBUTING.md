# Contributing to BitSkim

- Keep the release version at 0.1 until a new version is approved.
- Maintain wire compatibility and update protocol documentation alongside protocol changes.
- Keep the shared C engine bounded and nonblocking, without a threading requirement or whole-file buffering.
- Record build checks and runtime checks separately. Do not claim support on untested targets.
- Preserve third-party copyright notices and distribution requirements.
- Keep website source in `public-web/`; deployments and repository publication are explicit release actions.
- Use `scripts/release.py` for coordinated builds, signing, packaging and release preparation.
- Configure destinations, toolchains and credential references in external `~/Development/tools/git-release/projects/bitskim.json`. Do not commit credentials or personal machine paths.
- Refresh the Amiga distribution and full source archives after changes. The configured local development drawer and compiled-package mirror are updated through the release scripts. Preserve unrelated local projects and network settings.

See [docs/BUILDING.md](docs/BUILDING.md), [docs/RELEASING.md](docs/RELEASING.md) and [docs/VALIDATION.md](docs/VALIDATION.md).
