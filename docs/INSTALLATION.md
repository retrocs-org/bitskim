# Installing BitSkim 0.2

Repository: https://github.com/retrocs-org/bitskim
Website: https://bitskim.retrocs.org

## AmigaOS 3.x

Unpack the entire Amiga ZIP, open its BitSkim drawer and double-click **Install-BitSkim**. Its project icon starts the supplied **StartInstaller** launcher, which prefers SYS:System/Installer and falls back to C:Installer. The system Installer V42 or later is required and is not redistributed. Shell alternative: `Installer Install-BitSkim APPNAME BitSkim`, from the unpacked drawer.

Close BitSkim and BitSkimPrefs first. The launcher now allows and defaults to the system Installer's **Novice** level. Novice installation uses the standard locations below without destination questions, automatically enables Workbench boot startup and launches the installed app immediately afterwards. There are no separate boot-start or launch-now choices. Select Average (Intermediate) to choose only the main app and Preferences directories, or Expert to also choose the shared-data parent.

Custom installation has these three selections:

| Selection | Default | Installed files |
| --- | --- | --- |
| Main app drawer | `SYS:Utilities` | `BitSkim` and `BitSkim.info` directly in that drawer |
| Preferences drawer | `SYS:Prefs` | `BitSkimPrefs` and `BitSkimPrefs.info` directly in that drawer |
| Shared parent drawer | `SYS:` | A `BitSkim/` subdrawer containing shared resources, documentation and ARexx examples |

The shared drawer has no `.info`, so it does not appear in Workbench's normal icon-only view. It remains accessible through Shell and Show All Files. `Plugins/`, `Tools/`, `Themes/` and `Images/` reserve future components; the client currently has no plugin/skin loader and its graphics remain embedded. Updates remove the shared drawer's previous `.info`; unknown shared contents and old Config backups are preserved.

Other locations are fixed:

- `HELP:BitSkim.guide`, with a project icon opening `SYS:Utilities/MultiView`. `HELP:` must be available; no assign or startup-script changes are made.
- `ENVARC:BitSkim/BitSkim.prefs`, permanent settings with a visible drawer and project icon opening the selected BitSkimPrefs executable.
- `ENV:BitSkim/BitSkim.prefs`, live settings with corresponding icons. **Use** writes live settings only; **Save** writes live and permanent settings. Both apply changes to the primary running client when idle. Cancel discards the editor draft.

The icon DefaultTool for settings is updated to the selected Preferences path. AmigaOS normally restores the ENV: mirror from ENVARC: during boot. Existing canonical preferences are never replaced by the installer; otherwise older shared Config/BitSkim.prefs or flat ENVARC:BitSkim.prefs can seed the new archive. Legacy read fallback is retained. The fixed canonical archive takes precedence over legacy shared settings.

Installation locations remain recorded in `ENV:BitSkim/` and `ENVARC:BitSkim/` as `app.path`, `prefs.path`, `shared.path`, `guide.path`. Repeated updates replace these files instead of appending to them. Existing registered installations show their locations and offer only Update/Uninstall, retaining the recorded main/Preferences/shared destinations. Updates move the manual to fixed `HELP:`, update its location record and remove the known old guide files. No extra path files or installer tools are installed beside either executable.

Without records, standard older layouts are detected and their documented locations reused automatically; custom unregistered volumes are not disk-scanned. A leftover BitSkim drawer that blocks the direct executable path is renamed to BitSkim.previous, with its icon and all contents preserved, before installation. If that backup name already exists, the installer stops without overwriting it. Run from the separate unpacked distribution, not from the drawer being preserved. Incomplete/invalid installation records stop the operation rather than silently permitting a second install.

Keep the unpacked package for maintenance. Uninstall removes known executables/shared documentation/manual files and installation-location records. It preserves ENV:/ENVARC: settings and their icons, old shared Config, unknown resources, received files and drawers. There is no recursive deletion. Installer tools stay in the distribution. Version remains 0.2.

## Windows 10 x64 and newer

Run **BitSkim-0.2-Windows10-x64-Setup.exe**. It installs for all users, normally in `%ProgramFiles%\BitSkim`, creates Start Menu shortcuts and registers in Installed Apps. A desktop shortcut is optional. Administrator approval is requested for installation/removal. The app runs without elevation and stores each user’s settings in `%APPDATA%\BitSkim\preferences.ini`. Close BitSkim using Quit in its tray menu before installing, updating or removing. No login item or firewall rule is installed. Launch BitSkim from the finish page or Start Menu and allow network access if Windows asks.

Use Installed Apps or **Uninstall BitSkim** in the Start Menu to remove it. The uninstaller removes only its known files and shortcuts; `%APPDATA%\BitSkim\preferences.ini`, received files and unknown installation-folder files are preserved. The portable ZIP remains available for users who do not want installation. The setup executable and app are unsigned; Windows may show a publisher/reputation prompt. Code signing remains a release task.

## macOS / Apple silicon

Open **BitSkim-0.2-macOS-arm64.dmg** and drag **BitSkim.app** onto the **Applications** shortcut. Once copying finishes, eject the image and launch BitSkim from Applications. Quit the running app before replacing it with an update. If you previously pinned a different copy in the Dock, launch the installed copy and update that Dock shortcut.

macOS 13 or later is required. The public Mac release app and DMG are Developer ID signed and notarized, with stapled tickets. Local developer builds may instead be ad-hoc signed. Fresh-machine and older-macOS compatibility remain separate checks. The DMG does not register a login item or change permissions/firewall settings. To remove the app, quit it and move it from Applications to the Trash. Preferences and received files remain. The ZIP is still supplied as an alternative.

## Building the installers

See [BUILDING.md](BUILDING.md) for the target matrix and [AMIGA.md](AMIGA.md), [MACOS.md](MACOS.md) and [WINDOWS.md](WINDOWS.md#building) for exact prerequisites, overrides and outputs.

Build the applications first. `scripts/build-amiga.sh` also compiles StartInstaller and generates Install-BitSkim plus its Workbench project icon. `scripts/build-amiga-installer.py` is the authoritative Installer-script generator; it uses an explicit file manifest for copy/removal, with no recursive deletion.

Windows setup requires NSIS (makensis): run `scripts/build-windows-installer.sh`. Override MAKENSIS/NSISDIR for an isolated toolchain. The script uses the NSIS Modern UI and current-user registry/shortcuts. See the [official NSIS scripting reference](https://nsis.sourceforge.io/Docs/Chapter4.html).

The DMG requires macOS hdiutil, Python 3 with ds-store. Run `python3 scripts/build-macos-dmg.py`. Its Finder layout settings and build script live in installers/macos and scripts. The image contains the unchanged signed application bundle, Applications shortcut and local installation/license documents. No app is installed by building the image.

Finally run `python3 scripts/package.py` to refresh portable ZIPs, source, website downloads and SHA256SUMS. It includes already-built setup/DMG artifacts when present. Version remains 0.2. See VALIDATION.md for build checks versus actual native installation testing.

## Matching source and builds

The build scripts now create build/provenance fingerprints. Run them successfully for every packaged target after finishing source/doc edits; scripts/package.py rejects old binaries, changed payloads and stale installer dependencies before modifying ZIPs or downloads. Existing setup and DMG artifacts also require current fingerprints. See the main README for commands. Build stamps are not signatures or a replacement for platform signing.

The package drawer and Installer use the owner-supplied generic software-package/install artwork. Keep BitSkim.info beside the BitSkim drawer and Install-BitSkim.info beside the Install-BitSkim script. The latter is a project icon using StartInstaller; do not point it at the BitSkim application. That outer icon belongs to the distribution only; installed shared data deliberately has no drawer icon.

The Amiga package includes `BitSkim.guide`, a linked AmigaGuide manual. Double-click its book icon or choose Help in BitSkim or BitSkimPrefs. The guide is installed in the fixed HELP: drawer and both programs find it through the installation-location metadata. It opens in `SYS:Utilities/MultiView` (or `C:MultiView`). The installer installs and removes the guide and its `.info` file.

## One-command Amiga release

`./scripts/release-amiga.sh` compiles and packages the Amiga distribution into `outputs/BitSkim-0.2.lha`, the matching `BitSkim-0.2-AmigaOS.zip`, `BitSkim-0.2.lha.sha256` and updated SHA256SUMS. Use `AMIGA_CC=/path/to/m68k-amigaos-gcc` to select a compiler. `LHA` selects a LHa-for-UNIX writer; absent one, a pinned/checksummed upstream build is created locally under work/ (see AMIGA.md for prerequisites). An installed Lhasa reader alone cannot create archives.

On Amiga, extract the complete LHA with a compatible LhA tool (for example `lha x BitSkim-0.2.lha RAM:`), keeping `BitSkim.info` beside the extracted `BitSkim` drawer. Open the drawer and run Install-BitSkim. The release script changes no system configuration, performs no guest installation and does not publish a release. It retains version 0.2.

The distribution drawer icon stores an explicit icons-only/icon-view preference. Keep the outer BitSkim.info beside the extracted BitSkim drawer to apply it. Workbench shows entries with associated .info files by default; helpers, licenses and installation assets remain in the archive and can be viewed with Show All Files. An already-open drawer or a previously retained/snapshotted icon may keep the user's existing view until the new drawer icon is used.

## Novice boot startup and launch

Novice installation copies a small launcher to `SYS:WBStartup/BitSkimStart` with a native tool icon and DONOTWAIT tooltype. It launches the main app from its recorded location, not a duplicate application binary. It returns immediately after starting the app asynchronously, and skips launch when the primary BITSKIM ARexx host is already running. A working TCP/IP stack must be available when the main app starts; the installer does not start or reconfigure Roadshow.

`ENVARC:BitSkim/startup-owned` records ownership. Updates refresh an already-owned startup launcher even in custom mode; they do not change recorded app paths. Uninstall removes that owned launcher/icon and marker while retaining preferences. A same-named startup entry without the ownership marker is not overwritten. Novice upgrades retain recorded custom locations rather than creating another default installation. An unsuccessful immediate launch reports the problem without undoing the completed installation.

Intermediate and Expert installations separately ask whether to start at boot and whether to launch after installation. These choices are independent. Choosing No for boot removes only a previously owned startup entry. Start minimized/iconified remains an app preference independent of whether WBStartup launches the app. Actual boot behavior and native novice Installer interaction still require guest testing.

Installer levels: Novice uses default paths and enables boot/immediate startup automatically. Average (Intermediate) chooses the main app and Preferences drawers; Expert additionally chooses the shared-data parent. Help and configuration paths remain fixed. Existing registered installations reuse their directories. Intermediate and Expert get boot-start questions during install/update and a launch-now question on a fresh install; updates always restart the installed app. Uninstall does not ask these questions. Immediate launch uses a temporary helper and works independently of the boot-start choice.

Uninstall never asks for directory selections at any Installer level. It uses complete recorded paths first, or the documented direct/older combined default layout when no records exist, and displays the resolved directories before removal. Unknown custom layouts with missing/partial records cannot be located reliably; restore their records rather than guessing paths. A fresh installation still offers path choices according to the selected level.

Novice users can explicitly choose Update or Uninstall for an existing installation. The script temporarily enables the mandatory management choice and removal confirmation at Average level, then restores the selected level. All other novice path/startup automation remains unchanged.

Installation/removal confirmation pages use the native Proceed control and existing Abort Install button. They do not add a redundant Cancel choice. The Update/Uninstall choice and independent Yes/No startup preferences remain unchanged.

### Amiga update lifecycle

Close BitSkimPrefs before running the installer. After update selection, confirmation and path checks, the package-only StopBitSkim helper requests normal Ctrl-C shutdown of all registered BITSKIM/BITSKIM.1 through BITSKIM.99 clients. Active transfers are cancelled and the in-memory waiting queues are discarded. The updater waits up to ten seconds for both message ports and owning tasks to disappear; it aborts before replacing program files if shutdown fails. It does not forcibly terminate processes. After successful copying and recording of installation paths, updates always launch one copy of the newly installed application, using its saved window/iconify preferences. Fresh installations retain the existing startup choices. A failed or aborted update does not start a partly updated application. StopBitSkim stays in the distribution and is not installed into Utilities, Prefs or the shared drawer.

## Windows installation scope and configuration

Both Windows installers now default to Program Files (x64 BitSkim or x86 BitSkim
Classic), use machine-wide uninstall registration and all-users shortcuts, and
request administrator approval on systems that enforce it. Older registered
per-user installations must be uninstalled first; installers stop with the exact
old path rather than running a user-writable old uninstaller elevated. Settings
and received files are preserved. Existing machine-wide installs reuse their
recorded chosen path. No installer writes a user's INI settings.

Modern Windows: `%APPDATA%\BitSkim\preferences.ini`.
Classic Windows: `%APPDATA%\BitSkim Classic\preferences.ini`, resolved through
the Shell AppData location; existing BitSkim.ini in that folder or beside a
portable executable is copied once if the new INI does not exist. Unprofiled
Windows 95 without AppData uses `%WINDIR%\BitSkim Classic\preferences.ini`.
The portable app also uses these settings paths rather than its executable
folder. Before moving a custom classic portable copy, keep BitSkim.ini beside
it for first-run migration or copy it into the new per-user settings folder.
