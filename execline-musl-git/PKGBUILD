# Contributor: Lukas Braun <koomi+aur at hackerspace-bamberg dot de>
# Contributor: David Arroyo <droyo@aqwari.us>
_pkgname=execline
pkgname=$_pkgname-musl-git
pkgver=v2.1.4.2
pkgrel=1
pkgdesc="A (non-interactive) scripting language, like sh. [static-musl; GIT]"
arch=('i686' 'x86_64')
url="http://www.skarnet.org/software/execline"
license=('custom:ISC')
makedepends=('skalibs-musl' 'musl')
options=('staticlibs')
source=("git://git.skarnet.org/execline")
sha256sums=('SKIP')
provides=($_pkgname $_pkgname-musl)
conflicts=($_pkgname $_pkgname-musl)

pkgver() {
    cd $_pkgname
    git describe --abbrev=4 --dirty | tr - .
}

build() {
    cd $_pkgname

    export CPPFLAGS='-nostdinc -isystem /usr/lib/musl/include -isystem /usr/include'
    export CC="musl-gcc"
    ./configure --enable-static-libc --bindir=/usr/bin --with-include=/usr/include
    make
}

package() {
    cd $_pkgname

    make DESTDIR="$pkgdir/" install
    install -D -m644 COPYING "$pkgdir/usr/share/licenses/$_pkgname/LICENSE"
}
