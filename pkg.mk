.ONESHELL: # shell-like behaviour. Reference global variables via ${varname} and local make variables via $${varname}
SHELL := /bin/bash

ROOT=$(shell pwd)/
PKG=${ROOT}/pkg/
BUILD=${PKG}/build

include version.mk

VERSION?=$(YEETMOUSE_VERSION)
PKG_NAME=yeetmouse-${VERSION}
SRC_NAME=${PKG_NAME}.tar.xz

pkg_clean:
	@rm -rf ${BUILD}/*

# Prepare a clean source package (for releases on github or for pkg creation)
pkg_release:
	@
	mkdir -p ${BUILD}
	tar --exclude-vcs --exclude-vcs-ignores --transform='s,^,${PKG_NAME}/,' -cJf ${BUILD}/${SRC_NAME} -C "${ROOT}" .

# Arch package
pkg_arch: pkg_release
	@
	mkdir -p ${BUILD}/arch

	ln -fs ../${SRC_NAME} ${BUILD}/arch/${SRC_NAME}
	
#   Generate PKGBUILD for Arch based systems
	HASH=$(shell sha256sum "${BUILD}/${SRC_NAME}" | awk '{ print $$1 }')
	
	cp -f "${PKG}/arch/PKGBUILD.template" 	"${BUILD}/arch/PKGBUILD"
	sed -i 's|'__VERSION__'|'${VERSION}'|'	"${BUILD}/arch/PKGBUILD"
	sed -i 's|'__SRC__'|'${SRC_NAME}'|' 	"${BUILD}/arch/PKGBUILD"
	sed -i 's|'__HASH__'|'$${HASH}'|' 		"${BUILD}/arch/PKGBUILD"

#   Copy .install files
	cp -f ${PKG}/arch/*.install "${BUILD}/arch/"

#   And finally build the package
	makepkg -f -D ${BUILD}/arch/

# TBD: Debian package
# pkg_deb: pkg_release