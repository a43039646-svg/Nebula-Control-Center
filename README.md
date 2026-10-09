# Nebula Control Center

**A native Linux system control center built with C and GTK4.**

Nebula Control Center (NCC) combines system monitoring, diagnostics, process management and common Linux controls in one desktop application.

> **Version 2.2.0 — public preview.** NCC is built from source for the target Linux distribution. It is not a universal binary, and compatibility with every distribution, desktop environment, kernel, device or release is not guaranteed.

## Features

- **Overview** — CPU, memory, GPU information when discoverable, storage, network, battery and uptime
- **Processes** — inspect process resource use and request process termination
- **Hardware** — CPU, GPU, kernel, temperatures and other information exposed by the system
- **Services** — service information for systemd, OpenRC or runit when the relevant tools are available
- **Storage** — mounted filesystems and disk usage
- **Network** — interfaces, addresses and traffic counters
- **Power** — battery and system-load information
- **System Doctor** — checks for common Linux interfaces and optional tools
- **Startup** — desktop autostart entries
- **Gaming Mode** — detects GameMode, MangoHud and `powerprofilesctl`; available actions depend on system support
- **Permissions** — reports available system and authentication interfaces
- **Command Console** — predefined read-only diagnostics; arbitrary user-entered shell commands are not executed
- **Themes & Appearance** — distro-inspired accent themes and interface shapes
- **Localization** — external language files with an English fallback

The experimental third-party plugin system has been removed. NCC does not load or execute third-party plugins.

## Linux compatibility

NCC targets Linux desktop systems that provide GTK4 and standard Linux interfaces such as `/proc` and `/sys`. Compile it on the target distribution so it links against the libraries available on that system.

Linux distributions differ in GTK versions, graphics backends, service managers, authentication agents, filesystem layout and kernel features. A successful build on one distribution does not guarantee identical functionality on another.

### Automated build targets

GitHub Actions is configured to build the application and run focused privileged-helper safety tests in container images for these targets:

| CI target | Package family |
| --- | --- |
| Debian stable | APT / Debian packages |
| Fedora | DNF / RPM packages |
| Arch Linux | pacman packages |
| Alpine Linux | APK packages |

Check the repository's **Actions** tab for the latest results. These checks do not cover every release, desktop environment, hardware configuration or kernel.

### Supported dependency-helper families

`install-deps.sh` recognizes the package managers below and asks for confirmation before installing the build dependencies:

| Distribution family | Package manager | Dependency helper |
| --- | --- | --- |
| Debian, Ubuntu, Linux Mint and derivatives | APT | Recognized |
| Fedora and derivatives | DNF | Recognized |
| Arch Linux, EndeavourOS and derivatives | pacman | Recognized |
| openSUSE | Zypper | Recognized |
| Void Linux | XBPS | Recognized |
| Gentoo | Portage | Recognized |
| Alpine Linux | APK | Recognized |
| Other distributions | Varies | Install dependencies manually |

“Recognized” means that the helper knows the package-manager command; it does **not** mean that distribution has passed CI. Windows, macOS and BSD are not supported targets.

## Build from source

### 1. Get the source

```bash
git clone https://github.com/a43039646-svg/Nebula-Control-Center.git
cd Nebula-Control-Center
```

### 2. Preview or install dependencies

Required build dependencies:

- a C compiler (`gcc` or compatible)
- `make`
- `pkg-config` or `pkgconf` providing the `pkg-config` command
- GTK4 development headers and libraries
- `pciutils` (optional; provides `lspci` for additional GPU information)

Preview the dependency command first; this does not install packages:

```bash
./install-deps.sh --dry-run
```

To let the helper ask for confirmation and install the recognized dependencies:

```bash
./install-deps.sh
```

The helper does not request a general system upgrade and does not automatically refresh APT's package index. If packages cannot be found, refresh your distribution's package metadata using its normal procedure, then retry. You can also install dependencies manually using the examples below.

### Package examples

Package names can vary between releases and enabled repositories. These commands install build dependencies only.

**Debian / Ubuntu / Linux Mint**

```bash
sudo apt-get install build-essential make pkg-config libgtk-4-dev pciutils
```

**Fedora**

```bash
sudo dnf install gcc make pkgconf-pkg-config gtk4-devel pciutils
```

**Arch Linux / EndeavourOS**

```bash
sudo pacman -S --needed base-devel pkgconf gtk4 pciutils
```

**openSUSE**

```bash
sudo zypper install gcc make pkg-config gtk4-devel pciutils
```

**Void Linux**

```bash
sudo xbps-install -S gcc make pkgconf gtk4-devel pciutils
```

**Gentoo**

```bash
sudo emerge --ask dev-util/pkgconf gui-libs/gtk:4 sys-devel/gcc sys-devel/make sys-apps/pciutils
```

Follow Gentoo's normal Portage workflow and review configuration or USE-flag prompts before confirming.

**Alpine Linux**

```bash
sudo apk add build-base pkgconf gtk4.0-dev pciutils
```

For another distribution, install its equivalent compiler, `make`, `pkg-config`/`pkgconf` and GTK4 development package. `pciutils` is optional.

### 3. Build, test and run

To limit simultaneous compiler jobs on lower-powered machines:

```bash
nice -n 10 make -j1
make test
./nebula-control-center
```

The test target builds/runs the focused privileged-helper test script. It checks invalid input, mismatched process identity and sending `SIGTERM` to a temporary test process; it is not a substitute for reviewing CI results.

If `make` reports that GTK4 development files are missing, check:

```bash
pkg-config --modversion gtk4
```

If that command fails, install the GTK4 development package for your distribution and try again.

## Optional system-wide installation

You can run NCC directly from the build directory without installing it. In that mode, authenticated termination of protected processes is unavailable unless the privileged helper is installed at the configured system path.

To install system-wide:

```bash
sudo make install
```

With the default Makefile settings, installation places files at:

- Application: `/usr/local/bin/nebula-control-center`
- Privileged helper: `/usr/lib/nebula-control-center/nebula-process-terminator`
- Language files: `/usr/local/share/nebula-control-center/locales/`
- Desktop entry: `/usr/local/share/applications/org.nebula.ControlCenter.desktop`
- Application icon: `/usr/local/share/icons/hicolor/512x512/apps/nebula-control-center.png`
- polkit action: `/usr/share/polkit-1/actions/`

These paths are conventional defaults, not a guarantee for every immutable or non-FHS distribution. Such systems may need path overrides or native packaging work.

To uninstall an installation made using the same prefix and paths, run from the source directory:

```bash
sudo make uninstall
```

Use the same Makefile path overrides during uninstall if you changed them during installation.

## Process-termination security and limitations

Normal same-user process termination follows the permissions enforced by Linux. Terminating protected processes requires `pkexec` and an active graphical polkit authentication agent provided by the desktop environment. **Do not run the entire GUI as root.** The application does not read or store the administrator password.

The privileged helper uses Linux pidfd system calls when the build headers expose them, which helps avoid PID-reuse races. This normally requires Linux kernel 5.3 or newer and suitable system-call definitions in the development headers. If pidfd support is unavailable, the helper refuses the authenticated termination request rather than falling back to PID-only signaling.

The helper sends `SIGTERM`; it does not guarantee that a process exits immediately or that every process can be terminated. System services may need to be managed through their service manager.

## Optional integrations and troubleshooting

- **Services are missing:** NCC detects service managers and commands available on the system. Results differ between systemd, OpenRC, runit and minimal environments.
- **Administrator termination is unavailable:** check that `pkexec` and your desktop's polkit authentication agent are installed. The privileged helper is placed in its configured system path by `sudo make install`.
- **GPU details are missing:** install `pciutils` if available. Hardware and driver details vary with devices, drivers, permissions and virtualized environments.
- **Power-profile controls are unavailable:** `powerprofilesctl`, a compatible profile and supported hardware may be required.
- **Build fails on a new distro release:** check `pkg-config --modversion gtk4`, verify the GTK4 development package is installed, and report the distribution/release and relevant error output.
- **CI fails:** review the corresponding GitHub Actions run. A successful build on one distro does not prove portability to all Linux environments.

When reporting a bug, include the distribution and release, desktop environment, kernel version, whether NCC was run from the build directory or installed system-wide, and the relevant error output. Do not include passwords, access tokens or other secrets in logs.

## Project layout

```text
src/             C sources and headers
locales/         translations
icons/           application icon sizes
.github/         automated build workflow
tests/           focused helper safety tests
data/            polkit action template
Makefile         build, install and uninstall rules
install-deps.sh  package-manager-aware dependency helper
CHANGELOG.md     version history
```

## Development status

**Version 2.2.0 — cross-distribution public preview.** NCC is under active development. Automated builds cover selected container images; they do not cover every distribution, desktop environment, kernel or hardware configuration. Bug reports and contributions are welcome.

## License

GNU General Public License v3.0 or later. See [`LICENSE`](LICENSE).

## Nebula Project

Nebula Project creates open-source software and tools. **Built to explore.**
