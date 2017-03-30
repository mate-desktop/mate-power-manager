AC_PREREQ(2.63)

AC_INIT([mate-power-manager], [1.19.0], [http://www.mate-desktop.org/])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz check-news tar-ustar])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_INSTALL
LT_INIT
IT_PROG_INTLTOOL([0.50.1])

MATE_COMPILE_WARNINGS
YELP_HELP_INIT

# GLib min/max required versions
AC_DEFINE([GLIB_VERSION_MAX_ALLOWED], [GLIB_VERSION_2_36],
	[Warn on use of APIs added after GLib 2.36])
AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_36],
	[Warn on use of APIs deprecated before GLib 2.36])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AC_ARG_ENABLE(deprecated,
        [AS_HELP_STRING([--enable-deprecated],
                [Warn about deprecated usages [default=no]])],,
         [enable_deprecated=no])

if test "x$enable_deprecated" = "xyes"; then
	DISABLE_DEPRECATED=""
	CPPFLAGS="$CPPFLAGS $DISABLE_DEPRECATED"
fi

AC_ARG_ENABLE(extra-warn,
        [AS_HELP_STRING([--enable-extra-warn],
                [Enable extra warnings. gcc only. [default=no]])],,
         [enable_extra_warn=no])

if test "x$enable_extra_warn" = "xyes" && test "$GCC" = "yes"; then
	WARNINGFLAGS="$WARNINGFLAGS -Wall"
	WARNINGFLAGS="$WARNINGFLAGS -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-declarations"
#	WARNINGFLAGS="$WARNINGFLAGS -Wredundant-decls"
#	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-noreturn"
	WARNINGFLAGS="$WARNINGFLAGS -Wpointer-arith"
	WARNINGFLAGS="$WARNINGFLAGS -Wcast-align"
	WARNINGFLAGS="$WARNINGFLAGS -Wwrite-strings"
	WARNINGFLAGS="$WARNINGFLAGS -Winit-self"
	WARNINGFLAGS="$WARNINGFLAGS -Wreturn-type"
	WARNINGFLAGS="$WARNINGFLAGS -Wformat-nonliteral"
	WARNINGFLAGS="$WARNINGFLAGS -Wformat-security"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-include-dirs"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-format-attribute"
	WARNINGFLAGS="$WARNINGFLAGS -Wclobbered"
	WARNINGFLAGS="$WARNINGFLAGS -Wempty-body"
	WARNINGFLAGS="$WARNINGFLAGS -Wignored-qualifiers"
#	WARNINGFLAGS="$WARNINGFLAGS -Wsign-compare"
	WARNINGFLAGS="$WARNINGFLAGS -Wtype-limits"
#	WARNINGFLAGS="$WARNINGFLAGS -Wuninitialized"
#	WARNINGFLAGS="$WARNINGFLAGS -Waggregate-return"
	WARNINGFLAGS="$WARNINGFLAGS -Wdeclaration-after-statement"
	WARNINGFLAGS="$WARNINGFLAGS -Wshadow"
	WARNINGFLAGS="$WARNINGFLAGS -Winline"
#	WARNINGFLAGS="$WARNINGFLAGS -Wswitch-enum"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-parameter-type"
	WARNINGFLAGS="$WARNINGFLAGS -Woverride-init"
	WARNINGFLAGS="$WARNINGFLAGS -Wno-strict-aliasing"
else
	WARNINGFLAGS=""
fi
AC_SUBST(WARNINGFLAGS)

dnl ---------------------------------------------------------------------------
dnl - Debugging switches (uncomment this if you want to use gdb)
dnl ---------------------------------------------------------------------------
CPPFLAGS="$CPPFLAGS -g"
CPPFLAGS="$CPPFLAGS -fexceptions"

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

AM_GLIB_GNU_GETTEXT
GLIB_GSETTINGS

dnl ----------------------------------------------------------------------------
dnl - Library dependencies
dnl ----------------------------------------------------------------------------
GLIB_REQUIRED=2.36.0
GIO_REQUIRED=2.36.0
GDK_REQUIRED=3.14.0
GTK_REQUIRED=3.14.0
GNOME_KEYRING_REQUIRED=3.0.0
DBUS_REQUIRED=1.0
DBUS_GLIB_REQUIRED=0.70
LIBNOTIFY_REQUIRED=0.7.0
CAIRO_REQUIRED=1.0.0
LIBPANEL_REQUIRED=1.17.0
XRANDR_REQUIRED=1.3.0
XPROTO_REQUIRED=7.0.15
CANBERRA_REQUIRED=0.10
UPOWER_REQUIRED=0.9.5

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED gobject-2.0 gio-2.0 >= $GIO_REQUIRED gio-unix-2.0)

PKG_CHECK_MODULES(CANBERRA, libcanberra-gtk3 >= $CANBERRA_REQUIRED)

PKG_CHECK_MODULES(DBUS,[
 dbus-glib-1 >= $DBUS_GLIB_REQUIRED
 dbus-1 >= $DBUS_REQUIRED
 gthread-2.0])

PKG_CHECK_MODULES(CAIRO, [
 gtk+-3.0 >= $GTK_REQUIRED
 cairo >= $CAIRO_REQUIRED])

PKG_CHECK_MODULES(GDK, [
 gdk-3.0 >= $GDK_REQUIRED
 gdk-x11-3.0 >= $GDK_REQUIRED])

PKG_CHECK_MODULES(X11, [
 xrandr >= $XRANDR_REQUIRED
 x11 xext xproto >= $XPROTO_REQUIRED])

PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED)

PKG_CHECK_MODULES(UPOWER, upower-glib >= $UPOWER_REQUIRED)

dnl ---------------------------------------------------------------------------
dnl - Make paths available for source files
dnl ---------------------------------------------------------------------------
AC_SUBST(SYSCONFDIR, $sysconfdir)
AC_SUBST(DATADIR, $datadir)
AC_SUBST(BINDIR, $bindir)
AC_SUBST(SBINDIR, $sbindir)

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests,
              AS_HELP_STRING([--enable-tests],
                             [Build self tests]),
              enable_tests=$enableval,enable_tests=no)

AC_MSG_CHECKING([whether to support tests])
have_tests=no
if test x$enable_tests = xyes ; then
    have_tests=yes
    AC_DEFINE(HAVE_TESTS, 1, [Define if we want to use the self tests])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_TESTS], [test $have_tests = yes])

dnl ---------------------------------------------------------------------------
dnl - Build gnome-keyring support
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(keyring,
        [AS_HELP_STRING([--without-keyring],
                        [Disable the use of gnome-keyring])],
        [],
        [with_keyring=yes])

AM_CONDITIONAL([WITH_KEYRING],[test "$with_keyring" = "yes"])

if test "$with_keyring" = "yes"; then
        PKG_CHECK_MODULES(KEYRING, gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED)
        AC_DEFINE([WITH_KEYRING],[1],[Define if KEYRING support is enabled])
fi

dnl ---------------------------------------------------------------------------
dnl - Build applets
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(applets,
              AS_HELP_STRING([--disable-applets],
                             [Build the extra power applets]),
              enable_applets=$enableval,enable_applets=yes)

AC_MSG_CHECKING([whether to build applet support])
have_applets=no
if test x$enable_applets = xyes ; then
    have_applets=yes

    PKG_CHECK_MODULES(PANEL, [libmatepanelapplet-4.0 >= $LIBPANEL_REQUIRED])
    AC_DEFINE(HAVE_APPLETS, 1, [Define if we are building applets])
fi
AC_MSG_RESULT([$have_applets])
AM_CONDITIONAL([HAVE_APPLETS], [test $have_applets = yes])

dnl ---------------------------------------------------------------------------
dnl - Are we specifying a different dbus root ?
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(dbus-services,
            AS_HELP_STRING([--with-dbus-services=<dir>],
                           [where D-BUS services directory is]),
            [DBUS_SERVICES_DIR="$with_dbus_services"],
            [DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"])
AC_SUBST(DBUS_SERVICES_DIR)

dnl ---------------------------------------------------------------------------
dnl - Check for Solaris kstat support
dnl ---------------------------------------------------------------------------
AC_MSG_CHECKING(for Solaris kstat)
AC_CHECK_LIB(kstat, kstat_open,
	     [
	       AC_DEFINE(HAVE_KSTAT, 1, [Define if Solaris kstat is support])
	       GPM_EXTRA_LIBS="$GPM_EXTRA_LIBS -lkstat"
	     ])
AC_SUBST(GPM_EXTRA_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
src/Makefile
applets/Makefile
applets/brightness/Makefile
applets/inhibit/Makefile
help/Makefile
po/Makefile.in
policy/Makefile
data/Makefile
data/mate-power-manager.desktop.in
data/mate-power-preferences.desktop.in
data/mate-power-statistics.desktop.in
data/org.mate.power-manager.gschema.xml
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/actions/Makefile
data/icons/16x16/apps/Makefile
data/icons/16x16/status/Makefile
data/icons/22x22/Makefile
data/icons/22x22/actions/Makefile
data/icons/22x22/apps/Makefile
data/icons/22x22/status/Makefile
data/icons/24x24/Makefile
data/icons/24x24/actions/Makefile
data/icons/24x24/apps/Makefile
data/icons/24x24/status/Makefile
data/icons/32x32/Makefile
data/icons/32x32/actions/Makefile
data/icons/32x32/apps/Makefile
data/icons/32x32/status/Makefile
data/icons/48x48/Makefile
data/icons/48x48/actions/Makefile
data/icons/48x48/apps/Makefile
data/icons/48x48/status/Makefile
data/icons/scalable/Makefile
data/icons/scalable/actions/Makefile
data/icons/scalable/apps/Makefile
data/icons/scalable/status/Makefile
])

AC_OUTPUT

dnl ==========================================================================
echo "
                    MATE Power Manager $VERSION
                  =============================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        gnome-keyring support:     ${with_keyring}
        Building extra applets:    ${enable_applets}
        Self test support:         ${have_tests}
        dbus-1 services dir:       $DBUS_SERVICES_DIR
"
if [[ "${prefix}" != "/usr" ]] ; then
	echo '
WARNING!!!  MATE Power Manager uses the "pkexec" utility to provide root
permissions necessary for the "mate-power-backlight-helper" executable to run.

A link should be provided from the file
"/usr/share/polkit-1/actions/org.mate.power.policy" to the installed version
"${prefix}/share/polkit-1/actions/org.mate.power.policy" after installation.
'
fi

