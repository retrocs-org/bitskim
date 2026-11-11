# BitSkim public website

PHP templates and CSS with local app captures. Serve this directory as the site root at
https://bitskim.retrocs.org. The public repository is
https://github.com/retrocs-org/bitskim. PHP 8.0 or later is required on the web host. No database, external fonts, analytics, cookies,
JavaScript build step or account integration is required.

## Local preview

Run `php -S 127.0.0.1:8765 -t public-web` from the project root. The entry points are `index.php` and `release-notes.php`; shared metadata, header and footer live in `includes/`. The built-in server is for local preview only.

## Manual upload

After rebuilding and packaging, upload the complete contents of `public-web/`,
including `includes/`, `assets/` and `downloads/`, to your PHP-enabled host. Ensure `index.php` is the directory index and remove any old `index.html` or `release-notes.html` from a prior upload. The generated
`outputs/BitSkim-0.1-website.zip` contains that directory for upload. No deployment
or repository publication is performed by the packaging scripts.

Download packages are ignored by Git and must be supplied by the release process.
Run `python3 scripts/package.py` only after current Mac, Windows and Amiga builds,
DOS, both RISC OS variants, and any referenced installers have been verified.
The combined packager includes current DOS packages with their complete
corresponding source and RISC OS relink/source material. It updates download
checksums and the website archive. Run `python3 scripts/check-release-site.py`
after packaging; a missing local asset, stale repository link or missing download
is a release failure, not a placeholder to publish.

The hero computer drawing is an illustration. Gallery images are actual app
captures, with demo peers identified in their captions. DOS text-screen captures
use actual video memory and the running BIOS font through the project UI test
harness. No RISC OS runtime screenshot is claimed. Preserve the screenshot
provenance file alongside new captures.

Keep platform requirements and test status accurate. Native runtime validation,
compile/link validation, code signing and Apple notarization are distinct.
