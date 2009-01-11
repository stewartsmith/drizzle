dnl In order to add new charset, you must add charset name to
dnl this CHARSETS_AVAILABLE list and sql/share/charsets/Index.xml.
dnl If the character set uses strcoll or other special handling,
dnl you must also create strings/ctype-$charset_name.c

AC_DIVERT_PUSH(0)

define(CHARSETS_AVAILABLE0,binary)
define(CHARSETS_AVAILABLE5,utf8)

DEFAULT_CHARSET=utf8
CHARSETS_AVAILABLE="CHARSETS_AVAILABLE0 CHARSETS_AVAILABLE5"
CHARSETS_COMPLEX="utf8"

AC_DIVERT_POP

AC_ARG_WITH(charset,
   [  --with-charset=CHARSET
                          Default character set, use one of: CHARSETS_AVAILABLE0 CHARSETS_AVAILABLE5],
   [default_charset="$withval"],
   [default_charset="$DEFAULT_CHARSET"])

AC_ARG_WITH(collation,
   [  --with-collation=COLLATION
                          Default collation],
   [default_collation="$withval"],
   [default_collation="default"])

AC_MSG_CHECKING("character sets")

CHARSETS="$default_charset utf8"

for cs in $CHARSETS
do
  case $cs in 
    binary)
      ;;
    utf8)
      AC_DEFINE(HAVE_CHARSET_utf8mb4, 1, [Define to enable ut8])
      AC_DEFINE([USE_MB], 1, [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, [1], [ ])
      ;;
    *)
      AC_MSG_ERROR([Charset '$cs' not available. (Available are: $CHARSETS_AVAILABLE).
      See the Installation chapter in the Reference Manual.])
  esac
done


      default_charset_collations=""

case $default_charset in 
    utf8)
      default_charset_default_collation="utf8_general_ci"
      define(UTFC1, utf8_general_ci utf8_bin)
      define(UTFC2, utf8_czech_ci utf8_danish_ci)
      define(UTFC3, utf8_esperanto_ci utf8_estonian_ci utf8_icelandic_ci)
      define(UTFC4, utf8_latvian_ci utf8_lithuanian_ci)
      define(UTFC5, utf8_persian_ci utf8_polish_ci utf8_romanian_ci)
      define(UTFC6, utf8_sinhala_ci utf8_slovak_ci utf8_slovenian_ci)
      define(UTFC7, utf8_spanish2_ci utf8_spanish_ci)
      define(UTFC8, utf8_swedish_ci utf8_turkish_ci)
      define(UTFC9, utf8_unicode_ci)
      UTFC="UTFC1 UTFC2 UTFC3 UTFC4 UTFC5 UTFC6 UTFC7 UTFC8 UTFC9"
      default_charset_collations="$UTFC"
      ;;
    *)
      AC_MSG_ERROR([Charset $cs not available. (Available are: $CHARSETS_AVAILABLE).
      See the Installation chapter in the Reference Manual.])
esac

if test "$default_collation" = default; then
  default_collation=$default_charset_default_collation
fi

valid_default_collation=no
for cl in $default_charset_collations
do
  if test x"$cl" = x"$default_collation"
  then
    valid_default_collation=yes
    break
  fi
done

if test x$valid_default_collation = xyes
then
  AC_MSG_RESULT([default: $default_charset, collation: $default_collation; compiled in: $CHARSETS])
else
  AC_MSG_ERROR([
      Collation $default_collation is not valid for character set $default_charset.
      Valid collations are: $default_charset_collations.
      See the Installation chapter in the Reference Manual.
  ])
fi

AC_DEFINE_UNQUOTED([DRIZZLE_DEFAULT_CHARSET_NAME], ["$default_charset"],
                   [Define the default charset name])
AC_DEFINE_UNQUOTED([DRIZZLE_DEFAULT_COLLATION_NAME], ["$default_collation"],
                   [Define the default charset name])
