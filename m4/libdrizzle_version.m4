#
# Version information for libdrizzle
#
#
LIBDRIZZLE_VERSION=1.0.1
LIBDRIZZLE_LIBRARY_VERSION=4:0:0
#                             | | |
#                      +------+ | +---+
#                      |        |     |
#                   current:revision:age
#                      |        |     |
#                      |        |     +- increment if interfaces have been added
#                      |        |        set to zero if interfaces have been
#                      |        |        removed or changed
#                      |        +- increment if source code has changed
#                      |           set to zero if current is incremented
#                      +- increment if interfaces have been added, removed or
#                         changed
AC_SUBST(LIBDRIZZLE_LIBRARY_VERSION)
AC_SUBST(LIBDRIZZLE_VERSION)
AC_DEFINE_UNQUOTED([LIBDRIZZLE_VERSION],[$LIBDRIZZLE_VERSION], [libdrizzle version])

# libdrizzle versioning when linked with GNU ld.
AS_IF([test "$lt_cv_prog_gnu_ld" = "yes"],[
  LD_VERSION_SCRIPT="-Wl,--version-script=\$(top_srcdir)/config/drizzle.ver"
  ])
AC_SUBST(LD_VERSION_SCRIPT)

LIBDRIZZLE_HEX_VERSION=`echo $LIBDRIZZLE_VERSION | sed 's|[\-a-z0-9]*$||' | awk -F. '{printf "0x%0.2d%0.3d%0.3d", $[]1, $[]2, $[]3}'`
AC_SUBST([LIBDRIZZLE_HEX_VERSION])
