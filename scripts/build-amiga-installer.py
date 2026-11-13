#!/usr/bin/env python3
"""Generate a package-only Installer with separate application/Prefs destinations."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/amiga';out.mkdir(parents=True,exist_ok=True)
app_files=['BitSkim','BitSkim.info']
prefs_files=['BitSkimPrefs','BitSkimPrefs.info']
guide_files=['BitSkim.guide','BitSkim.guide.info']
shared_files=['BUILD-PROVENANCE.json','README.MD','LICENSE','CATPPUCCIN-LICENSE','THIRD-PARTY-NOTICES.MD','NOVADOCK-LICENSE','INSTALLATION.md','ARexx/README.MD','ARexx/NOVADOCK.md']
shared_files+=['ARexx/'+p.name for p in sorted((ROOT/'examples/arexx').glob('*.rexx'))]
legacy=['BitSkim.prefs-path','BitSkimPrefs.guide-path']
script=r'''; BitSkim 0.2 - Commodore Installer V42+ (AmigaOS 3.x).
; Keep this script and StartInstaller in the unpacked distribution only.
(set @app-name "BitSkim")
(set @default-dest "SYS:Utilities")
(if (= @icon "") (set #src (expandpath "")) (set #src (pathonly (expandpath @icon))))
(if (= #src "") (set #src (expandpath "")))
(welcome)
(message "Close BitSkimPrefs first. Updates automatically close running BitSkim clients, cancel active transfers and discard waiting queues, then restart the installed app.\nSettings and received files are preserved. The installer stays in this unpacked package.")
; BEGIN installation discovery
(set #selected-level @user-level)
(set #novice (= @user-level 0))
(set #expert (= @user-level 2))
(set #installed 0)
(set #legacy-layout 0)
(set #recorded 0)
(set #app "")
(set #prefs "")
(set #shared "")
(set #manual "")
; Restore a missing live location record from its persistent copy.
(if (and (= (exists "ENV:BitSkim/app.path") 0) (= (exists "ENVARC:BitSkim/app.path") 1))
 (
  (if (<> (exists "ENV:BitSkim") 2) (makedir "ENV:BitSkim"))
  (copyfiles (prompt "Reading recorded BitSkim installation") (help @copyfiles-help)
   (source "ENVARC:BitSkim/app.path") (dest "ENV:BitSkim") (optional "force"))
 ))
(if (= (exists "ENV:BitSkim/app.path") 1)
 (
  (set #recorded 1)
  (if (> (getsize "ENV:BitSkim/app.path") 1023) (abort "A recorded BitSkim path is too long. Nothing was installed or removed."))
  (set #app (getenv "BitSkim/app.path"))
 ))
; Restore a missing live location record from its persistent copy.
(if (and (= (exists "ENV:BitSkim/prefs.path") 0) (= (exists "ENVARC:BitSkim/prefs.path") 1))
 (
  (if (<> (exists "ENV:BitSkim") 2) (makedir "ENV:BitSkim"))
  (copyfiles (prompt "Reading recorded BitSkim installation") (help @copyfiles-help)
   (source "ENVARC:BitSkim/prefs.path") (dest "ENV:BitSkim") (optional "force"))
 ))
(if (= (exists "ENV:BitSkim/prefs.path") 1)
 (
  (set #recorded 1)
  (if (> (getsize "ENV:BitSkim/prefs.path") 1023) (abort "A recorded BitSkim path is too long. Nothing was installed or removed."))
  (set #prefs (getenv "BitSkim/prefs.path"))
 ))
; Restore a missing live location record from its persistent copy.
(if (and (= (exists "ENV:BitSkim/shared.path") 0) (= (exists "ENVARC:BitSkim/shared.path") 1))
 (
  (if (<> (exists "ENV:BitSkim") 2) (makedir "ENV:BitSkim"))
  (copyfiles (prompt "Reading recorded BitSkim installation") (help @copyfiles-help)
   (source "ENVARC:BitSkim/shared.path") (dest "ENV:BitSkim") (optional "force"))
 ))
(if (= (exists "ENV:BitSkim/shared.path") 1)
 (
  (set #recorded 1)
  (if (> (getsize "ENV:BitSkim/shared.path") 1023) (abort "A recorded BitSkim path is too long. Nothing was installed or removed."))
  (set #shared (getenv "BitSkim/shared.path"))
 ))
; Restore a missing live location record from its persistent copy.
(if (and (= (exists "ENV:BitSkim/guide.path") 0) (= (exists "ENVARC:BitSkim/guide.path") 1))
 (
  (if (<> (exists "ENV:BitSkim") 2) (makedir "ENV:BitSkim"))
  (copyfiles (prompt "Reading recorded BitSkim installation") (help @copyfiles-help)
   (source "ENVARC:BitSkim/guide.path") (dest "ENV:BitSkim") (optional "force"))
 ))
(if (= (exists "ENV:BitSkim/guide.path") 1)
 (
  (set #recorded 1)
  (if (> (getsize "ENV:BitSkim/guide.path") 1023) (abort "A recorded BitSkim path is too long. Nothing was installed or removed."))
  (set #manual (getenv "BitSkim/guide.path"))
 ))
(set #appdefault "SYS:Utilities")
(set #prefsdefault "SYS:Prefs")
(set #shareddefault "SYS:")
(set #guide "HELP:")
(set #oldguide "")
(if #recorded
 (
  (if (or (= #app "") (= #prefs "") (= #shared ""))
   (abort "Incomplete BitSkim installation records. Restore the app, prefs and shared .path files in ENVARC:BitSkim and ENV:BitSkim. No new installation is allowed while these records remain."))
  (if (or (<> (fileonly #app) "BitSkim") (<> (fileonly #prefs) "BitSkimPrefs"))
   (abort "Invalid BitSkim installation records. Nothing was installed or removed."))
  (set #dest (pathonly #app))
  (set #prefdest (pathonly #prefs))
  (if (<> #manual "") (set #oldguide (pathonly #manual)))
  (set #installed 1)
 )
 (
  ; Probe known older/default layouts; do not scan arbitrary volumes.
  (if (or (= (exists "SYS:Utilities/BitSkim") 1) (or (= (exists "SYS:Utilities/BitSkim/BitSkim") 1) (and (= (exists "SYS:Utilities/BitSkim") 2) (exists "SYS:Utilities/BitSkim/Install-BitSkim"))))
   (set #installed 1))
  (if (or (= (exists "SYS:Utilities/BitSkim/BitSkim") 1) (and (= (exists "SYS:Utilities/BitSkim") 2) (exists "SYS:Utilities/BitSkim/Install-BitSkim")))
   (
    (set #legacy-layout 1)
    (set #appdefault "SYS:Utilities/BitSkim")
    (set #shareddefault (pathonly #appdefault))
    (set #oldguide #appdefault)
    (if (= (exists "SYS:Utilities/BitSkim/BitSkimPrefs") 1) (set #prefsdefault #appdefault))
   ))
  (if #installed
   (message "An older BitSkim installation was found without complete location records. The documented installation directories will be used automatically. Only update or uninstall will be offered."))
(if (or #novice #installed)
 (
  (set #dest #appdefault)
  (set #prefdest #prefsdefault)
  (set #shared (tackon #shareddefault "BitSkim"))
 )
 (
(set #dest (askdir
 (prompt "Choose the main BitSkim application drawer.")
 (help "Only BitSkim and its icon go here. Select the same locations for updates/removal.")
 (default #appdefault)))
(set #prefdest (askdir
 (prompt "Choose the drawer for BitSkimPrefs.")
 (help "Only BitSkimPrefs and its icon go here. The usual location is SYS:Prefs.")
 (default #prefsdefault)))
(set #sharedparent #shareddefault)
(if #expert
 (set #sharedparent (askdir
 (prompt "Choose the parent drawer for shared components. A BitSkim drawer is created inside it.")
 (help "Only shared files use a BitSkim subdrawer here. Executables go directly into their selected drawers. User files are preserved.")
 (default #shareddefault))))
(set #shared (tackon #sharedparent "BitSkim"))
 ))
  (if (= (exists (tackon #dest "BitSkim")) 1) (set #installed 1))
 ))
; Reject source/destination filename conflicts before any filesystem mutation.
(set #blocking-drawer (= (exists (tackon #dest "BitSkim")) 2))
(if (= (expandpath #src) (expandpath #dest)) (abort "A destination cannot be the unpacked source drawer."))
(if (= (expandpath #src) (expandpath #prefdest)) (abort "A destination cannot be the unpacked source drawer."))
(if (= (expandpath #src) (expandpath #shared)) (abort "A destination cannot be the unpacked source drawer."))
(if (= (expandpath #src) (expandpath #guide)) (abort "A destination cannot be the unpacked source drawer."))
(set #locations (cat "Main app: " #dest "\nPreferences: " #prefdest "\nShared components: " #shared "\nAmigaGuide: " #guide))
(if #installed
 (
  (message (cat "Existing BitSkim installation\n\n" #locations))
  (if (and (<> #oldguide "") (<> (expandpath #oldguide) (expandpath #guide)))
   (message (cat "Previous guide directory: " #oldguide "\nUpdate moves the manual directly to HELP:.")))
  (if (<> (exists (tackon #dest "BitSkim")) 1)
   (message "The recorded main executable is missing. Update can restore it; Uninstall removes the remaining known files."))
  ; Native Novice skips askchoice. Lift only this mandatory decision.
  (if #novice (user 1))
  (set #action (askchoice (prompt "Manage this installed copy of BitSkim")
   (help "Update reuses the displayed directories. Uninstall removes only known BitSkim files and keeps configuration and unknown files.")
   (choices "Update BitSkim" "Uninstall BitSkim") (default 0)))
  (user #selected-level)
 )
 (
  (if (not #novice) (message (cat "Install BitSkim in these locations:\n\n" #locations "\n\nProceed to install, or use Abort Install to stop.")))
  (set #action 0)
 ))
(set #startup-owned (= (exists "ENVARC:BitSkim/startup-owned") 1))
(set #install-boot 0)
(set #launch-now 0)
(if (= #action 0)
 (if #novice
  ((set #install-boot 1) (set #launch-now 1))
  (
   (set #install-boot (askbool (prompt "Start BitSkim at boot?")
    (help "Enable the owned Workbench startup launcher. Choosing No removes only a previously owned entry.")
    (choices "Yes" "No") (default #startup-owned)))
   (if #installed (set #launch-now 1)
    (set #launch-now (askbool (prompt "Start BitSkim after installation?")
     (help "Launch the installed application when copying finishes.") (choices "Yes" "No") (default 1))))
  )))
(if (and #launch-now (exists "T:BitSkim-InstallLaunch"))
 (abort "A previous temporary BitSkim-InstallLaunch exists in T:. Remove it before retrying installation."))
(if (and (= #action 0) #install-boot)
 (if (and (not #startup-owned) (or (exists "SYS:WBStartup/BitSkimStart") (exists "SYS:WBStartup/BitSkimStart.info")))
  (abort "An unowned BitSkimStart entry exists in SYS:WBStartup. Nothing was installed or replaced. Move it aside before using novice installation.")))
; END installation discovery
(if (= #action 0)
(
 (if (<> (exists (tackon #src "BitSkim")) 1) (abort "The source package is incomplete."))
 (if (<> (exists (tackon #src "BitSkimPrefs")) 1) (abort "The source package has no BitSkimPrefs."))
 ; BEGIN installation path checks
 ; Installation-only path collision checks must not prevent Uninstall.
(if (= (exists (tackon #prefdest "BitSkimPrefs")) 2) (abort "The Preferences destination contains a conflicting BitSkimPrefs drawer."))
(if (or (= #shared (tackon #dest "BitSkim")) (and (exists #shared) (exists (tackon #dest "BitSkim")) (= (expandpath #shared) (expandpath (tackon #dest "BitSkim"))))) (abort "The shared drawer cannot have the same path as the main executable."))
(if (or (= #shared (tackon #prefdest "BitSkimPrefs")) (and (exists #shared) (exists (tackon #prefdest "BitSkimPrefs")) (= (expandpath #shared) (expandpath (tackon #prefdest "BitSkimPrefs"))))) (abort "The shared drawer cannot have the same path as the Preferences executable."))
 ; END installation path checks

 ; BEGIN stop running clients
 (if #installed
 (
  (if (<> (exists (tackon #src "StopBitSkim")) 1) (abort "The source package has no StopBitSkim helper. No program files were replaced."))
  (if (exists "T:BitSkim-InstallStop") (abort "A previous temporary BitSkim-InstallStop exists in T:. Remove it before retrying."))
  (copyfiles (prompt "Closing running BitSkim clients before update") (help "Wait for normal shutdown. No process is forcibly killed.")
   (source (tackon #src "StopBitSkim")) (dest "T:") (newname "BitSkim-InstallStop") (optional "force"))
  (set #stop-result (run "T:BitSkim-InstallStop"))
  (delete "T:BitSkim-InstallStop" (optional "force"))
  (if (<> #stop-result 0) (abort "BitSkim did not exit within 10 seconds. Close all BitSkim clients, then retry. No program files were replaced."))
 ))
 ; END stop running clients
 ; BEGIN preserve blocking drawer
 (if #blocking-drawer
 (
  (set #old-drawer (tackon #dest "BitSkim"))
  (set #backup-drawer (tackon #dest "BitSkim.previous"))
  (if (or (exists #backup-drawer) (exists (cat #backup-drawer ".info")))
   (abort "BitSkim.previous already exists. Keep that backup and move it elsewhere before retrying; nothing was overwritten."))
  (if (= (expandpath #src) (expandpath #old-drawer))
   (abort "Run the installer from the separate Bin distribution, not from the old drawer being preserved."))
  (rename #old-drawer #backup-drawer)
  (if (exists (cat #old-drawer ".info"))
   (rename (cat #old-drawer ".info") (cat #backup-drawer ".info")))
 ))
 ; END preserve blocking drawer
 (if (<> (exists #dest) 2) (makedir #dest))
 (if (<> (exists #prefdest) 2) (makedir #prefdest))
 (if (<> (exists #shared) 2) (makedir #shared))
 (if (<> (exists "HELP:") 2) (abort "HELP: is unavailable. Restore the standard HELP: assignment before installing."))
 (if (<> (exists #guide) 2) (makedir #guide))
 (if (<> (exists (tackon #shared "ARexx")) 2) (makedir (tackon #shared "ARexx")))
 (if (<> (exists (tackon #shared "Plugins")) 2) (makedir (tackon #shared "Plugins")))
 (if (<> (exists (tackon #shared "Images")) 2) (makedir (tackon #shared "Images")))
 (if (<> (exists (tackon #shared "Tools")) 2) (makedir (tackon #shared "Tools")))
 (if (<> (exists (tackon #shared "Themes")) 2) (makedir (tackon #shared "Themes")))
'''
for name in app_files+prefs_files+guide_files+shared_files:
 base='#prefdest' if name in prefs_files else '#guide' if name in guide_files else '#shared' if name in shared_files else '#dest'
 dest='(tackon #shared "ARexx")' if '/' in name else base
 copy='(copyfiles (prompt "Installing '+name+'") (help @copyfiles-help) (source (tackon #src "'+name+'")) (dest '+dest+') (optional "force"))'
 if name.endswith('.info') and name in ('BitSkim.info','BitSkimPrefs.info'):
  copy='(if (= (exists (tackon '+base+' "'+name+'")) 0) '+copy+')'
 script+=' '+copy+'\n'
script+=' (if (exists (cat #shared ".info") (noreq)) (delete (cat #shared ".info") (optional "force")))\n'
# Keep all installation path metadata out of Utilities/Prefs.
for base in ('ENV:BitSkim','ENVARC:BitSkim'):
 script+=' (if (<> (exists "'+base+'") 2) (makedir "'+base+'"))\n'
 for leaf,value in [('app.path','(tackon #dest "BitSkim")'),('prefs.path','(tackon #prefdest "BitSkimPrefs")'),('shared.path','#shared'),('guide.path','(tackon #guide "BitSkim.guide")')]:
  script+=' (if (exists "'+base+'/'+leaf+'" (noreq)) (delete "'+base+'/'+leaf+'" (optional "force")))\n'
  script+=' (textfile (dest "'+base+'/'+leaf+'") (prompt "Recording BitSkim locations") (help "Shared installation paths for the client and Preferences.") (append '+value+'))\n'
# Preserve settings, migrating the old shared/flat archive only when no new archive exists.
script+=r''' (if (= (exists "ENVARC:BitSkim/BitSkim.prefs") 0)
 (
  (if (= (exists (tackon (tackon #shared "Config") "BitSkim.prefs")) 1)
   (copyfiles (prompt "Migrating saved settings") (help @copyfiles-help)
    (source (tackon (tackon #shared "Config") "BitSkim.prefs")) (dest "ENVARC:BitSkim") (optional "force"))
   (if (= (exists "ENVARC:BitSkim.prefs") 1)
    (copyfiles (prompt "Migrating saved settings") (help @copyfiles-help)
     (source "ENVARC:BitSkim.prefs") (dest "ENVARC:BitSkim") (optional "force"))
    (textfile (dest "ENVARC:BitSkim/BitSkim.prefs") (prompt "Creating default settings")
     (help "Existing settings are never replaced.") (append "theme=mocha\ntheme_defaults=1\n"))))
 ))
 (if (= (exists "ENV:BitSkim/BitSkim.prefs") 0)
  (copyfiles (prompt "Preparing live settings") (help @copyfiles-help)
   (source "ENVARC:BitSkim/BitSkim.prefs") (dest "ENV:BitSkim") (optional "force")))
'''
for base in ('ENV:','ENVARC:'):
 script+=' (if (= (exists "'+base+'BitSkim.info") 0) (copyfiles (prompt "Installing visible drawer icon") (help @copyfiles-help) (source (tackon #src "InstallAssets/'+('HelpDrawer.info' if base=='HELP:' else 'SettingsDrawer.info')+'")) (dest "'+base+'") (newname "BitSkim.info") (optional "force")))\n'
for base in ('ENV:BitSkim','ENVARC:BitSkim'):
 script+=' (if (= (exists "'+base+'/BitSkim.prefs.info") 0) (copyfiles (prompt "Installing settings icon") (help @copyfiles-help) (source (tackon #src "InstallAssets/BitSkim.prefs.info")) (dest "'+base+'") (optional "force")))\n'
 script+=' (tooltype (dest "'+base+'/BitSkim.prefs") (setdefaulttool (tackon #prefdest "BitSkimPrefs")) (prompt "Linking settings to Preferences") (help "Uses your selected Preferences location."))\n'
def delete(base,name):
 return ' (if (exists (tackon '+base+' "'+name+'") (noreq)) (delete (tackon '+base+' "'+name+'") (optional "force")))\n'
script+=' (if (and (<> #oldguide "") (<> (expandpath #oldguide) (expandpath #guide)))\n (\n'
for name in guide_files:script+=delete('#oldguide',name)
script+=' ))\n'
# Clean only known legacy package-only files from older installations.
for name in legacy:script+=delete('#dest',name)
script+=' (if (<> (expandpath #dest) (expandpath #prefdest))\n (\n'
for name in prefs_files+['BitSkimPrefs.guide-path']:script+=delete('#dest',name)
script+=r''' ))
 (if #install-boot
 (
  (if (<> (exists "SYS:WBStartup") 2) (makedir "SYS:WBStartup"))
  (copyfiles (prompt "Installing BitSkim boot launcher") (help @copyfiles-help)
   (source (tackon #src "StartBitSkim")) (dest "SYS:WBStartup") (newname "BitSkimStart") (optional "force"))
  (copyfiles (prompt "Installing BitSkim startup icon") (help @copyfiles-help)
   (source (tackon #src "InstallAssets/Startup.info")) (dest "SYS:WBStartup") (newname "BitSkimStart.info") (optional "force"))
  (tooltype (dest "SYS:WBStartup/BitSkimStart") (settooltype "DONOTWAIT" "YES")
   (prompt "Enabling background boot launch") (help "Workbench does not wait for BitSkim to exit."))
  (if (= (exists "ENVARC:BitSkim/startup-owned") 0)
   (textfile (dest "ENVARC:BitSkim/startup-owned") (prompt "Recording startup ownership")
    (help "Only this owned entry is removed by Uninstall.") (append "BitSkimStart 0.2")))
 ))
 (if (and (not #install-boot) #startup-owned)
 (
  (if (exists "SYS:WBStartup/BitSkimStart" (noreq)) (delete "SYS:WBStartup/BitSkimStart" (optional "force")))
  (if (exists "SYS:WBStartup/BitSkimStart.info" (noreq)) (delete "SYS:WBStartup/BitSkimStart.info" (optional "force")))
  (if (exists "ENV:BitSkim/startup-owned" (noreq)) (delete "ENV:BitSkim/startup-owned" (optional "force")))
  (delete "ENVARC:BitSkim/startup-owned" (optional "force"))
 ))
 (if #launch-now
 (
  (copyfiles (prompt "Preparing immediate launch") (help @copyfiles-help)
   (source (tackon #src "StartBitSkim")) (dest "T:") (newname "BitSkim-InstallLaunch") (optional "force"))
  (set #launch-result (run "T:BitSkim-InstallLaunch"))
  (delete "T:BitSkim-InstallLaunch" (optional "force"))
  (if (<> #launch-result 0) (message "BitSkim was installed but could not start now. Check the TCP/IP stack and launch it manually."))
 ))
 (complete 100)
 (exit (cat "BitSkim: " #dest "\nBitSkimPrefs: " #prefdest "\nShared: " #shared "\nGuide: " #guide "\n\nKeep the unpacked package for updates and removal. The installer was not installed."))
)
(
 (if (= #installed 0) (abort "There is no detected installation to uninstall."))
 ; Ask removal confirmation even when the selected level is Novice.
 ; BEGIN uninstall confirmation
 (if #novice (user 1))
 (message "Uninstall BitSkim from the displayed locations.\n\nSettings, received files and unrelated files will be kept.\n\nProceed to remove the known program files, or use Abort Install to stop.")
 (set #remove-approved 1)
 (user #selected-level)
 ; END uninstall confirmation
 (if #remove-approved
 (
'''
for base,names in [('#dest',app_files+legacy),('#prefdest',prefs_files+['BitSkimPrefs.guide-path']),('#shared',shared_files),('#guide',guide_files)]:
 for name in names:script+=delete(base,name)
script+=' (if #legacy-layout\n (\n'
for name in ('Install-BitSkim','Install-BitSkim.info','StartInstaller'):script+=delete('#dest',name)
script+=' ))\n'
script+=' (if (and (<> #oldguide "") (<> (expandpath #oldguide) (expandpath #guide)))\n (\n'
for name in guide_files:script+=delete('#oldguide',name)
script+=' ))\n'
script+=r''' (if #startup-owned
 (
  (if (exists "SYS:WBStartup/BitSkimStart" (noreq)) (delete "SYS:WBStartup/BitSkimStart" (optional "force")))
  (if (exists "SYS:WBStartup/BitSkimStart.info" (noreq)) (delete "SYS:WBStartup/BitSkimStart.info" (optional "force")))
  (if (exists "ENV:BitSkim/startup-owned" (noreq)) (delete "ENV:BitSkim/startup-owned" (optional "force")))
  (delete "ENVARC:BitSkim/startup-owned" (optional "force"))
 ))
'''
for base in ('ENV:BitSkim','ENVARC:BitSkim'):
 for leaf in ('app.path','prefs.path','shared.path','guide.path'):
  script+=' (if (exists "'+base+'/'+leaf+'" (noreq)) (delete "'+base+'/'+leaf+'" (optional "force")))\n'
script+=r'''
  (exit "BitSkim files were removed. Shared user files, ENV:/ENVARC: settings and unrelated files were kept.")
 )
 (exit (quiet)))
))
'''
(out/'Install-BitSkim').write_text(script)
from amiga_package_icons import build
build(ROOT)
(out/'Install-BitSkim.info').write_bytes((ROOT/'assets/icons/Install-BitSkim.info').read_bytes())
print('Built package-only Amiga Installer with three destination selections')
