#!/bin/sh

ROOT="$(git rev-parse --show-toplevel)"

# Read current version
VERSION=$(cat "${ROOT}/Makefile" | grep -oP "(?<=DKMS_VER\?=)[0-9\.]+")
SRC_NAME=leetmouse-$VERSION

SRC=$SRC_NAME.tar.xz

# Clear the build folder from old releases
rm -rf $ROOT/pkg/build/

# Create new package file
. $ROOT/scripts/build_pkg.sh

# Build package work directory
WORKDIR=${ROOT}/pkg/build/${SRC_NAME}_$(dpkg --print-architecture)
mkdir -p ${WORKDIR}/DEBIAN

# ########## Generate control file for Debian based systems
cp -f "${ROOT}/pkg/control.template" "${WORKDIR}/DEBIAN/control"
cp -f ${ROOT}/pkg/postinst ${WORKDIR}/DEBIAN/
cp -f ${ROOT}/pkg/prerm    ${WORKDIR}/DEBIAN/
cp -f ${ROOT}/pkg/postrm   ${WORKDIR}/DEBIAN/
for f in ${WORKDIR}/DEBIAN/*; do
	sed -i 's|'__VERSION__'|'$VERSION'|g' "$f"
done

# Set some permissions
chmod 0775 ${WORKDIR}/DEBIAN/*

DESTDIR=${WORKDIR}

cd ${ROOT} && make DESTDIR="${WORKDIR}" setup_dkms udev_install

dpkg-deb --build --root-owner-group ${WORKDIR}
