
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = 						\
	po						\
	src						\
	data						\
	help						\
	policy

if HAVE_APPLETS
SUBDIRS += applets
endif

EXTRA_DIST =						\
	autogen.sh

# Distribute the Meson build system files as well
EXTRA_DIST += \
	meson.build				\
	meson_options.txt			\
	meson_post_install.sh			\
	applets/brightness/meson.build		\
	applets/inhibit/meson.build		\
	applets/power-profiles/meson.build	\
	data/meson.build			\
	data/icons/meson.build			\
	help/meson.build			\
	help/LINGUAS				\
	po/meson.build				\
	policy/meson.build			\
	src/meson.build

clean-local :
	rm -f *~

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

DISTCLEANFILES =					\
	mate-power-manager-*.tar.gz

DISTCHECK_CONFIGURE_FLAGS =   \
	--disable-schemas-install \
	--disable-applets         \
	--disable-tests \
	--without-keyring \
	--enable-compile-warnings=no \
	CFLAGS='-Wno-deprecated-declarations'

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
