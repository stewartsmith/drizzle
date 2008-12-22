

EXTRA_LTLIBRARIES = liberrmsg_stderr.la
pkgplugin_LTLIBRARIES =	@plugin_errmsg_stderr_shared_target@
liberrmsg_stderr_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liberrmsg_stderr_la_CPPFLAGS= $(GLOBAL_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liberrmsg_stderr_la_SOURCES = errmsg_stderr.cc
