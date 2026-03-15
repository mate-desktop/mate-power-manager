#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	libcanberra-gtk3-dev
	libdbus-glib-1-dev
	libgcrypt20-dev
	libglib2.0-dev
	libgtk-3-dev
	libmate-panel-applet-dev
	libnotify-dev
	libsecret-1-dev
	libtool-bin
	libupower-glib-dev
	libx11-dev
	libxext-dev
	libxml-parser-perl
	libxrandr-dev
	make
	mate-common
	pkg-config
	xmlto
	yelp-tools
	iso-codes
	gobject-introspection
	libgirepository1.0-dev
	libdconf-dev
    xvfb
)

infobegin "Update system"
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
