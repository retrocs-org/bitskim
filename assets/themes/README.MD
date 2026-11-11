# Shared palettes

`palettes.json` is the single source for both native clients. Run `python3 scripts/generate-themes.py` after editing; the generated Swift and C files are included in source releases, so ordinary builds need no generator dependency.

Latte, Frappe, Macchiato and Mocha use colors from the official Catppuccin palette (https://catppuccin.com/palette/ and https://github.com/catppuccin/palette), under its included MIT license. BitSkim Rosewater and BitSkim Mint are application-specific accent variants, not additional official Catppuccin flavors. Arctic and Classic are BitSkim palettes.

Mac: all eight palettes color the window and preferences, with matching light/dark native control appearance. Menu-bar menus, system alerts and file panels retain native macOS styling.

Amiga: Classic uses the current Workbench palette. Other choices use a private BitSkim screen with eight allocated colors (an eight-bit screen on RTG or four-bit screen on planar displays), and matching DrawInfo pens for GadTools and requesters. This avoids modifying Workbench's global palette. Screen mode/resolution follow Workbench; if the mode cannot open a suitable screen the app falls back to Classic. Changing screens keeps the transfer engine running, but is allowed only while idle.
