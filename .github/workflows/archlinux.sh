#!/usr/bin/bash

set -eo pipefail

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

# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-power-manager
requires+=(
	autoconf-archive
	dbus-glib
	gcc
	gettext
	git
	glib2-devel
	gobject-introspection
	itstool
	libcanberra
	libnotify
	libsecret
	make
	mate-common
	mate-panel
	polkit
	python
	upower
	which
    xorg-server-xvfb
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
