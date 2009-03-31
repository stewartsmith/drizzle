if BUILD_LOGGING_GEARMAN

EXTRA_LTLIBRARIES = liblogging_gearman.la
pkgplugin_LTLIBRARIES =	@plugin_logging_gearman_shared_target@
liblogging_gearman_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_gearman_la_LIBADD= ${LIBGEARMAN}
liblogging_gearman_la_CPPFLAGS = ${AM_CPPFLAGS} -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_gearman_la_SOURCES = logging_gearman.cc

EXTRA_LIBRARIES = liblogging_gearman.a
noinst_LIBRARIES = @plugin_logging_gearman_static_target@
liblogging_gearman_a_SOURCES = $(liblogging_gearman_la_SOURCES)

endif
