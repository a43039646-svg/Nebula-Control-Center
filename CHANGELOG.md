# Changelog

## 2.2.0 — Cross-distribution preview

- Added a narrow administrator helper for sending `SIGTERM` to protected processes through polkit authentication.
- Added process start-time checks to reduce the chance of acting on a reused PID.
- Kept the whole GTK application running as the normal user; the app does not collect or store administrator passwords.
- Added package-manager-aware dependency setup for APT, DNF, pacman, Zypper, XBPS, Portage and APK.
- Added an explicit confirmation and `--dry-run` mode to the dependency installer.
- Added a `make test` target and CI build matrix for Debian, Fedora, Arch Linux and Alpine.
- Added service listing fallbacks for systemd, OpenRC and runit where their tools work.
- Disabled gaming power-profile controls when `powerprofilesctl` is unavailable and report unsupported changes instead of assuming success.
- Kept optional distribution integrations from being hard requirements for core monitoring.

This is a preview release. GTK GUI build and runtime behavior still need to be tested on the target distribution; the development environment used to prepare this archive does not include GTK4 development packages.
