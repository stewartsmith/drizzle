

EXTRA_LTLIBRARIES = liblogging_syslog.la
pkgplugin_LTLIBRARIES =	@plugin_logging_syslog_shared_target@
liblogging_syslog_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_syslog_la_CPPFLAGS = $(GLOBAL_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_syslog_la_SOURCES = logging_syslog.cc
