#!/bin/sh
set -eu

DRY_RUN=0
case "${1:-}" in
    --dry-run) DRY_RUN=1 ;;
    --help|-h)
        cat <<'USAGE'
Usage: ./install-deps.sh [--dry-run]

Install the build dependencies for Nebula Control Center using a detected
Linux package manager. The script asks for confirmation before installing.
--dry-run  Print the package-manager command without installing anything.
USAGE
        exit 0
        ;;
    '') ;;
    *) echo "Unknown option: $1" >&2; echo "Use --help for usage." >&2; exit 2 ;;
esac

run_as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "Error: run as root or install sudo, then retry." >&2
        exit 1
    fi
}

install_packages() {
    printf 'Package-manager command:'
    printf ' %s' "$@"
    printf '\n'

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "Dry run: no packages were installed."
        return 0
    fi

    echo "This installs NCC build dependencies; it does not request a full system upgrade."
    printf 'Continue? [y/N] '
    IFS= read -r answer || answer=''
    case "$answer" in
        y|Y|yes|YES|Yes) ;;
        *) echo "Cancelled; no packages were installed."; exit 0 ;;
    esac

    run_as_root "$@"
}

if command -v apt-get >/dev/null 2>&1; then
    install_packages apt-get install -y --no-install-recommends build-essential make pkg-config libgtk-4-dev pciutils
elif command -v dnf >/dev/null 2>&1; then
    install_packages dnf install -y --setopt=install_weak_deps=False gcc make pkgconf-pkg-config gtk4-devel pciutils
elif command -v pacman >/dev/null 2>&1; then
    install_packages pacman -S --needed --noconfirm base-devel pkgconf gtk4 pciutils
elif command -v zypper >/dev/null 2>&1; then
    install_packages zypper --non-interactive --no-recommends install gcc make pkg-config gtk4-devel pciutils
elif command -v xbps-install >/dev/null 2>&1; then
    install_packages xbps-install -S -y gcc make pkgconf gtk4-devel pciutils
elif command -v emerge >/dev/null 2>&1; then
    install_packages emerge --ask=n dev-util/pkgconf gui-libs/gtk:4 sys-devel/gcc sys-devel/make sys-apps/pciutils
elif command -v apk >/dev/null 2>&1; then
    install_packages apk add --no-cache build-base pkgconf gtk4.0-dev pciutils
else
    echo "Unsupported package manager." >&2
    echo "Install a C compiler, make, pkg-config/pkgconf, GTK4 development files and optionally pciutils manually." >&2
    exit 1
fi
