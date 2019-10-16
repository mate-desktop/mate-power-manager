actioniconsdir = $(pkgdatadir)/icons/hicolor
appiconsdir = $(datadir)/icons/hicolor
statusiconsdir = $(pkgdatadir)/icons/hicolor

sizes = 16x16 22x22 24x24 32x32 48x48 64x64 96x96 128x128 256x256

nobase_dist_actionicons_DATA = \
	$(foreach size,$(sizes),$(wildcard $(size)/actions/*.png)) \
	$(wildcard scalable/actions/*.svg)

nobase_dist_appicons_DATA = \
	$(foreach size,$(sizes),$(wildcard $(size)/apps/*.png)) \
	$(wildcard scalable/apps/*.svg)

nobase_dist_statusicons_DATA = \
	$(foreach size,$(sizes),$(wildcard $(size)/status/*.png)) \
	$(wildcard scalable/status/*.svg)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(pkgdatadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi
