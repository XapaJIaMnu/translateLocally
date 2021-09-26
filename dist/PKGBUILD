# Maintainer: Nikolay Bogoychev <nheart@gmail.com>

pkgname=translatelocally-git
pkgver=r002.e81917c
pkgrel=1
pkgdesc='A fast privacy focused machine translation client that translates on your own machine.'
arch=('x86_64')
url='https://github.com/XapaJIaMnu/translateLocally'
license=('MIT')
depends=('qt5-base' 'qt5-svg' 'pcre2' 'libarchive' 'protobuf')
makedepends=('git' 'cmake' 'qt5-tools' 'gcc-libs' 'make' 'binutils')
source=("git+$url.git")
sha256sums=('SKIP')

pkgver() {
  cd translateLocally
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
  # No tags, do this when we have tags
  # git describe --long --tags | sed 's/^foo-//;s/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd translateLocally
  mkdir -p build
  cd build
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  make
}

package() {
  cd translateLocally/build
  install -Dm644 ../LICENCE.md "$pkgdir/usr/share/licenses/$pkgname/LICENCE"
  make DESTDIR="$pkgdir" install
}
