
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = 						\
	src						\
	po						\
	data						\
	help						\
	policy

if HAVE_APPLETS
SUBDIRS += applets
endif

EXTRA_DIST =						\
	autogen.sh					\
	intltool-extract.in				\
	intltool-merge.in				\
	intltool-update.in

clean-local :
	rm -f *~

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

DISTCLEANFILES =					\
	intltool-extract				\
	intltool-merge					\
	intltool-update					\
	mate-power-manager-*.tar.gz

DISTCHECK_CONFIGURE_FLAGS =   \
	--disable-schemas-install \
	--disable-applets         \
	--disable-tests

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

