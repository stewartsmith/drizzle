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

#include <m_string.h>
#include <m_ctype.h>


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

struct my_cs_file_section_st
{
  int        state;
  const char *str;
};

#define _CS_MISC	1
#define _CS_ID		2
#define _CS_CSNAME	3
#define _CS_FAMILY	4
#define _CS_ORDER	5
#define _CS_COLNAME	6
#define _CS_FLAG	7
#define _CS_CHARSET	8
#define _CS_COLLATION	9
#define _CS_UPPERMAP	10
#define _CS_LOWERMAP	11
#define _CS_UNIMAP	12
#define _CS_COLLMAP	13
#define _CS_CTYPEMAP	14
#define _CS_PRIMARY_ID	15
#define _CS_BINARY_ID	16
#define _CS_CSDESCRIPT	17
#define _CS_RESET	18
#define	_CS_DIFF1	19
#define	_CS_DIFF2	20
#define	_CS_DIFF3	21


#define MY_CS_CSDESCR_SIZE	64
#define MY_CS_TAILORING_SIZE	1024

typedef struct my_cs_file_info
{
  char   csname[MY_CS_NAME_SIZE];
  char   name[MY_CS_NAME_SIZE];
  uchar  ctype[MY_CS_CTYPE_TABLE_SIZE];
  uchar  to_lower[MY_CS_TO_LOWER_TABLE_SIZE];
  uchar  to_upper[MY_CS_TO_UPPER_TABLE_SIZE];
  uchar  sort_order[MY_CS_SORT_ORDER_TABLE_SIZE];
  uint16_t tab_to_uni[MY_CS_TO_UNI_TABLE_SIZE];
  char   comment[MY_CS_CSDESCR_SIZE];
  char   tailoring[MY_CS_TAILORING_SIZE];
  size_t tailoring_length;
  CHARSET_INFO cs;
  int (*add_collation)(CHARSET_INFO *cs);
} MY_CHARSET_LOADER;



/*
  Check repertoire: detect pure ascii strings
*/
uint
my_string_repertoire(const CHARSET_INFO * const cs, const char *str, ulong length)
{
  const char *strend= str + length;
  if (cs->mbminlen == 1)
  {
    for ( ; str < strend; str++)
    {
      if (((uchar) *str) > 0x7F)
        return MY_REPERTOIRE_UNICODE30;
    }
  }
  else
  {
    my_wc_t wc;
    int chlen;
    for (; (chlen= cs->cset->mb_wc(cs, &wc, (uchar *)str, (uchar *)strend)) > 0; str+= chlen)
    {
      if (wc > 0x7F)
        return MY_REPERTOIRE_UNICODE30;
    }
  }
  return MY_REPERTOIRE_ASCII;
}


/*
  Detect whether a character set is ASCII compatible.

  Returns true for:
  
  - all 8bit character sets whose Unicode mapping of 0x7B is '{'
    (ignores swe7 which maps 0x7B to "LATIN LETTER A WITH DIAERESIS")
  
  - all multi-byte character sets having mbminlen == 1
    (ignores ucs2 whose mbminlen is 2)
  
  TODO:
  
  When merging to 5.2, this function should be changed
  to check a new flag MY_CS_NONASCII, 
  
     return (cs->flag & MY_CS_NONASCII) ? 0 : 1;
  
  This flag was previously added into 5.2 under terms
  of WL#3759 "Optimize identifier conversion in client-server protocol"
  especially to mark character sets not compatible with ASCII.
  
  We won't backport this flag to 5.0 or 5.1.
  This function is Ok for 5.0 and 5.1, because we're not going
  to introduce new tricky character sets between 5.0 and 5.2.
*/
bool
my_charset_is_ascii_based(const CHARSET_INFO * const cs)
{
  return 
    (cs->mbmaxlen == 1 && cs->tab_to_uni && cs->tab_to_uni['{'] == '{') ||
    (cs->mbminlen == 1 && cs->mbmaxlen > 1);
}


/*
  Detect if a character set is 8bit,
  and it is pure ascii, i.e. doesn't have
  characters outside U+0000..U+007F
  This functions is shared between "conf_to_src"
  and dynamic charsets loader in "mysqld".
*/
bool
my_charset_is_8bit_pure_ascii(const CHARSET_INFO * const cs)
{
  size_t code;
  if (!cs->tab_to_uni)
    return 0;
  for (code= 0; code < 256; code++)
  {
    if (cs->tab_to_uni[code] > 0x7F)
      return 0;
  }
  return 1;
}


/*
  Shared function between conf_to_src and mysys.
  Check if a 8bit character set is compatible with
  ascii on the range 0x00..0x7F.
*/
bool
my_charset_is_ascii_compatible(const CHARSET_INFO * const cs)
{
  uint i;
  if (!cs->tab_to_uni)
    return 1;
  for (i= 0; i < 128; i++)
  {
    if (cs->tab_to_uni[i] != i)
      return 0;
  }
  return 1;
}
