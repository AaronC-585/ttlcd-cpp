# Maintainer: Your Name <you@example.com>
pkgname=ttlcd
pkgver=1.0.0
pkgrel=1
pkgdesc="TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels"
arch=('x86_64')
url="https://github.com/YOURUSERNAME/ttlcd"
license=('MIT')
depends=('libusb' 'opencv' 'freetype2')
makedepends=('gcc' 'pkg-config' 'make')
source=("$pkgname-$pkgver.tar.gz::https://github.com/YOURUSERNAME/ttlcd/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$pkgname-$pkgver"
    make -j$(nproc)
}

package() {
    cd "$pkgname-$pkgver"
    install -Dm755 ttlcd "$pkgdir/usr/bin/ttlcd"
}
