# Nebula Control Center

**A native Linux system control center built with C and GTK4.**

Nebula Control Center (NCC) brings system monitoring, diagnostics, process management and common Linux controls into one desktop application.

> **Version 2.2.0 — public preview.** NCC targets multiple Linux distributions and builds from source. It is not a universal binary, and compatibility with every distribution, desktop environment or kernel is not guaranteed.

## Features

- **Overview** — CPU, memory, GPU information when discoverable, storage, network, battery and uptime
- **Processes** — inspect resource use and request process termination
- **Hardware** — CPU, GPU, kernel, temperatures and other details exposed by the system
- **Services** — service information for systemd, OpenRC or runit when their tools are available
- **Storage** — mounted filesystems and disk usage
- **Network** — interfaces, addresses and traffic counters
- **Power** — battery and system-load information
- **System Doctor** — checks for common Linux interfaces and optional tools
- **Startup** — desktop autostart entries
- **Gaming Mode** — detects GameMode, MangoHud and `powerprofilesctl`; available actions depend on system support
- **Permissions** — reports available system and authentication interfaces
- **Command Console** — predefined read-only diagnostics; it does not run arbitrary commands typed by the user
- **Themes & Appearance** — distro-inspired accent themes and interface shapes
- **Localization** — external language files with an English fallback

The experimental third-party plugin system has been removed. NCC does not load or execute third-party plugins.

## Linux compatibility

NCC is designed for **Linux desktop systems with GTK4**. It uses standard C, GTK4 and Linux interfaces such as `/proc` and `/sys`. It can be built locally against the libraries available on the target distribution.

There is no single binary that is guaranteed to work everywhere. Distribution releases differ in library versions, graphics backends, service managers, authentication agents, filesystem layout and kernel features.

### Automated build targets

The repository's GitHub Actions workflow is configured to compile and run the focused helper safety tests in containers based on:

| CI target | Package family |
| --- | --- |
| Debian stable | APT / Debian packages |
| Fedora | DNF / RPM packages |
| Arch Linux | pacman packages |
| Alpine Linux | APK packages |

A CI target is a build check, not a guarantee for every desktop session, hardware configuration or release of that distribution. Check the repository's **Actions** tab for the current run results before treating a target as verified.

### Dependency helper support

`install-deps.sh` recognizes these package-manager families and offers to install the build dependencies:

| Distribution family | Package manager | Status in dependency helper |
| --- | --- | --- |
| Debian, Ubuntu, Linux Mint and derivatives | APT | Recognized |
| Fedora and derivatives | DNF | Recognized |
| Arch Linux, EndeavourOS and derivatives | pacman | Recognized |
| openSUSE | Zypper | Recognized |
| Void Linux | XBPS | Recognized |
| Gentoo | Portage | Recognized |
| Alpine Linux | APK | Recognized |
| Other Linux distributions | Varies | Install dependencies manually |

“Recognized” means the helper knows a package-manager command; it does not mean that distribution has passed CI. Windows, macOS and BSD are not supported targets.

## Build from source

### 1. Get the source

```bash
git clone https://github.com/a43039646-svg/Nebula-Control-Center.git
cd Nebula-Control-Center
```

### 2. Preview or install build dependencies

The build needs a C compiler, `make`, `pkg-config`/`pkgconf`, GTK4 development files and (optionally) `pciutils` for extra GPU information.

First preview what the helper would run:

```bash
./install-deps.sh --dry-run
```

Review the command. To let the helper ask for confirmation and install the recognized dependencies, run:

```bash
./install-deps.sh
```

The helper does not request a general system upgrade and does not automatically refresh APT's package index. If a package cannot be found, refresh your distribution's package metadata using its normal procedure, then retry. You may also install the packages manually using the examples below.

### Package examples

These are build-dependency examples for common package managers. Package names can vary by distribution release and enabled repositories.

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

Follow Gentoo's usual Portage workflow and review any USE-flag or configuration prompts before confirming.

**Alpine Linux**

```bash
sudo apk add build-base pkgconf gtk4.0-dev pciutils
```

For another distribution, install its equivalent compiler, `make`, `pkg-config`/`pkgconf`, and GTK4 development packages. `pciutils` is optional.

### 3. Build, test and run

To reduce CPU contention on a lower-powered machine, compile one job at a time:

```bash
nice -n 10 make -j1
make test
./nebula-control-center
```

If `make` reports that GTK4 development files are missing, check that this succeeds:

```bash
pkg-config --modversion gtk4
```

The helper safety test checks invalid input, mismatched process identity and a normal signal to a temporary test process. Do not use the test as a substitute for reviewing CI results.

## Optional system-wide installation

You can run NCC from the build directory without installing it. In that mode, administrator-authenticated process termination is not available unless the privileged helper is installed at its configured system path.

To install system-wide:

```bash
sudo make install
```

The default Makefile paths are FHS-style locations:

- Application: `/usr/local/bin/nebula-control-center`
- Privileged helper: `/usr/lib/nebula-control-center/nebula-process-terminator`
- Language files: `/usr/local/share/nebula-control-center/locales/`
- Desktop entry: `/usr/local/share/applications/`
- polkit action: `/usr/share/polkit-1/actions/`

These defaults are suitable for many conventional Linux distributions. Systems with a different filesystem layout may need Makefile path overrides or packaging work; do not assume the default install paths are correct for every immutable or non-FHS distribution.

To uninstall an installation made with the same prefix and paths, return to the source directory and run:

```bash
sudo make uninstall
```

Use the same Makefile path overrides during uninstall if you changed them during installation.

## Process-termination security and limitations

Normal same-user termination follows the permissions enforced by Linux. Terminating protected processes requires `pkexec` and an active graphical polkit authentication agent installed by the desktop environment; the application must not be run as root.

The privileged helper uses Linux pidfd system calls when the build headers expose them, helping it avoid PID-reuse races. This normally requires Linux kernel 5.3 or newer and suitable system-call definitions in the development headers. If pidfd support is unavailable, the helper refuses the authenticated termination request rather than falling back to PID-only signaling.

The helper sends `SIGTERM`; it does not promise that a process will exit immediately or that every process can be terminated. System services and protected processes may require their own management tools.

## Optional integrations and troubleshooting

- **Services not listed:** NCC detects service managers/tools that are installed and accessible. Results differ between systemd, OpenRC, runit and minimal environments.
- **Administrator termination unavailable:** check that `pkexec` and your desktop's polkit authentication agent are installed. The helper is only added to its configured system path by system-wide installation.
- **Missing GPU details:** install `pciutils` if available and remember that hardware/driver information depends on the machine and permissions.
- **Power-profile actions unavailable:** `powerprofilesctl`, compatible hardware and an applicable power profile are required. NCC should not report a mode change as successful if the underlying system has not changed.
- **Build fails on a new distro version:** check `pkg-config --modversion gtk4`, confirm the GTK4 development package is installed, and open an issue with the distribution and release, desktop environment, kernel version and relevant error output.
- **CI failures:** consult the repository's GitHub Actions run; a local successful build on one distribution does not establish portability everywhere.

## Project layout

```text
src/             C sources and headers
locales/         translations
icons/           application icon sizes
.github/         automated build workflow
tests/           focused safety tests
data/            polkit policy template
Makefile         build, install and uninstall rules
install-deps.sh  package-manager-aware dependency helper
CHANGELOG.md     version history
```

## Development status

**Version 2.2.0 — cross-distribution preview.** The project is under active development. The automated matrix checks selected distribution container images; it does not cover every distribution, desktop environment, kernel or hardware configuration. Bug reports and contributions are welcome.

When reporting an issue, include your distribution and release, desktop environment, kernel version, whether you built from source or installed system-wide, and the relevant error output. Do not include access tokens, passwords or other secrets in logs.

## License

GNU General Public License v3.0 or later. See [`LICENSE`](LICENSE).

## Nebula Project

Nebula Project creates open-source software and tools. **Built to explore.**
