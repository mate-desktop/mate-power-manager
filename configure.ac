AC_PREREQ(2.63)

AC_INIT([mate-power-manager], [1.28.1], [https://github.com/mate-desktop/mate-power-manager/pulls],
        [mate-power-manager], [https://mate-desktop.org])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz check-news tar-ustar])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

MATE_DEBUG_CHECK([no])
MATE_COMPILE_WARNINGS

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_INSTALL
LT_INIT

YELP_HELP_INIT

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

# GLIB_COMPILE_RESOURCES
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
	AC_MSG_ERROR([glib-compile-resources not found])
fi

# XMLLINT
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
	AC_MSG_ERROR([xmllint not found])
fi

AC_ARG_ENABLE(deprecated,
        [AS_HELP_STRING([--enable-deprecated],
                [Warn about deprecated usages [default=no]])],,
         [enable_deprecated=no])

if test "x$enable_deprecated" = "xyes"; then
	DISABLE_DEPRECATED=""
	CPPFLAGS="$CPPFLAGS $DISABLE_DEPRECATED"
fi

CPPFLAGS="$CPPFLAGS -fexceptions"

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

GLIB_GSETTINGS

dnl ----------------------------------------------------------------------------
dnl - Library dependencies
dnl ----------------------------------------------------------------------------
GLIB_REQUIRED=2.50.0
GIO_REQUIRED=2.50.0
GDK_REQUIRED=3.22.0
GTK_REQUIRED=3.22.0
LIBSECRET_REQUIRED=0.11
GNOME_KEYRING_REQUIRED=3.0.0
DBUS_REQUIRED=1.0
DBUS_GLIB_REQUIRED=0.70
LIBNOTIFY_REQUIRED=0.7.0
CAIRO_REQUIRED=1.0.0
MATE_DESKTOP_REQUIRED=1.27.1
LIBPANEL_REQUIRED=1.17.0
XRANDR_REQUIRED=1.3.0
XPROTO_REQUIRED=7.0.15
CANBERRA_REQUIRED=0.10
UPOWER_REQUIRED=0.99.8

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

PKG_CHECK_MODULES(MATE_DESKTOP, [
 mate-desktop-2.0 >= $MATE_DESKTOP_REQUIRED])

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
dnl - Build libsecret support
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(libsecret,
        [AS_HELP_STRING([--without-libsecret],
                        [Disable the use of libsecret])],
        [],
        [with_libsecret=yes])

AM_CONDITIONAL([WITH_LIBSECRET],[test "$with_libsecret" = "yes"])

if test "$with_libsecret" = "yes"; then
        PKG_CHECK_MODULES(LIBSECRET, libsecret-1 >= $LIBSECRET_REQUIRED)
        AC_DEFINE([WITH_LIBSECRET],[1],[Define if LIBSECRET support is enabled])
fi

dnl ---------------------------------------------------------------------------
dnl - Build gnome-keyring support
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(keyring,
        [AS_HELP_STRING([--with-keyring],
                        [Enable the use of gnome-keyring])],
        [],
        [with_keyring=no])

AM_CONDITIONAL([WITH_KEYRING],[test "$with_keyring" = "yes"])

if test "$with_keyring" = "yes"; then
        PKG_CHECK_MODULES(KEYRING, gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED)
        AC_DEFINE([WITH_KEYRING],[1],[Define if KEYRING support is enabled])
fi

if test "$with_libsecret" = "yes" -a "$with_keyring" = "yes"; then
	AC_MSG_ERROR([Please select only 1, keyring or libsecret])
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
])

AC_OUTPUT

dnl ==========================================================================
echo "
Configure summary:

    ${PACKAGE_STRING}
    `echo $PACKAGE_STRING | sed "s/./=/g"`

    prefix ......................: ${prefix}
    datadir .....................: ${datadir}
    compiler ....................: ${CC}
    cflags ......................: ${CFLAGS}
    cwarnings ...................: ${WARN_CFLAGS}

    libsecret support ...........: ${with_libsecret}
    gnome-keyring support .......: ${with_keyring}
    Building extra applets ......: ${enable_applets}
    Self test support ...........: ${have_tests}
    dbus-1 services dir .........: $DBUS_SERVICES_DIR
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

