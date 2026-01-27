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
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	dbus-glib
	file
	gcc
	git
	itstool
	libcanberra
	libsecret
	libgnome-keyring
	libnotify
	make
	mate-common
	mate-panel
	python
	upower
	which
	yelp-tools
	iso-codes
	gobject-introspection
	dconf
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
