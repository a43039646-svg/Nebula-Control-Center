# Nebula Control Center 2.2.0

A native Linux system control center built with **C + GTK4**. Nebula Control Center brings system monitoring, diagnostics, process management and useful system information together in one desktop application.

## Features

- **Overview** — CPU, memory, GPU information where discoverable, storage, network, battery and uptime
- **Processes** — inspect process resource use and request a normal `SIGTERM`
- **Administrator process termination** — when Linux denies access, ask the desktop's polkit agent to authenticate; the helper only sends `SIGTERM` and verifies the selected process identity to guard against PID reuse
- **Hardware** — CPU, GPU, kernel, temperatures and system details where exposed by the kernel
- **Services** — service listing for systemd, OpenRC or runit when their commands are installed and working
- **Storage** — mounted filesystems and disk use
- **Network** — network interfaces, addresses and traffic counters
- **Power** — battery and system load information
- **System Doctor** — quick checks for common Linux interfaces and tools
- **Startup** — desktop autostart entries
- **Gaming Mode** — detects GameMode, MangoHud and `powerprofilesctl`; power-profile actions depend on hardware and system support
- **Permissions** — reports available system and authentication interfaces
- **Command Console** — predefined read-only diagnostics; it does not execute arbitrary user-entered shell commands
- **Themes & Appearance** — distro-inspired accents and interface shapes
- **Localization** — language files with English fallback

The experimental plugin system is removed. NCC does not load or execute third-party plugins.

## Compatibility and portability

NCC is a **Linux desktop application** written in C with GTK4. It is intended to build from source on distributions that provide a C compiler, `make`, `pkg-config`/`pkgconf`, GTK4 development headers, and the Linux `/proc` and `/sys` interfaces. There is no single binary that can be guaranteed to run on every distribution: use your distribution's packages and build locally so the binary links against the libraries available on that system.

The dependency helper recognizes common package-manager families:

- **Debian / Ubuntu / Linux Mint:** APT
- **Fedora:** DNF
- **Arch Linux / EndeavourOS and derivatives:** pacman
- **openSUSE:** Zypper
- **Void Linux:** XBPS
- **Gentoo:** Portage
- **Alpine Linux:** APK

This is broad source-build support, not a claim that every version of every distribution has been tested. GUI availability, graphics backends, polkit authentication, service managers and power-profile controls differ between machines. Core monitoring mainly uses Linux `/proc` and `/sys`; integration features are detected where possible and may be unavailable.

### Build dependencies

- C compiler (`gcc` or compatible)
- `make`
- `pkg-config` or `pkgconf` providing the `pkg-config` command
- GTK4 development files
- `pciutils` (optional; provides `lspci` for additional GPU information)

Preview the dependency command without installing anything:

```bash
./install-deps.sh --dry-run
```

Install the recognized build dependencies after reviewing the command:

```bash
./install-deps.sh
```

The script asks for confirmation before invoking the package manager. It does not run a general system upgrade or automatically refresh APT's package index. If the required packages cannot be found, refresh your distribution's package metadata using its normal recommended procedure and retry.

Then build one task at a time (useful on lower-powered computers), run the safety test, and launch:

```bash
nice -n 10 make -j1
make test
./nebula-control-center
```

A GitHub Actions build check is configured for Debian, Fedora, Arch Linux and Alpine Linux. It checks compilation and the focused privileged-helper safety test; it does **not** claim to test every Linux distribution or every desktop session.

### Install system-wide

```bash
sudo make install
```

The system installation places the app in `/usr/local/bin`, its restricted process-termination helper in `/usr/lib/nebula-control-center`, locale files under `/usr/local/share/nebula-control-center/locales`, and a polkit action in `/usr/share/polkit-1/actions`.

Administrator-authenticated process termination requires `pkexec` and a graphical polkit authentication agent installed by the desktop environment. The application never reads or stores the administrator password. If those components are unavailable, normal same-user process termination still works and protected-process termination reports why authentication is unavailable. Do **not** run the entire GUI as root.

The privileged helper uses Linux `pidfd_open` and `pidfd_send_signal`
to avoid PID-reuse races. This normally requires Linux kernel 5.3 or newer
and build headers that expose the pidfd system-call numbers. If pidfd support
is missing, the helper refuses authenticated termination rather than falling
back to PID-only signaling. Same-user process termination remains subject to
normal Linux permissions.

### Optional integrations

- Service listings use systemd when available, otherwise OpenRC or runit tools when detected.
- Gaming power-profile actions require `powerprofilesctl` and a profile supported by the current machine. NCC does not pretend a hardware-unsupported mode switch succeeded.
- GPU, temperature, battery and network details vary by hardware, kernel drivers, permissions and virtualized environments.

## Project structure

```text
src/             C sources and headers
locales/         translations
icons/           application icon sizes
assets/          project preview imagery, when present
data/            polkit action template
Makefile         build, install and uninstall rules
install-deps.sh  package-manager-aware dependency helper
```

## Development status

**Version: 2.2.0 — cross-distribution preview.** NCC is under active development. It has not been tested on every Linux distribution or service manager. Please report the distribution, desktop environment and relevant logs with bugs.

## License

GNU GPL version 3 or later. See [`LICENSE`](LICENSE).

## Nebula Project

Open-source utilities for Linux and other platforms. **Built to explore.**
