
pkgplugindir = $(pkglibdir)/plugin

EXTRA_LTLIBRARIES = liblogging_query.la
pkgplugin_LTLIBRARIES =	@plugin_logging_query_shared_target@
liblogging_query_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_query_la_CPPFLAGS = $(GLOBAL_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_query_la_SOURCES = logging_query.cc
