# Future Aminet submission — BitSkim 0.2

No release command uploads by default. Aminet submissions are a separate explicit action, intended only when the first public release is ready.

## Build locally

```sh
./scripts/release-amiga.sh
```

Produces the verified versioned local archive `outputs/BitSkim-0.2.lha`, matching ZIP/checksums, and a draft `outputs/aminet/bitskim.readme.template`. The template deliberately lacks a usable uploader identity and is not named `.readme`. It is not an upload-ready submission.

## Prepare a submission when ready

```sh
./scripts/release-amiga.sh --aminet-prepare --uploader 'you@your-domain.org (Your Name)' --author 'Your author credit'
```

Replace both contact/credit examples with your actual details. This rebuilds the Amiga release, checks its contents and creates:

- `outputs/aminet/bitskim.lha`: stable unversioned Aminet filename, copied from the verified local LHA.
- `outputs/aminet/bitskim.readme`: matching required metadata and description, LF-only, at most 78 columns, Short field at most 40 characters. Version remains 0.2; architecture is m68k-amigaos, category comm/tcp.
- `outputs/aminet/submission.json`: local destination, source fingerprint, identity and file-hash record. It is not uploaded.

Review the readme and make sure the public source/project links and release claims are ready. Uploader is a contact address supplied to Aminet, not a private credential. Do not commit personal submission records to the repository; outputs/ is generated data. Change generation options/source and prepare again if corrections are needed; hand-editing prepared files invalidates their recorded hashes.

```sh
./scripts/release-amiga.sh --aminet-check
```

Checks the prepared pair and current build fingerprint without connecting to Aminet. Missing contact details, changed files or an obsolete build stop the operation.

## Explicit upload later

```sh
./scripts/release-amiga.sh --aminet-upload --confirm-upload
```

This command performs a public submission: anonymous FTP to main.aminet.net, directory /new, using the uploader email as the anonymous password. It sends only bitskim.lha followed by bitskim.readme, both in binary mode so LF bytes stay intact. It never uploads the ZIP, checksum, source archive, manifest or receipt. Upload requires a prepared pair, matching hashes/source build and the explicit confirmation flag. Nothing is uploaded by normal builds or preparation.

A local upload-receipt.json records completed transfers. Success means transfer to the incoming queue, not publication or acceptance by Aminet. A failed transfer may leave partial remote files. The script refuses automatic retries when a receipt exists: inspect the receipt/server state first, then archive the local receipt manually only when a retry or later submission is appropriate.

The fixed version is still 0.2. Aminet recommends a new version for a later updated public upload; the project owner must explicitly authorize any version bump. Recheck the official instructions before future use because submission endpoints/policies may change.

## Requirements checked on 2 November 2026

The official web upload page currently directs users to FTP. Aminet requests an accompanying readme with matching base name/case and mandatory Short, Uploader, Type and Architecture fields. It prefers stable filenames without version numbers; accepted filenames are at most 30 characters. The readme uses LF line endings and lines no longer than 78 columns. Distribution is deliberately omitted: BitSkim retains its MIT license without adding an Aminet-only distribution restriction.

Sources:
- https://aminet.net/upload
- https://wiki.aminet.net/Uploading_instructions
- https://wiki.aminet.net/The_Readme_file

Validation uses a simulated FTP client only. No live upload or incoming-server authentication test was performed while adding this feature.
