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

#include <drizzled/charset.h>
#include <drizzled/error.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/configmake.h>
#include <vector>

#include <drizzled/visibility.h>

using namespace std;

namespace drizzled {

/*
  We collect memory in this vector that we free on delete.
*/
static vector<unsigned char*> memory_vector;

/*
  The code below implements this functionality:

    - Initializing charset related structures
    - Loading dynamic charsets
    - Searching for a proper charset_info_st
      using charset name, collation name or collation ID
    - Setting server default character set
*/

bool my_charset_same(const charset_info_st *cs1, const charset_info_st *cs2)
{
  return cs1 == cs2 || not strcmp(cs1->csname, cs2->csname);
}

static uint get_collation_number_internal(const char *name)
{
  for (charset_info_st **cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1;
       cs++)
  {
    if ( cs[0] && cs[0]->name && !my_strcasecmp(&my_charset_utf8_general_ci, cs[0]->name, name))
    {
      return cs[0]->number;
    }
  }
  return 0;
}

static unsigned char* cs_alloc(size_t size)
{
  memory_vector.push_back(new unsigned char[size]);
  return memory_vector.back();
}

static void init_state_maps(charset_info_st *cs)
{
  cs->state_map= cs_alloc(256);
  cs->ident_map= cs_alloc(256);

  unsigned char *state_map= cs->state_map;
  unsigned char *ident_map= cs->ident_map;

  /* Fill state_map with states to get a faster parser */
  for (int i= 0; i < 256; i++)
  {
    if (my_isalpha(cs,i))
      state_map[i]= MY_LEX_IDENT;
    else if (my_isdigit(cs,i))
      state_map[i]= MY_LEX_NUMBER_IDENT;
    else if (my_mbcharlen(cs, i)>1)
      state_map[i]= MY_LEX_IDENT;
    else if (my_isspace(cs,i))
      state_map[i]= MY_LEX_SKIP;
    else
      state_map[i]= MY_LEX_CHAR;
  }
  state_map['_']=state_map['$']= MY_LEX_IDENT;
  state_map['\'']= MY_LEX_STRING;
  state_map['.']= MY_LEX_REAL_OR_POINT;
  state_map['>']=state_map['=']=state_map['!']=  MY_LEX_CMP_OP;
  state_map['<']=  MY_LEX_LONG_CMP_OP;
  state_map['&']=state_map['|']= MY_LEX_BOOL;
  state_map['#']= MY_LEX_COMMENT;
  state_map[';']= MY_LEX_SEMICOLON;
  state_map[':']= MY_LEX_SET_VAR;
  state_map[0]= MY_LEX_EOL;
  state_map['\\']=  MY_LEX_ESCAPE;
  state_map['/']=  MY_LEX_LONG_COMMENT;
  state_map['*']=  MY_LEX_END_LONG_COMMENT;
  state_map['@']=  MY_LEX_USER_END;
  state_map['`']=  MY_LEX_USER_VARIABLE_DELIMITER;
  state_map['"']=  MY_LEX_STRING_OR_DELIMITER;

  /*
    Create a second map to make it faster to find identifiers
  */
  for (int i= 0; i < 256; i++)
  {
    ident_map[i]= state_map[i] == MY_LEX_IDENT || state_map[i] == MY_LEX_NUMBER_IDENT;
  }

  /* Special handling of hex and binary strings */
  state_map['x']= state_map['X']=  MY_LEX_IDENT_OR_HEX;
  state_map['b']= state_map['B']=  MY_LEX_IDENT_OR_BIN;
}

static bool charset_initialized= false;

DRIZZLED_API charset_info_st *all_charsets[256];
const DRIZZLED_API charset_info_st *default_charset_info = &my_charset_utf8_general_ci;

void add_compiled_collation(charset_info_st * cs)
{
  all_charsets[cs->number]= cs;
  cs->state|= MY_CS_AVAILABLE;
}

static void init_available_charsets(myf myflags)
{
  /*
    We have to use charset_initialized to not lock on THR_LOCK_charset
    inside get_internal_charset...
  */
  if (charset_initialized)
    return;
  memset(&all_charsets, 0, sizeof(all_charsets));
  init_compiled_charsets(myflags);

  /* Copy compiled charsets */
  for (charset_info_st**cs= all_charsets;
    cs < all_charsets+array_elements(all_charsets)-1;
    cs++)
  {
    if (*cs && cs[0]->ctype)
      init_state_maps(*cs);
  }

  charset_initialized= true;
}

void free_charsets()
{
  charset_initialized= false;

  while (not memory_vector.empty())
  {
    delete[] memory_vector.back();
    memory_vector.pop_back();
  }
}

uint32_t get_collation_number(const char *name)
{
  init_available_charsets(MYF(0));
  return get_collation_number_internal(name);
}

uint32_t get_charset_number(const char *charset_name, uint32_t cs_flags)
{
  charset_info_st **cs;
  init_available_charsets(MYF(0));

  for (cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
  {
    if ( cs[0] && cs[0]->csname && (cs[0]->state & cs_flags) && !my_strcasecmp(&my_charset_utf8_general_ci, cs[0]->csname, charset_name))
      return cs[0]->number;
  }
  return 0;
}

const char *get_charset_name(uint32_t charset_number)
{
  init_available_charsets(MYF(0));

  const charset_info_st *cs= all_charsets[charset_number];
  if (cs && (cs->number == charset_number) && cs->name )
    return cs->name;

  return "?";   /* this mimics find_type() */
}

static const charset_info_st *get_internal_charset(uint32_t cs_number)
{
  charset_info_st *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
  if ((cs= all_charsets[cs_number]))
  {
    if (!(cs->state & MY_CS_COMPILED) && !(cs->state & MY_CS_LOADED))
    {
      assert(0);
    }
    cs= (cs->state & MY_CS_AVAILABLE) ? cs : NULL;
  }
  if (cs && !(cs->state & MY_CS_READY))
  {
    if ((cs->cset->init && cs->cset->init(cs, cs_alloc)) ||
        (cs->coll->init && cs->coll->init(cs, cs_alloc)))
      cs= NULL;
    else
      cs->state|= MY_CS_READY;
  }

  return cs;
}

const charset_info_st *get_charset(uint32_t cs_number)
{
  if (cs_number == default_charset_info->number)
    return default_charset_info;

  init_available_charsets(MYF(0));	/* If it isn't initialized */

  if (!cs_number || cs_number >= array_elements(all_charsets)-1)
    return NULL;

  return get_internal_charset(cs_number);
}

const charset_info_st *get_charset_by_name(const char *cs_name)
{
  init_available_charsets(MYF(0));	/* If it isn't initialized */
  uint32_t cs_number= get_collation_number(cs_name);
  return cs_number ? get_internal_charset(cs_number) : NULL;
}

const charset_info_st *get_charset_by_csname(const char *cs_name, uint32_t cs_flags)
{
  init_available_charsets(MYF(0));	/* If it isn't initialized */
  uint32_t cs_number= get_charset_number(cs_name, cs_flags);
  return cs_number ? get_internal_charset(cs_number) : NULL;
}


/*
  Escape apostrophes by doubling them up

  SYNOPSIS
    escape_quotes_for_drizzle()
    charset_info        Charset of the strings
    to                  Buffer for escaped string
    to_length           Length of destination buffer, or 0
    from                The string to escape
    length              The length of the string to escape

  DESCRIPTION
    This escapes the contents of a string by doubling up any apostrophes that
    it contains. This is used when the NO_BACKSLASH_ESCAPES SQL_MODE is in
    effect on the server.

  NOTE
    To be consistent with escape_string_for_mysql(), to_length may be 0 to
    mean "big enough"

  RETURN VALUES
    UINT32_MAX  The escaped string did not fit in the to buffer
    >=0         The length of the escaped string
*/

size_t escape_quotes_for_drizzle(const charset_info_st *charset_info,
                                 char *to, size_t to_length,
                                 const char *from, size_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + (to_length ? to_length-1 : 2*length);
  bool overflow= false;
  bool use_mb_flag= use_mb(charset_info);
  for (end= from + length; from < end; from++)
  {
    int tmp_length;
    if (use_mb_flag && (tmp_length= my_ismbchar(charset_info, from, end)))
    {
      if (to + tmp_length > to_end)
      {
        overflow= true;
        break;
      }
      while (tmp_length--)
	*to++= *from++;
      from--;
      continue;
    }
    /*
      We don't have the same issue here with a non-multi-byte character being
      turned into a multi-byte character by the addition of an escaping
      character, because we are only escaping the ' character with itself.
     */
    if (*from == '\'')
    {
      if (to + 2 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= '\'';
      *to++= '\'';
    }
    else
    {
      if (to + 1 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= *from;
    }
  }
  *to= 0;
  return overflow ? UINT32_MAX : to - to_start;
}

} /* namespace drizzled */
