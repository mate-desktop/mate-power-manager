#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	cairo-devel
	dbus-glib-devel
	desktop-file-utils
	gcc
	git
	glib2-devel
	gtk3-devel
	libcanberra-devel
	libgnome-keyring-devel
	libnotify-devel
	libsecret-devel
	make
	mate-common
	mate-panel-devel
	mesa-libGL-devel
	polkit-devel
	popt-devel
	redhat-rpm-config
	upower-devel
	iso-codes-devel
	gobject-introspection-devel
	dconf-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
