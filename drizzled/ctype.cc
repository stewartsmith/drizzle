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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/charset.h>

namespace drizzled
{

/*

  This files implements routines which parse XML based
  character set and collation description files.

  Unicode collations are encoded according to

    Unicode Technical Standard #35
    Locale Data Markup Language (LDML)
    http://www.unicode.org/reports/tr35/

  and converted into ICU string according to

    Collation Customization
    http://oss.software.ibm.com/icu/userguide/Collate_Customization.html

*/


#define MY_CS_CSDESCR_SIZE	64
#define MY_CS_TAILORING_SIZE	1024

typedef struct my_cs_file_info
{
  char   csname[MY_CS_NAME_SIZE];
  char   name[MY_CS_NAME_SIZE];
  unsigned char  ctype[MY_CS_CTYPE_TABLE_SIZE];
  unsigned char  to_lower[MY_CS_TO_LOWER_TABLE_SIZE];
  unsigned char  to_upper[MY_CS_TO_UPPER_TABLE_SIZE];
  unsigned char  sort_order[MY_CS_SORT_ORDER_TABLE_SIZE];
  uint16_t tab_to_uni[MY_CS_TO_UNI_TABLE_SIZE];
  char   comment[MY_CS_CSDESCR_SIZE];
  char   tailoring[MY_CS_TAILORING_SIZE];
  size_t tailoring_length;
  charset_info_st cs;
  int (*add_collation)(charset_info_st *cs);
} MY_CHARSET_LOADER;

} /* namespace drizzled */
