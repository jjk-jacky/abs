# Contributor: Lukas Braun <koomi+aur at hackerspace-bamberg dot de>
# Contributor: David Arroyo <droyo@aqwari.us>
_pkgname=s6
pkgname=$_pkgname-musl-git
pkgver=v2.5.0.0
pkgrel=1
pkgdesc="skarnet's small supervision suite [static-musl; GIT]"
arch=('i686' 'x86_64')
url="http://www.skarnet.org/software/s6"
license=('custom:ISC')
makedepends=('skalibs-musl' 'execline-musl' 'musl')
options=('staticlibs')
source=("git://git.skarnet.org/s6"
         0001-s6-log-Add-p-directive-for-prefix.patch)
sha256sums=('SKIP'
            'ec8da225b41aeebd2a905c71d29c486593664f7380c1ec0f77416f7ad5b7ca9a')
provides=($_pkgname $_pkgname-musl)
conflicts=($_pkgname $_pkgname-musl)

pkgver() {
    cd $_pkgname
    git describe --abbrev=4 --dirty | tr - .
}

prepare() {
    cd $_pkgname
    patch -p1 -i ../0001-s6-log-Add-p-directive-for-prefix.patch
}

build() {
  cd $_pkgname

  export CPPFLAGS='-nostdinc -isystem /usr/lib/musl/include -isystem /usr/include'
  export CC="musl-gcc"
  ./configure --enable-static-libc --bindir=/usr/bin --sbindir=/usr/bin --libexecdir=/usr/lib/s6
  make
}

package() {
  cd $_pkgname

  make DESTDIR="$pkgdir/" install
  install -D -m644 COPYING "$pkgdir/usr/share/licenses/$_pkgname/LICENSE"
}
