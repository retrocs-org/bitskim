# Releasing BitSkim

[Website](https://bitskim.retrocs.org) · [Public source](https://github.com/retrocs-org/bitskim)

Use Python 3.9+ from the project root. `scripts/release.py` builds once, prepares a manifest and retained upload copies, then publishes those checked bytes. Review notes, destinations, packages and native validation before publication. Manual compilation is documented separately in [BUILDING.md](BUILDING.md).

## Release rules

- BitSkim remains **0.2** until a version change is agreed. VERSION, application metadata, changelog, notes and package names must agree. A tag override does not change application versions.
- New releases use the unprefixed version tag, `0.2`, and the same release title. Pass `--tag 0.2` explicitly; the default is also `0.2`. Do not publish a new 0.2 build under an older release tag.
- Both repositories must receive the same annotated tag object and source commit. Published tags are never moved, force-pushed or recreated. Fetch an existing tag locally before resuming from a different checkout.
- Repository releases are **normal/latest releases**, not prereleases. GitHub uploads through an unpublished draft, then explicitly sets `draft: false`, `prerelease: false` and `make_latest: "true"`. GitLab uses a current release date and checks its latest-release endpoint. Both hosts' latest endpoints must identify the selected tag.
- Platform validation labels remain factual. A normal release can contain a documented experimental client; it does not certify that client's hardware compatibility. The iOS and Android development clients remain source-only and are excluded from the binary inventory.
- Do not change reviewed assets or notes during publication. Matching existing bytes can be reused; unknown/different hashes, sizes, descriptions, asset inventories or tag objects stop publication.
- Publication does not create repositories, push source branches, deploy the PHP website, overwrite conflicting release assets or roll back another service. Source pushing is a separate explicit action. No hosted CI configuration is required.

## Configure outside Git

Default settings file:

```text
~/Development/tools/git-release/projects/bitskim.json
```

Override with `BITSKIM_RELEASE_CONFIG` or `--config /path/to/settings.json`. Keep real internal destinations, machine paths, signing/profile names and credential selectors outside the public checkout. There is deliberately no real release-settings JSON or template file in the source archive. A minimal configuration shape is:

```json
{
  "github": "https://github.com/retrocs-org/bitskim",
  "gitlab": "https://gitlab.example.invalid/group/bitskim",
  "source_branch": "main",
  "notary_profile": "bitskim",
  "aminet": false,
  "uploader": "",
  "author": "BitSkim contributors",
  "env": {
    "BITSKIM_SIGN_IDENTITY": "Developer ID Application: Your Name (TEAMID)"
  },
  "credentials": {
    "github": {"provider": "env", "variable": "GITHUB_TOKEN"},
    "gitlab": {
      "provider": "1password",
      "reference": "op://YourVault/YourItem/YourField"
    }
  }
}
```

Replace the example internal destination locally. Optional `github_git`/`gitlab_git` select the matching HTTPS or ordinary SSH Git destination; each must identify the same repository as its HTTPS API URL. Empty `source_branch` uses the current branch. Set `aminet: true` and a real public uploader email to generate the separate Aminet submission during preparation. Author/uploader text must be ASCII.

Allowed `env` keys are:

| Purpose | Settings |
| --- | --- |
| Python, PHP and Mac signing | `BITSKIM_RELEASE_PYTHON`, `PHP`, `BITSKIM_SIGN_IDENTITY` |
| Amiga and LHA writer | `AMIGA_CC`, `LHA`, `LHA_READER`, `AMIGA_DEV_BIN` |
| Modern/classic Windows and installer | `WINDOWS_CC`, `LEGACY_WINDOWS_CC`, `LEGACY_WINDOWS_RC`, `MAKENSIS`, `NSISDIR` |
| DOS | `WATCOM`, `DOS_RUNNER` |
| Atari | `ATARI_CC`, `STING_INCLUDE` |
| RISC OS | `RISCOS_CC`, `RISCOS_SDK_SOURCE` |
| Optional Linux builds | `BITSKIM_LINUX_INSTANCE`, `BITSKIM_LIMA_HOME`, `BITSKIM_LINUX_ARCHES`, `BITSKIM_LINUX_PKG_CONFIG_PATH` |
| Optional local distribution mirror | `BITSKIM_BUILDS_DIR` |

Keep installed toolchains under the shared tools directory, not a disposable checkout. See the platform build guides for provisioning. LHA creation needs a compression-capable LHa-for-UNIX writer; Lhasa only extracts. Mac app and DMG publication require Developer ID signing, accepted notarization and stapled tickets. The JSON contains certificate/profile names, not private keys or passwords.

## Credentials

Environment tokens or 1Password are supported. A named-field selector can replace the copied reference:

```json
"credentials": {
  "github": {
    "provider": "1password",
    "vault": "YourVault",
    "item": "Your GitHub item",
    "field": "your-group/bitskim-rw",
    "account": ""
  }
}
```

Choose a copied `reference` **or** `vault`/`item`/`field`, never both. An optional `account` selects the 1Password account. Named fields can contain slashes. The `op` CLI must be signed in or connected to its desktop app; missing, ambiguous or cancelled lookups stop publication without displaying secret output.

Tokens are resolved only for selected publication services and stay in memory. Source pushes and builds do not fetch API tokens. Known credential environment variables are removed from build children; known values and common token formats are redacted from build logs. Git pushes use ordinary SSH/credential-helper authentication separately. Apple signing/notarization credentials remain in the keychain.

GitHub credentials need Contents write access to the exact repository. Helper-generated GitHub App installation tokens can be stored in the same 1Password field: the tool checks repository ID/name against the installation repository list rather than relying on user push-role flags. Installation tokens normally expire after one hour; refresh through the helper before publishing. The helper and release config must use the same vault/item/field. GitLab tokens need `api` scope and suitable project access, normally Developer or higher; protected-tag rules can impose additional requirements.

## Push committed source only

```sh
python3 scripts/release.py source --to gitlab,github --dry-run
python3 scripts/release.py source --to gitlab,github --confirm-push
```

All selected branch pushes are checked before any real push. Only committed source/history is sent, without force, mirror or follow-tags. This does not compile or upload a release. `.gitignore` controls initial staging, but does not remove already tracked files or secrets in older commits.

For a newly extracted source folder without Git history:

```sh
python3 scripts/release.py source --init --to gitlab,github --dry-run
```

`--init` verifies every selected remote is accessible and has no refs, checks Git identity and `.gitignore`, then initializes/stages/commits locally. Its default commit message is `Push changes to repository`; `--commit-message` changes only this first commit. A dry run with `--init` can therefore change local Git metadata, but never uploads. Inspect the commit before `--confirm-push`. Existing committed repositories get no extra initial commit, and existing history is never replaced.

The tool rejects source-input symbolic links, common credential filenames and token/private-key formats before building, preparing, checking and source publication. This is limited detection of current release inputs, not comprehensive secret or history scanning. Review tracked files and history separately before making a private repository public. Matching values are not printed.

## Release notes and changelog

`CHANGELOG.MD` is the historical change list. Its current-version section uses `### Features`, `### Enhancements` and `### Bugs`, with the complete changes for that version. Keep earlier versions there.

`docs/RELEASE-NOTES.md` is the Markdown description sent unchanged to both hosts. It must contain:

```markdown
# BitSkim 0.2

Short application description.

## Features

Overview of the whole application.

## Installation

Platform downloads, requirements, setup and manual location.

## Source and Licence

Public source, exact tagged release, licence and dependency notices.

## Version Changelog

### 0.2

#### Features

- Complete new functionality from this version's changelog.

#### Enhancements

- Complete improvements from this version's changelog.

#### Bugs

- Complete corrections from this version's changelog.
```

The three current-version groups must exactly match `CHANGELOG.MD`; preparation rejects missing changes, wrong headings and unfinished placeholders. Do not copy earlier releases into the current release description. The first public release lists initial capabilities rather than pre-release fixes. Use versioned repository asset names, factual validation limits and public URLs; omit internal paths, credential details and development commentary. Aminet metadata headers belong only in the plain-text readme.

Finish notes, changelog, documentation and website-source edits **before building**. They are fingerprinted release inputs. A later documentation edit makes builds stale and requires rebuilding. Review [VALIDATION.md](VALIDATION.md) and [RELEASE-READINESS.md](RELEASE-READINESS.md); a successful compiler check is not native runtime certification.

## Build, check, prepare and publish

After committing the reviewed release inputs and checking source destinations, use one fresh artifact directory consistently:

```sh
python3 scripts/release.py plan
python3 scripts/release.py build --output outputs/release-0.2-new --verbose
python3 scripts/release.py check --output outputs/release-0.2-new
python3 scripts/release.py prepare --output outputs/release-0.2-new \
  --tag 0.2 --notes docs/RELEASE-NOTES.md
```

Review the manifest, notes, all packages/checksums and retained copies under `outputs/release-0.2-new/release/`, then explicitly publish:

```sh
python3 scripts/release.py publish --output outputs/release-0.2-new \
  --tag 0.2 --notes docs/RELEASE-NOTES.md \
  --to gitlab,github --confirm-publish
```

Publication uses the notes already bound into the prepared manifest; `--notes` selects input for preparation, not an override of prepared publication notes. `release` combines preparation and publication of an already verified build; it does not compile:

```sh
python3 scripts/release.py release --output outputs/release-0.2-new \
  --tag 0.2 --to gitlab,github --confirm-publish
```

These commands are for a new tag, never a way to replace conflicting published assets. Use `--tag-message "BitSkim 0.2 release"` during preparation to choose the annotation; publication uses that prepared annotation. A supplied publication annotation must match. Commit messages, tag names and tag annotations are separate values.

`plan` prints steps without building or uploading. `build` requires a clean committed checkout, runs buffered host tests, compiles the desktop/classic clients, signs/notarizes the Mac artifacts, packages and validates downloads, builds/tests both required Linux architectures, and optionally refreshes configured local distribution mirrors. It does not compile/publish the mobile development clients. Dependencies and Apple notarization use the network; no release API tokens are fetched.

Builds print stages, elapsed times and package sizes. Detailed redacted output is saved in `<output>/release/build.log`; `--verbose` also streams it. Failed build stages retain `failed-build.log`. Build fingerprints and copies of provenance records remain local, outside the public release asset list.

Without `--output`, the artifact directory is `outputs/`, with state in `outputs/release/`. Custom destinations must be inside `outputs/`, cannot traverse symbolic links and cannot use the reserved `outputs/release/` directory. A custom build directory must be fresh. A build refuses to overwrite a directory containing publication receipts. Compilation still uses shared `build/` intermediates: use separate checkouts for concurrent versions or independently retained native build trees.

A full release requires `BITSKIM_LINUX_INSTANCE` to name an existing configured Lima build instance. Both `arm64` and `x86_64` are required; `BITSKIM_LINUX_ARCHES` defaults to `arm64,x86_64` and cannot select a partial release. Build individual Linux targets using the standalone commands in [LINUX.md](LINUX.md). The inventory checks all intended packages, even when no Linux archive exists, and names missing or unexpected files.

## Repository asset inventory

For 0.2, upload these seventeen packages and one matching `<filename>.sha256` each—**34 uploaded assets** on each host:

```text
BitSkim-0.2-macOS-arm64.zip
BitSkim-0.2-macOS-arm64.dmg
BitSkim-0.2-Windows10-x64.zip
BitSkim-0.2-Windows10-x64-Setup.exe
BitSkim-0.2-AmigaOS.zip
BitSkim-0.2.lha
BitSkim-0.2-DOS-mTCP.zip
BitSkim-0.2-RISCOS3-ARM2.zip
BitSkim-0.2-RISCOS3-StrongARM.zip
BitSkim-0.2-Windows-Classic-x86.zip
BitSkim-0.2-Atari-TOS-STinG-68000.zip
BitSkim-0.2-Atari-MiNTNet-68000.zip
BitSkim-0.2-Atari-MiNTNet-68060.zip
BitSkim-0.2-Linux-arm64.tar.gz
BitSkim-0.2-Linux-CLI-arm64.tar.gz
BitSkim-0.2-Linux-x86_64.tar.gz
BitSkim-0.2-Linux-CLI-x86_64.tar.gz
```

Checksums contain the SHA-256 digest, two spaces, exact filename and newline. Upload identical package and checksum bytes to both hosts. Required dependency source/relink materials remain inside their respective binary packages.

The hosts supply tagged source ZIP/tar downloads. Local source ZIP/tar, website ZIP and aggregate SHA256SUMS are built/validated but not attached to repository releases. Neither mobile development APKs/apps, logs, settings, manifests nor receipts are public release assets. Deploy the PHP website separately from `public-web/`.

Preparation retains exact upload copies at `<output>/release/publish/outputs/`. Hashes are checked before each destination and each upload. GitHub checks digest and size, reuses only matching assets and publishes its verified draft last. An incomplete existing public release is refused. GitLab uses versioned generic packages, downloads/hash-checks their bytes and links them from the normal release. Unexpected public assets/links and differing notes are refused. API writes reject redirects; credentials are never forwarded to another download origin. `gh`/`glab` are not required.

## Aminet submission

Aminet uses exactly these stable, case-sensitive names, without a version:

```text
bitskim.lha
bitskim.readme
```

The LHA is an unchanged copy of the verified versioned Amiga LHA; renaming must not rebuild it. The readme's Version field identifies the release. Do not upload other clients, the website, source ZIP or checksums as the Aminet application archive.

The generator writes ASCII/LF text, at most 76 columns, with these header fields in order: `Short`, `Author`, `Uploader`, `Type`, `Version`, `Architecture`, `Requires`, `Distribution`, `URL`. BitSkim uses category `comm/tcp` and architecture `m68k-amigaos >= 3.0.0`. The body contains the description, FEATURES, INSTALLATION, SOURCE AND LICENCE, and the full matching VERSION CHANGELOG grouped as Features/Enhancements/Bugs. Markdown syntax and decorative Unicode are removed. The uploader email is public.

With `aminet: true` and uploader configured, ordinary release preparation creates `<output>/aminet/`. Review both files before selecting Aminet explicitly:

```sh
python3 scripts/release.py publish --output outputs/release-0.2-new \
  --tag 0.2 --to aminet --confirm-publish
```

Use `--to gitlab,github,aminet` for all three services. The standalone Amiga-only route remains available:

```sh
./scripts/release-amiga.sh
python3 scripts/aminet_release.py prepare \
  --uploader "you@example.org" --author "Your ASCII Name"
python3 scripts/aminet_release.py check
python3 scripts/aminet_release.py upload --confirm-upload
```

The standalone route uses `outputs/aminet/` by default and does not publish GitHub/GitLab releases. A stale Amiga fingerprint requires rebuilding, not editing recorded hashes. `scripts/bootstrap-lha.sh` prepares a writer in shared tools when needed.

### Submit an already published release

If the selected output directory retains a completed GitHub/GitLab publication receipt and its checked upload copies, use the separate path:

```sh
python3 scripts/release.py prepare-aminet --output outputs/release-0.2-new --tag 0.2
python3 scripts/release.py publish --output outputs/release-0.2-new \
  --tag 0.2 --to aminet --confirm-publish
```

This creates `<output>/release/aminet-existing/` from the retained published LHA and `CHANGELOG.MD` at its recorded source commit. Working-tree edits do not rebuild or change that archive. It requires the matching repository-publication receipt and staged hashes; it does not download arbitrary remote assets or accept an unverified local replacement. The same route can submit a retained 0.1 publication by selecting its output directory and `--tag 0.1`.

Repository manifests, packages, tags and receipts stay unchanged. The separate Aminet submission has its own manifest/receipt. Preserve any existing preparation/receipt before considering another submission; automatic retry is refused.

### Incoming protection and moderation

Aminet uses anonymous, unencrypted FTP to `main.aminet.net:21`, `/new`. The incoming listing must be readable and neither stable filename may already exist. The tool checks before publication and again immediately before transfer; it will not knowingly overwrite an unknown pending submission. FTP listing and upload are not an atomic server transaction, so a concurrent uploader can still race this check.

The archive is sent first, readme last. Acknowledgement means **awaiting moderator review**, not public publication. Partial/uncertain uploads preserve their receipt and block automatic retry. Review the incoming server with Aminet administrators before recovery. Stable-name submissions do not automatically remove any older versioned Aminet entries.

## Receipts, retries and corrections

`<output>/release/receipt.json` binds the manifest hash, annotated tag object, completed destinations and current destination. Resume with the same output/tag/manifest after inspecting a failure. Completed services are retained; another service's failure does not roll them back. Network/API failures are not automatically retried. Repository identity, tag conflicts, existing bytes/notes and selected Aminet state are checked before remote mutations; permissions can still change after preflight.

Aminet uses `<output>/aminet/upload-receipt.json`, or its separate `release/aminet-existing/upload-receipt.json`. The receipt is written before the first FTP transfer. `failed-may-be-partial` requires inspection; deleting receipts or changing fingerprints is not recovery. Acknowledged submissions have status `transferred-awaiting-aminet-review`.

Prefer a new patch release for corrected published packages. A reviewed notes-only correction can be made manually without changing tag/package bytes; the normal tool refuses differing prepared notes. Explicitly authorized replacement is a separate recovery operation: retain originals/hashes, verify replacement bytes on every service and preserve a replacement record. Assess whether Aminet needs another submission when its bytes change. Do not bypass conflicts by deleting assets, receipts or tags.

## Testing and practical limits

```sh
python3 -m unittest discover -s tests -p 'test_release*.py' -b
python3 -m unittest discover -s tests -p 'test_aminet_release.py' -b
python3 -m unittest discover -s tests -b
```

Tests use temporary Git repositories, local artifacts and mocked APIs/FTP. They never publish. Exercise hosted adapters against dedicated test repositories before a production tag. The release tool's checks establish matching source/artifact bytes, not reproducible toolchains or universal native compatibility. Multi-service publication is not atomic; changes between preflight and upload can stop a later destination.

API references: [GitHub releases](https://docs.github.com/en/rest/releases/releases), [GitHub release assets](https://docs.github.com/en/rest/releases/assets), [GitLab releases](https://docs.gitlab.com/api/releases/), [GitLab generic packages](https://docs.gitlab.com/user/packages/generic_packages/).

## Required Linux release assets

Set `BITSKIM_LINUX_INSTANCE` in the external build environment to an existing running Lima Linux instance with the dependencies in [LINUX.md](LINUX.md). The release build compiles/tests `arm64,x86_64` before packaging; `BITSKIM_LINUX_ARCHES` must contain both architectures once; its default is `arm64,x86_64`. Cross builds require target libraries/toolchains and a test runner already provisioned. `BITSKIM_LIMA_HOME` selects the shared tools location; `BITSKIM_LINUX_PKG_CONFIG_PATH` selects isolated target metadata if needed. A full release stops before building if no instance is configured.

The standalone Linux builder/install/packager also works directly on Linux without release credentials. The combined packager includes present current Linux binaries as desktop+CLI and CLI-only tar.gz archives. All four Linux archives are required alongside the desktop/retro inventory, with matching source provenance and per-file SHA-256 sidecars on both release hosts. Stale Linux binaries/packages fail checks rather than being silently published. The website exposes each Linux download only after a normal public GitHub release contains that asset; it never fabricates a 0.1 Linux fallback.


## Native installer acceptance

After building, prepare isolated Windows fixtures using the configured NSIS tool:

```sh
python3 scripts/prepare-windows-installer-review.py --makensis /path/to/makensis
```

In Windows, run `build/windows-installer-review/Run-wizard-review.cmd` and approve the administrator prompt. It stages its script/setup files locally before elevation. Only installers identified as `BitSkimInstallerTest` are accepted. It navigates Welcome, License, Components, Directory and Finish pages; checks cancellation, 0.1 upgrade, repeated/fresh 0.2 installation and uninstall; runs the installed native CoreCheck; and verifies preservation of unknown files, settings and received data. It never approves UAC automatically. A failed or unfamiliar page is left for inspection; the report is copied back as `wizard-review.log`. The silent-only runner remains available separately. Windows 10 needs its own native run.

The packaged Mac executable supports an isolated native startup/view check:

```sh
/path/to/BitSkim.app/Contents/MacOS/BitSkim --check-startup --review-output /absolute/review/output
```

It uses a temporary preference domain and private profile, opens the actual native views, renders the main/preferences/tools screens in all eight themes, checks the listener/TLS identity and removes only its own profile. It performs no file transfer. Run it from the app copied out of the final DMG. The image builder uses native APFS creation and checks the mounted and copied bundle signatures, preventing HFS Finder metadata from invalidating an otherwise signed app. The release pipeline checks the notarized package automatically; run the complete check locally with:

```sh
python3 scripts/check-macos-dmg.py --require-notarization --native-views \
  --review-output /absolute/review/output
```

This mounts only its own read-only image, copies the app into a temporary installation directory and checks version, strict signatures, tickets and Gatekeeper. This is separate from a human drag-to-Applications review.

For Amiga, keep the existing bridged Workbench session and its network settings. Use the unpacked `Builds:BitSkim/Install-BitSkim` package, review the displayed application/Prefs/shared destinations and settings preservation, and check the installed 0.2 app/preferences and discovery. Do not substitute a non-networked profile or silently change the running profile to obtain a pass. Keep native reports/screenshots locally; compiler and installer-flow tests do not establish a native GUI/network result.
