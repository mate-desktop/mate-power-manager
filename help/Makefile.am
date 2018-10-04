@YELP_HELP_RULES@

HELP_ID = mate-power-manager
HELP_FILES = index.docbook legal.xml
HELP_MEDIA = \
	figures/applet-brightness.png \
	figures/applet-inhibit.png \
	figures/gpm-cell-capacity.png \
	figures/gpm-charged.png \
	figures/gpm-critical.png \
	figures/gpm-low.png \
	figures/gpm-prefs-ac.png \
	figures/gpm-prefs-battery.png \
	figures/gpm-prefs-general.png \
	figures/gpm-stats-graph.png \
	figures/gpm-suspend-problem.png \
	figures/gpm-unplugged.png \
	figures/gs-prefs.png

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )
