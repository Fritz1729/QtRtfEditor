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
source=("${_pkgbasename}-${pkgver}.tar.gz")
sha256sums=('2786eaff8528621fff5cbd2a6e0ad2e778873d7a483494415def675e3be5e095')

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
