/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"

/*
  Include all compiled character sets into the client
  If a client don't want to use all of them, he can define his own
  init_compiled_charsets() that only adds those that he wants
*/

#ifdef HAVE_CHARSET_utf8mb4
extern CHARSET_INFO my_charset_utf8mb4_icelandic_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_latvian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_romanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_slovenian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_polish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_estonian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_spanish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_swedish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_turkish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_czech_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_danish_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_lithuanian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_slovak_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_spanish2_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_roman_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_persian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_esperanto_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_hungarian_uca_ci;
extern CHARSET_INFO my_charset_utf8mb4_sinhala_uca_ci;
#endif /* HAVE_CHARSET_utf8mb4 */


bool init_compiled_charsets(myf flags __attribute__((unused)))
{
  CHARSET_INFO *cs;

  add_compiled_collation(&my_charset_bin);
  add_compiled_collation(&my_charset_filename);

#ifdef HAVE_CHARSET_utf8mb4
  add_compiled_collation(&my_charset_utf8mb4_general_ci);
  add_compiled_collation(&my_charset_utf8mb4_bin);
  add_compiled_collation(&my_charset_utf8mb4_unicode_ci);
  add_compiled_collation(&my_charset_utf8mb4_icelandic_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_latvian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_romanian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_slovenian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_polish_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_estonian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_spanish_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_swedish_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_turkish_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_czech_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_danish_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_lithuanian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_slovak_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_spanish2_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_roman_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_persian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_esperanto_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_hungarian_uca_ci);
  add_compiled_collation(&my_charset_utf8mb4_sinhala_uca_ci);
#endif /* HAVE_CHARSET_utf8mb4 */

  /* Copy compiled charsets */
  for (cs=compiled_charsets; cs->name; cs++)
    add_compiled_collation(cs);
  
  return false;
}
