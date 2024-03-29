#!/bin/bash

DKMS_VER=$(cat "./Makefile" | grep -oP "(?<=DKMS_VER\?=)[0-9\.]+")

# Desvincular todos os mouses do driver
sudo /usr/lib/udev/leetmouse_manage unbind_all

# Desinstalar o driver
sudo dkms remove -m leetmouse-driver -v $DKMS_VER
sudo make remove_dkms && sudo make udev_uninstall

# Instalar o driver
sudo make setup_dkms && sudo make udev_install
sudo dkms install -m leetmouse-driver -v $DKMS_VER