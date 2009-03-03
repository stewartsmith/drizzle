EXTRA_LTLIBRARIES = liblogging_query.la
pkgplugin_LTLIBRARIES =	@plugin_logging_query_shared_target@
liblogging_query_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_query_la_CPPFLAGS = $(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_query_la_SOURCES = logging_query.cc

EXTRA_LIBRARIES = liblogging_query.a
noinst_LIBRARIES = @plugin_logging_query_static_target@
liblogging_query_a_SOURCES = $(liblogging_query_la_SOURCES)
