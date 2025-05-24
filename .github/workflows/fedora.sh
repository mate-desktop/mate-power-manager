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

# https://src.fedoraproject.org/rpms/mate-power-manager/blob/rawhide/f/mate-power-manager.spec
requires+=(
	autoconf-archive
	gcc
	git
	make
	mate-common
	mate-panel-devel
	cairo-devel
	dbus-glib-devel
	desktop-file-utils
	glib2-devel
	gtk3-devel
	libcanberra-devel
	libnotify-devel
	libsecret-devel
	make
	mate-common
	mate-desktop-devel
	mate-panel-devel
	mesa-libGL-devel
	polkit-devel
	popt-devel
	upower-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
