.PHONY: macos amiga windows riscos test icons installers package amiga-release clean
macos:
	./scripts/build-macos.sh
amiga:
	./scripts/build-amiga.sh
windows:
	./scripts/build-windows.sh
riscos:
	./scripts/build-riscos.sh
test:
	python3 -m unittest discover -s tests -v
icons:
	python3 scripts/build-icons.py
installers:
	./scripts/build-windows-installer.sh
	python3 scripts/build-macos-dmg.py
package:
	python3 scripts/package.py
clean:
	rm -rf build

amiga-release:
	./scripts/release-amiga.sh

atari:
	./scripts/build-atari.sh

.PHONY: release-plan release-build release-check release-prepare
release-plan:
	python3 scripts/release.py plan
release-build:
	python3 scripts/release.py build
release-check:
	python3 scripts/release.py check
release-prepare:
	python3 scripts/release.py prepare
