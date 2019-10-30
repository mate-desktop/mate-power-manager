polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.mate.power.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

$(polkit_policy_DATA): $(polkit_policy_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

.in2.in:
	sed "s|[@]sbindir@|${sbindir}|" $< > $@

EXTRA_DIST = org.mate.power.policy.in2
DISTCLEANFILES = $(polkit_policy_DATA) $(polkit_policy_in_files)

clean-local :
	rm -f *~

