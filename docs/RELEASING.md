# Releasing BitSkim

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

Run `python3 scripts/release.py` from the BitSkim project root. A separate universal release-tools checkout is not required. Python 3.9+ and the platform toolchains are required; see [BUILDING.md](BUILDING.md).

## External settings

The default configuration is:

```text
~/Development/tools/git-release/projects/bitskim.json
```

Keep the release settings JSON at this external location; it is deliberately not shipped in the repository. Configure repository destinations, toolchain paths and credential lookup details there. Override with `--config /path/to/settings.json`, or set `BITSKIM_RELEASE_CONFIG`. Actual settings are not included in Git/source archives. Documentation snippets below use generic placeholders.

The JSON holds repository HTTPS and SSH URLs, compiler paths, the signing identity name, notarization keychain profile name, optional Aminet contact and 1Password references. Repository URLs are not authentication secrets, but internal destinations and machine settings stay outside the public checkout.

## Credentials from 1Password

A credential can use an exact copied reference:

```json
"credentials": {
  "gitlab": {
    "provider": "1password",
    "reference": "op://YourVault/YourItem/YourField",
    "account": ""
  },
  "github": {
    "provider": "1password",
    "vault": "YourVault",
    "item": "Your GitHub item",
    "field": "Your release token field",
    "account": ""
  }
}
```

Choose either a copied `reference` or `vault`/`item`/`field`. An exact reference is preferred when labels are ambiguous. Optional `account` selects the intended 1Password account. Field names containing slashes work with the named-field form. The installed 1Password CLI must be signed in or connected to the desktop app.

The token is retrieved only for selected GitHub/GitLab destinations during publication. Values are captured in memory, never printed or saved to configuration/manifests. Compilation does not fetch API tokens. Cancellation or ambiguous/missing fields stop publication.

Environment variables remain an alternative:

```json
"credentials": {
  "gitlab": {"provider": "env", "variable": "GITLAB_TOKEN"},
  "github": {"provider": "env", "variable": "GITHUB_TOKEN"}
}
```

Git source/tag pushes use ordinary SSH credentials or Git credential helpers; API tokens do not configure SSH. Mac signing/notarization credentials remain in the keychain. The config contains their names, not private keys/passwords.

## 1. Push committed source only

```sh
python3 scripts/release.py source --to gitlab,github --dry-run
python3 scripts/release.py source --to gitlab,github --confirm-push
```

This pushes the current committed branch and its history. It does not compile, create release tags or upload packages. All selected pushes are dry-run first; force/mirror/follow-tags are not used. Optional `source_branch` changes the destination branch name; empty means the current branch. An extracted archive needs importing into a Git checkout and committing first. Existing Git history must be preserved. The root check handles macOS path capitalization correctly.

For an extracted source folder without `.git`, explicitly create a first local commit:

```sh
python3 scripts/release.py source --init --to gitlab,github --dry-run
```

`--init` first checks every selected remote for any branches/tags/refs. A nonempty or inaccessible remote stops the operation before initialization. It requires a reviewed `.gitignore` and configured Git author identity, initializes a main branch, stages nonignored files and creates the first local commit. The default initial commit message is `Push changes to repository`; override it with `--commit-message`. Existing committed repositories get no additional initial commit. The dry run can therefore change local Git metadata, but never uploads. After review, use the ordinary source command with `--confirm-push`. It never force-pushes or replaces existing history.

Only committed files are pushed. Already tracked files are not excluded by later .gitignore rules. This script does not provide comprehensive secret scanning of source/history/packages.

## 2. Compile and package

```sh
python3 scripts/release.py plan
python3 scripts/release.py build
python3 scripts/release.py check
```

The build runs host tests, compiles macOS, Windows, AmigaOS, DOS, RISC OS, Atari and Windows Classic clients; signs/notarizes Mac artifacts; packages the source/website/client downloads; and validates them. Configured local distribution mirrors are refreshed. No repositories or Aminet submissions are published by these commands. Successful test output is buffered so simulated uploads and mocked build plans do not appear in normal build logs. Failures still show their diagnostics. Dependency downloads and Apple notarization use the network.

Finish source/doc/website changes before compiling: provenance records cover release inputs, and packaging rejects stale builds. Runtime and hardware validation remain separate from compilation.

## 3. Prepare and publish a release

```sh
python3 scripts/release.py prepare --tag 0.1
# Review outputs/release/manifest.json and its prepared asset copies.
python3 scripts/release.py publish --tag 0.1 --to gitlab,github --confirm-publish
```

Or prepare and publish the current verified build in one explicit operation:

```sh
python3 scripts/release.py release --tag 0.1 --to gitlab,github --confirm-publish
```

`--tag 0.1` means the exact tag/title `0.1`; omitting it defaults to `v0.1`. The application version remains 0.1. Changing a tag does not change embedded versions or package filenames. Existing tags at another commit are never moved.

Publication requires a clean, committed Git checkout and current builds. Preparation from an extracted archive is allowed, but publication is blocked without a commit. Source/commit, current settings destinations, archive contents, notarization and hashes are checked again before upload.

## Standard release assets

GitHub and GitLab receive the same platform packages, each with a matching `<filename>.sha256`. The checksum contains SHA-256, two spaces, filename and newline. Release title equals the exact tag.

Source-code downloads are supplied by the hosts from the Git tag. Standalone source archives, the website bundle and aggregate SHA256SUMS are still built/validated locally, but are excluded from the default publication list. Required corresponding-source/relink material inside binary packages remains included.

Exact copies for upload are staged in `outputs/release/publish/outputs`, bound to the prepared manifest. GitHub uses a draft until expected asset digests match. GitLab stores versioned generic packages, downloads/hash-checks them and links them from the release. No gh/glab installation is needed.

## Aminet

Set a real `uploader` contact in the external config before preparing an Aminet submission. Review `outputs/aminet/bitskim.readme` and the LHA, then:

```sh
python3 scripts/release.py publish --tag 0.1 --to aminet --confirm-publish
```

Use `--to gitlab,github,aminet` to select all three. Aminet uses its separate verified adapter and anonymous FTP incoming submission. Successful transfer means awaiting moderator review, not public publication.

## Failure and retry

`outputs/release/receipt.json` binds completed destinations to the manifest. Reuse an unchanged manifest after an interrupted GitHub/GitLab upload; matching existing assets are reused and conflicting hashes/notes stop publication. Never force-move tags or delete receipts to hide an earlier attempt. A partial Aminet attempt requires inspecting its own receipt/incoming server before retrying. There is no automatic network retry or rollback of another successful destination.

Only one tag/manifest is prepared at a time in this project. If an existing publication receipt belongs to a different tag/manifest, stop and preserve/review that release state before beginning another release. The script does not silently overwrite it.

Publication does not create repositories, push source branches automatically or deploy the website. The explicit source action is separate.

## Commit message, tag name and tag annotation

These are three different values. A GitHub commit link displaying "Import BitSkim 0.1" shows the source commit message, not the release tag.

For the first commit in a new repository:

```sh
python3 scripts/release.py source --init --commit-message "BitSkim 0.1 source release" --to gitlab,github --dry-run
```

For a new annotated release tag:

```sh
python3 scripts/release.py release --tag 0.1 --tag-message "BitSkim 0.1 release" --to gitlab,github --confirm-publish
```

The tag name and release title are `0.1`; the tag annotation is `BitSkim 0.1 release`. Neither changes the existing source commit message. With separate prepare/publish, supply the annotation during prepare; publish uses that prepared value. Existing commits and tags are never amended or force-pushed. An existing Git repository gets no extra initial commit from --init. To change an already published commit message would require rewriting history, which this tool does not do.

Build tools should live under a persistent tools directory rather than a temporary source checkout. Configure `MAKENSIS` and `NSISDIR` for the Windows installer compiler; `BITSKIM_RELEASE_PYTHON`, `PHP`, `RISCOS_CC`, `RISCOS_SDK_SOURCE`, `DOS_RUNNER`, `ATARI_CC` and `STING_INCLUDE` likewise accept external locations. Keep these machine-specific paths in the external JSON.
