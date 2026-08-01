#!/bin/bash
# Build QtRtfEditor package without polluting the source tree.
# Creates an isolated build directory under /tmp/ and runs makepkg there.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL=0
if [[ "${1:-}" == "--install" ]]; then
  INSTALL=1
  shift
fi
BUILD_DIR="${1:-/tmp/qt-rtf-editor-build}"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy PKGBUILD and patch repo_root and pkgver so prepare() can find
# the source tree and use the project version from CMakeLists.txt.
_pkgver=$(grep 'VERSION' "${SCRIPT_DIR}/CMakeLists.txt" | grep -v 'cmake_minimum_required' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)
sed -e "s|repo_root=\"\$(dirname \"\${BASH_SOURCE\[0\]}\")\"|repo_root='${SCRIPT_DIR}'|" \
    -e "/^pkgrel=/a pkgver=${_pkgver}" \
    "${SCRIPT_DIR}/PKGBUILD" > "${BUILD_DIR}/PKGBUILD"

# Run makepkg in the clean directory.
# All build artifacts (src/, pkg/, archive.tar.gz) stay in BUILD_DIR.
(cd "$BUILD_DIR" && makepkg --noconfirm)

if [[ "$INSTALL" -eq 1 ]]; then
  PKG_FILE=$(ls "$BUILD_DIR"/qt-rtf-editor-*.pkg.tar.zst | head -n 1)
  sudo pacman -U --noconfirm "$PKG_FILE"
fi
