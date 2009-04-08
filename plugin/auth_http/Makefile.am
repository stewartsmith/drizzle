if BUILD_AUTH_HTTP

EXTRA_LTLIBRARIES = libauth_http.la
pkgplugin_LTLIBRARIES =	@plugin_auth_http_shared_target@
libauth_http_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
libauth_http_la_CPPFLAGS = $(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libauth_http_la_SOURCES = auth_http.cc

EXTRA_LIBRARIES = libauth_http.a
noinst_LIBRARIES = @plugin_auth_http_static_target@
libauth_http_a_SOURCES = $(libauth_http_la_SOURCES)

endif
