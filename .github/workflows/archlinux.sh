#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
	meson  # Used for meson build
)

# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-power-manager/-/blob/main/PKGBUILD
requires+=(
	autoconf-archive
	dbus-glib
	file
	gcc
	git
	glib2-devel
	itstool
	libcanberra
	libgnome-keyring
	libnotify
	libsecret
	make
	mate-common
	mate-panel
	python
	upower
	which
	yelp-tools
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
