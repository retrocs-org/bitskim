# Website changelog

Changes to the BitSkim public website. Application changes are recorded in the
[app changelog](../CHANGELOG.MD). Website entries follow the corresponding app
version; unreleased entries do not imply a website deployment.

## 0.2 — Unreleased

### Features

- Collapsible client compatibility table in Support, showing completed checks and remaining validation limits for each platform.
- Compatibility information generated from the same maintained record as the app README and validation documentation.
- Support guidance for optional file modification-date preservation and mobile folder transfers.

### Enhancements

- Documented build fingerprints, recovery counters and confirmed checkpoint cleanup alongside the app guides.
- Updated version/metadata/recovery guidance without hardcoding the current public release; download cards continue using published GitHub release assets.
- Build/compatibility documentation now distinguishes the Windows 3.x development executable from classic Mac OS 8/9 port preparation; neither is presented as a released download.
- Automatic download matching for Linux desktop and CLI packages when they appear in public GitHub releases.
- Build and support documentation expanded for Linux and the experimental iOS and Android clients.
- Separate website changelog, linked from the website README.

## 0.1 — Initial website

### Features

- Single-page introduction with classic-computer illustrations, animated file transfers and actual application screenshots.
- Client downloads grouped by availability and experimental status, with unreleased clients collapsed.
- Automatic per-client GitHub release downloads, version labels, checksum links and shared source-archive links.
- Build-from-source instructions with platform requirements and manual compilation commands.
- Support section with collapsible frequently asked questions, including broadcast discovery, network troubleshooting and encryption.
- Application release-notes page and shared PHP header, footer and site configuration.

### Enhancements

- Server-side release metadata caching, last-known download information and clearly labelled fallback links when GitHub is unavailable.
- Public website and repository links standardized to bitskim.retrocs.org and retrocs-org/bitskim.

### Bugs

- Download links point to real GitHub release assets instead of local placeholder pages.
- Support aligns with the other page sections, and Build from source appears in the navigation.
