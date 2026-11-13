# Using BitSkim

Detailed controls and preferences for BitSkim 0.2. For installation and platform requirements, see [INSTALLATION.md](INSTALLATION.md).

## Get started on a Mac

Requires Apple silicon and macOS 13 or later. Open the Mac DMG, drag `BitSkim.app` onto Applications, eject the image and launch the installed copy. The ZIP remains an alternative. Allow local-network access when macOS requests it. If the firewall asks, incoming connections must be allowed for reception. Public Mac release packages are Developer ID signed and notarized, with stapled tickets. Local developer builds default to ad-hoc signing unless a signing identity is supplied.

Open BitSkim on another computer on the same LAN. Click a peer to choose one file, or drop a Finder file onto its tile. Use **Connect by IP** for a manual IPv4 address and optional port. Open the slider button, **BitSkim → Preferences**, or the menu-bar **Preferences…** item. Press **Save Preferences** for transfer settings; theme and startup options save immediately.

## Mac menu bar and compact window

The main window starts at 560×480 points and uses an adaptive grid of square peer tiles. Each tile shows a larger platform icon, computer name, platform and address. The entire tile accepts a file drop and highlights on hover/drag; clicking opens the file picker. The grid changes column count as the window resizes and scrolls for more peers. **Keep running in the menu bar** is enabled by default: closing the window or using its hide-to-menu-bar button keeps discovery and reception alive. A paper-plane menu-bar item provides Show BitSkim, Connect by IP, Send File To, Refresh, incoming-transfer control, themes, preferences and Quit.

Enable **Start minimized to the menu bar** to launch without a main window or Dock icon. Showing the main window brings back the Dock icon; hiding/closing it returns to menu-bar operation. Incoming offers that require a decision, and save panels that require a destination, reveal the main window. Auto-accept to a configured folder can remain unattended. Turning off menu-bar mode restores normal quit-on-main-window-close behavior. This setting controls launch appearance, not login-at-startup registration.

## Themes

All three clients offer **Catppuccin Latte, Frappe, Macchiato, Mocha, BitSkim Rosewater, BitSkim Mint, Arctic and Classic**. The first four are official Catppuccin palettes; Rosewater and Mint are BitSkim accent variants. All clients default to Catppuccin Mocha and remember their choice.

Mac: use Preferences or the menu-bar Theme submenu. The main window and preferences change immediately; standard macOS menus, consent dialogs and file panels retain native styling.

Amiga: open Preferences, select **Appearance** in the sidebar, and use the **Theme** selector to preview a sample client window. Click to cycle forward, or Shift-click to cycle backward; **Use** or **Save** applies the selection. Both the client and preferences open as ordinary Workbench windows. Their content, action/sidebar buttons, checkboxes, theme selector and peer rows use the selected palette, with pressed/selected states. Intuition title bars and system requesters follow Workbench styling. No private application screen is opened and other applications' colors are preserved. Low-color screens approximate colors using available pens. Choose **Start iconified** under General to begin directly as a Workbench AppIcon without opening the main window.

Palette source, provenance and license: [assets/themes/](../assets/themes/). Generated C/Swift tables are committed; run `python3 scripts/generate-themes.py` after editing the shared JSON palette source.

## Get started on Windows

Run the Windows 10 x64 Setup.exe for an all-users Program Files installation with Start Menu shortcuts and an uninstaller, or unzip the portable package and open `BitSkim.exe`. Setup requests administrator approval; running the app does not. Drop one file directly onto a nearby computer’s tile, or select a tile and choose Send file. Each tile is a large drop zone with recipient hover feedback. You can also enter an IPv4 address and use Send file. The tray menu provides Show, Preferences and Quit. Preferences includes themes, receiving policy and optional startup minimized to the tray. See [Windows setup and validation](WINDOWS.md).

The executable targets Windows 10 x64. Automated native engine checks passed in a Windows 11 ARM; Windows 10 and native GUI interaction still need verification.

## Get started on an Amiga

Unpack the Amiga LHA or ZIP and double-click `Install-BitSkim` in its drawer. Novice installation uses default paths, enables WBStartup and starts the app after installation; custom mode chooses the main app, Preferences and shared-data parent locations; Help and configuration use fixed system locations. You can also copy the complete drawer manually into `Bin:`. Launch `Bin:BitSkim/BitSkim` or `Bin:BitSkim/BitSkimPrefs`; each has a distinct native Workbench icon. Keep the drawer’s `BitSkim.info` alongside the drawer itself.

Requires AmigaOS 3.x, a Workbench screen at least 640×256, and an AmiTCP-compatible `bsdsocket.library` version 4 or later (Roadshow is the initial real-network-stack target). The executable targets the 68000 instruction set; real 68000 hardware performance and minimum RAM are not yet certified.

Copy `BitSkim` and `BitSkim.info` into the same drawer, then double-click the icon. Both programs request a 32 KiB stack at startup, so Shell, ARexx and dock launches should not need a `Stack` command (not yet verified on real hardware). Select a peer and choose **Send file...** to enter a full Amiga file path, or choose the file in Workbench and drop it into the main window. The send dialog shows and retains its recipient; a drop into the main window uses the currently selected peer. The drop targets the selected peer; when no peer is selected it uses the manual IP field. Entering an IP clears the selected peer when the field is committed. Manual addresses may include a port, for example `127.0.0.1:46440`.

**Preferences** opens the separate installed `BitSkimPrefs` program, normally in `SYS:Prefs`; a portable copy can keep the two executables together. Use or Save reloads the running primary client when idle. The editor works without a TCP/IP stack. Its sidebar contains **General**, **Appearance**, **Receiving** and **About**. Edits survive section changes; **Revert** restores all opening settings and **Cancel** discards the draft. See [ARexx commands and integration examples](AMIGA-AREXX.md) for dock and Directory Opus scripting.

Preferences uses `ENV:BitSkim/BitSkim.prefs` for live settings and `ENVARC:BitSkim/BitSkim.prefs` for permanent settings. **Use** writes only the live copy; **Save** writes both. The default download drawer is `RAM:` and asking for a location is enabled. Choose a persistent disk drawer before using unattended reception. Hostname is used when available, falling back to BitSkim. The classic client assumes Latin-1 names; incoming names must be representable in Latin-1 and at most 30 characters. Rename incompatible files on the sender first.

A TCP/IP stack needs its compatible network driver and configuration.

## Amiga iconify and background operation

Choose **Iconify** in the main window to hide BitSkim and leave a running AppIcon on the Workbench desktop. Incoming offers, discovery replies and current transfers continue using the same network engine. While iconified, outgoing discovery searches and peer-list tracking pause and the cached list is cleared, except when queued sends still need recipient discovery. Visible or iconified, BitSkim sleeps until network, Workbench, ARexx or break signals need attention instead of polling. Restoring the window resumes searches and immediately refreshes the list. Double-click the desktop **BitSkim** icon to reopen the window in the selected theme. To quit, restore the window and use its close gadget.

Enable **Start iconified** in Preferences and press Save to start future launches directly as a Workbench AppIcon. It is off by default and does not add anything to User-Startup. If Workbench cannot register an icon, BitSkim stays visible rather than running without a way to reopen it. The app loads its bundled `BitSkim.info` through icon.library, falling back to a default tool icon when necessary.

Incoming offers show a modeless Accept/Decline window with an editable full save path. The main task keeps networking, timeouts and ARexx running while it is open. Closing the offer declines it; remote cancellation/timeout closes the stale offer. With **Automatically accept offers** enabled and **Always ask where to save** off, reception stays iconified and saves to the configured folder. Use a persistent disk drawer instead of `RAM:` for files that must survive a restart. Normal client notices are modeless too. Startup errors before the ARexx host exists and the separate preferences editor may use standard requesters.

The desktop AppIcon is a restore control, not a peer destination. Dropping files onto it reopens BitSkim and explains how to send from the window; it does not guess a recipient or send files automatically. Enable **Close window to iconify** under General to make the main window’s close gadget hide BitSkim as an AppIcon. This is off by default. The **Quit** button and ARexx `QUIT` always exit; the preferences window’s close gadget still discards its draft. Saving reloads the option in the running client when idle. If icon registration fails, the main window stays open.

## Preferences and receiving policy

| Setting | Behavior |
|---|---|
| Client name | Editable; hostname default, bounded UTF-8 wire representation |
| Visible | Advertise/respond to discovery; hidden clients can still search |
| Allow incoming transfers | Permit incoming connections and file offers |
| Auto-accept | Skip accept/decline; off by default |
| Always ask where to save | Show the editable save-path dialog, even with auto-accept enabled |
| Download folder | Used for unattended reception when “always ask” is off |

Visibility is not access control. Turning visibility off still allows manual-IP reception when incoming transfers remain enabled. Names, IPs and installation IDs are not authenticated identities. A native confirmation dialog/requester is the incoming notification; OS notification-center delivery is not implemented.

## Queues, TLS and resume

See [TRANSFERS.md](TRANSFERS.md) for multi-file selection, full batch lists and accept-all approval, legacy per-file fallback, retry/skip controls, shared-key TLS modes and resuming retained downloads. TLS builds use pinned OpenSSL 3.5.9 (Apache-2.0), bootstrapped by `scripts/build-openssl.sh`; Perl, make and network access are needed on the first build.

### Transfer speed limits

Each client provides separate send and receive limits in its Network preferences (KiB/s). Zero means Unlimited; the maximum is 1,048,575 KiB/s. Settings apply to the next connection/file. Limits cover file payload, not discovery, control messages or encryption overhead. Small bounded bursts are permitted. A sender uses the lower of its own upload limit and the recipient’s download limit. Limited reception requires an updated sender; legacy senders are refused before approval when a receive limit is set. Unlimited transfers remain compatible with older clients. DOS also accepts `LIMITS <sendKiB> <receiveKiB>`.

Transfer dialogs remember separate send/receive folders, show directory totals and retain completion counts. Cancel stops the whole directory. Tools includes a no-file TCP connection test; see [transfer behavior and network checks](TRANSFER-EXPERIENCE.MD).

## Receive approval and queue controls

On macOS and modern Windows, **Auto-accept only verified devices** is on by default.
When automatic acceptance is enabled, unknown devices still show an approval prompt
unless this additional restriction is turned off. Remembered device trust requires
an encrypted connection with a matching saved public key; a computer name or a
shared-key connection alone is not a verified-device pin.

On macOS, select multiple files or folders in the send picker to append them to
the queue. Adding items preserves existing entries and leaves a paused queue paused.
An expired or cancelled receive dialog cannot approve a later offer.
