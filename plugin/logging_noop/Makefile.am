# drizzle/plugin/logging_noop/Makefile.am

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin

EXTRA_LTLIBRARIES =	liblogging_noop.la
pkgplugin_LTLIBRARIES =	@plugin_logging_noop_shared_target@
liblogging_noop_la_LDFLAGS =	-module -rpath $(pkgplugindir)
liblogging_noop_la_CPPFLAGS=	$(GLOBAL_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_noop_la_SOURCES =	logging_noop.cc
