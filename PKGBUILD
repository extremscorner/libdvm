# Maintainer: Extrems <extrems@extremscorner.org>

_pkgname=libdvm
pkgname="libogc2-${_pkgname}-git"
pkgver=r54.b6cad64
pkgrel=1
pkgdesc='FAT and disk/volume management library for GameCube and Wii.'
arch=('any')
url='https://github.com/extremscorner/libdvm'
license=('ZPL-2.1')
groups=('gamecube-dev' 'wii-dev')
depends=('devkitPPC>=r42' 'libogc2')
makedepends=('catnip' 'cmake' 'git' 'libogc2-cmake' 'ninja')
provides=("libogc2-${_pkgname}" 'libogc2-libfat')
conflicts=("libogc2-${_pkgname}" 'libogc2-libfat')
options=(!strip libtool staticlibs !buildflags)

pkgver() {
	cd "${startdir}"
	printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short=7 HEAD)"
}

build() {
	cd "${startdir}"
	catnip build gamecube wii
}

package() {
	cd "${startdir}"
	DESTDIR="${pkgdir}" catnip install gamecube wii
	for platform in gamecube wii; do
		install -Dm 644 COPYING -t "${pkgdir}${DEVKITPRO}/libogc2/${platform}/share/licenses/${_pkgname}"
	done
}
