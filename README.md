# Nebula Control Center v2.1.1

A native Linux control center written in C + GTK4.

## v2.0 modules

- Overview
- Processes
- Hardware
- Services
- Storage
- Network
- Power
- System Doctor
- Startup
- Gaming Mode
- Permissions
- Command Console
- Plugins
- Settings

## New v2.0 features

### System Doctor
Checks common Linux interfaces, root disk pressure, package manager availability,
failed systemd services, network tools and admin helpers.

### Startup
Shows `.desktop` entries found in the user's autostart directory and `/etc/xdg/autostart`.

### Gaming Mode
Detects GameMode, MangoHud and `powerprofilesctl`.
Provides Performance and Balanced profile actions when `powerprofilesctl` is available.

### Permissions
Shows whether the application is running as root and which Linux/system
interfaces and admin helpers are available.

### Command Console
Provides a small set of predefined read-only diagnostics:
- kernel / OS
- root disk usage
- network addresses
- failed services

The console does not execute arbitrary user-entered shell commands.

### Plugins
Discovers `.desktop` and `.plugin` descriptors from:
- `~/.local/share/nebula-control-center/plugins`
- `/usr/local/share/nebula-control-center/plugins`

Plugin loading/execution is intentionally not implemented yet.

## Localization

Locale files live under `locales/`. The project contains approximately 100
language slots with English fallback for missing translations.

## Build

On Debian/Ubuntu/Linux Mint:

    sudo apt install build-essential pkg-config libgtk-4-dev pciutils

Then:

    make
    ./nebula-control-center

Install system-wide:

    sudo make install

## Important

This release is a large development milestone. I recommend running it first
and testing each module before adding more functionality. The design goal is
to keep system operations explicit and understandable instead of hiding
dangerous shell commands behind the UI.

## Project structure

    src/
      main.c
      system_info.c/.h
      process_manager.c/.h
      storage.c/.h
      network.c/.h
      services.c/.h
      i18n.c/.h
      doctor.c/.h
      startup.c/.h
      gaming.c/.h
      permissions.c/.h
      plugins.c/.h

    locales/
      *.lang


## v2.0.1 hotfix

Fixed the startup segmentation fault caused by the Command Console label array not being NULL-terminated before passing it to GTK4 `gtk_drop_down_new_from_strings()`.


## v2.1

### Plugins removed

The experimental plugin descriptor system was removed from the main application.
Nebula Control Center v2.1 does not load or execute third-party plugins.

This keeps the control center smaller and avoids introducing an extension API before there is a defined security model.

### Localization

All v2.0/v2.1 UI strings are routed through the localization system.
The locale directory still supports 100+ languages. English is the fallback for untranslated strings; Ukrainian, Russian and Danish include translations for the new v2.x UI.

### Current modules

- Overview
- Processes
- Hardware
- Services
- Storage
- Network
- Power
- System Doctor
- Startup
- Gaming Mode
- Permissions
- Command Console
- Settings


## v2.1.1

- Added the official Nebula Control Center app icon.
- Desktop entry now uses `Icon=nebula-control-center`.
- `make install` installs the icon into the standard hicolor icon theme.
- `make uninstall` removes the installed icon as well.
