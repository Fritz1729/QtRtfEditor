# Maintainer: F1729

pkgname=qt-rtf-editor
_pkgbasename=QtRtfEditor
pkgver=0.1.0
pkgrel=1
pkgdesc='Reusable RTF-capable QTextEdit subclass'
arch=('x86_64' 'aarch64')
url='https://github.com/Fritz1729/QtRtfEditor'
license=('GPL-3.0-only' 'custom')
depends=('qt6-base')
makedepends=('cmake' 'ninja' 'qt6-base')
checkdepends=('qt6-base')
source=("${_pkgbasename}-${pkgver}.tar.gz::https://github.com/Fritz1729/QtRtfEditor/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('b9c3debec5270da96acea293dd9c030d15798e8082c883d3f3473cd44390b6ba')

build() {
  cmake -B build -S "${srcdir}/${_pkgbasename}-${pkgver}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=ON \
    -G Ninja
  cmake --build build
}

check() {
  ctest --test-dir build --output-on-failure
}

package() {
  cmake --install build --prefix "${pkgdir}/usr"
}
