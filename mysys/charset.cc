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
#include "mysys_err.h"
#include <mystrings/m_ctype.h>
#include <mystrings/m_string.h>
#include <my_dir.h>


/*
  The code below implements this functionality:

    - Initializing charset related structures
    - Loading dynamic charsets
    - Searching for a proper CHARSET_INFO
      using charset name, collation name or collation ID
    - Setting server default character set
*/

bool my_charset_same(const CHARSET_INFO *cs1, const CHARSET_INFO *cs2)
{
  return ((cs1 == cs2) || !strcmp(cs1->csname,cs2->csname));
}


static uint
get_collation_number_internal(const char *name)
{
  CHARSET_INFO **cs;
  for (cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
  {
    if ( cs[0] && cs[0]->name &&
         !my_strcasecmp(&my_charset_utf8_general_ci, cs[0]->name, name))
      return cs[0]->number;
  }
  return 0;
}


static bool init_state_maps(CHARSET_INFO *cs)
{
  uint32_t i;
  unsigned char *state_map;
  unsigned char *ident_map;

  if (!(cs->state_map= (unsigned char*) my_once_alloc(256, MYF(MY_WME))))
    return 1;

  if (!(cs->ident_map= (unsigned char*) my_once_alloc(256, MYF(MY_WME))))
    return 1;

  state_map= cs->state_map;
  ident_map= cs->ident_map;

  /* Fill state_map with states to get a faster parser */
  for (i=0; i < 256 ; i++)
  {
    if (my_isalpha(cs,i))
      state_map[i]=(unsigned char) MY_LEX_IDENT;
    else if (my_isdigit(cs,i))
      state_map[i]=(unsigned char) MY_LEX_NUMBER_IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
    else if (my_mbcharlen(cs, i)>1)
      state_map[i]=(unsigned char) MY_LEX_IDENT;
#endif
    else if (my_isspace(cs,i))
      state_map[i]=(unsigned char) MY_LEX_SKIP;
    else
      state_map[i]=(unsigned char) MY_LEX_CHAR;
  }
  state_map[(unsigned char)'_']=state_map[(unsigned char)'$']=(unsigned char) MY_LEX_IDENT;
  state_map[(unsigned char)'\'']=(unsigned char) MY_LEX_STRING;
  state_map[(unsigned char)'.']=(unsigned char) MY_LEX_REAL_OR_POINT;
  state_map[(unsigned char)'>']=state_map[(unsigned char)'=']=state_map[(unsigned char)'!']= (unsigned char) MY_LEX_CMP_OP;
  state_map[(unsigned char)'<']= (unsigned char) MY_LEX_LONG_CMP_OP;
  state_map[(unsigned char)'&']=state_map[(unsigned char)'|']=(unsigned char) MY_LEX_BOOL;
  state_map[(unsigned char)'#']=(unsigned char) MY_LEX_COMMENT;
  state_map[(unsigned char)';']=(unsigned char) MY_LEX_SEMICOLON;
  state_map[(unsigned char)':']=(unsigned char) MY_LEX_SET_VAR;
  state_map[0]=(unsigned char) MY_LEX_EOL;
  state_map[(unsigned char)'\\']= (unsigned char) MY_LEX_ESCAPE;
  state_map[(unsigned char)'/']= (unsigned char) MY_LEX_LONG_COMMENT;
  state_map[(unsigned char)'*']= (unsigned char) MY_LEX_END_LONG_COMMENT;
  state_map[(unsigned char)'@']= (unsigned char) MY_LEX_USER_END;
  state_map[(unsigned char) '`']= (unsigned char) MY_LEX_USER_VARIABLE_DELIMITER;
  state_map[(unsigned char)'"']= (unsigned char) MY_LEX_STRING_OR_DELIMITER;

  /*
    Create a second map to make it faster to find identifiers
  */
  for (i=0; i < 256 ; i++)
  {
    ident_map[i]= (unsigned char) (state_map[i] == MY_LEX_IDENT ||
			   state_map[i] == MY_LEX_NUMBER_IDENT);
  }

  /* Special handling of hex and binary strings */
  state_map[(unsigned char)'x']= state_map[(unsigned char)'X']= (unsigned char) MY_LEX_IDENT_OR_HEX;
  state_map[(unsigned char)'b']= state_map[(unsigned char)'B']= (unsigned char) MY_LEX_IDENT_OR_BIN;
  return 0;
}


#define MY_CHARSET_INDEX "Index.xml"

const char *charsets_dir= NULL;
static int charset_initialized=0;


char *get_charsets_dir(char *buf)
{
  const char *sharedir= SHAREDIR;
  char *res;

  if (charsets_dir != NULL)
    strncpy(buf, charsets_dir, FN_REFLEN-1);
  else
  {
    if (test_if_hard_path(sharedir) ||
	is_prefix(sharedir, DEFAULT_CHARSET_HOME))
      strxmov(buf, sharedir, "/", CHARSET_DIR, NULL);
    else
      strxmov(buf, DEFAULT_CHARSET_HOME, "/", sharedir, "/", CHARSET_DIR,
	      NULL);
  }
  res= convert_dirname(buf,buf,NULL);
  return(res);
}

CHARSET_INFO *all_charsets[256];
const CHARSET_INFO *default_charset_info = &my_charset_utf8_general_ci;

void add_compiled_collation(CHARSET_INFO * cs)
{
  all_charsets[cs->number]= cs;
  cs->state|= MY_CS_AVAILABLE;
}

void *cs_alloc(size_t size)
{
  return my_once_alloc(size, MYF(MY_WME));
}


static bool init_available_charsets(myf myflags)
{
  char fname[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
  bool error=false;
  /*
    We have to use charset_initialized to not lock on THR_LOCK_charset
    inside get_internal_charset...
  */
  if (!charset_initialized)
  {
    CHARSET_INFO **cs;
    /*
      To make things thread safe we are not allowing other threads to interfere
      while we may changing the cs_info_table
    */
    pthread_mutex_lock(&THR_LOCK_charset);
    if (!charset_initialized)
    {
      memset(&all_charsets, 0, sizeof(all_charsets));
      init_compiled_charsets(myflags);

      /* Copy compiled charsets */
      for (cs=all_charsets;
           cs < all_charsets+array_elements(all_charsets)-1 ;
           cs++)
      {
        if (*cs)
        {
          if (cs[0]->ctype)
            if (init_state_maps(*cs))
              *cs= NULL;
        }
      }

      strcpy(get_charsets_dir(fname), MY_CHARSET_INDEX);
      charset_initialized=1;
    }
    pthread_mutex_unlock(&THR_LOCK_charset);
  }
  return error;
}


void free_charsets(void)
{
  charset_initialized=0;
}


uint32_t get_collation_number(const char *name)
{
  init_available_charsets(MYF(0));
  return get_collation_number_internal(name);
}


uint32_t get_charset_number(const char *charset_name, uint32_t cs_flags)
{
  CHARSET_INFO **cs;
  init_available_charsets(MYF(0));

  for (cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
  {
    if ( cs[0] && cs[0]->csname && (cs[0]->state & cs_flags) &&
         !my_strcasecmp(&my_charset_utf8_general_ci, cs[0]->csname, charset_name))
      return cs[0]->number;
  }
  return 0;
}


const char *get_charset_name(uint32_t charset_number)
{
  const CHARSET_INFO *cs;
  init_available_charsets(MYF(0));

  cs=all_charsets[charset_number];
  if (cs && (cs->number == charset_number) && cs->name )
    return (char*) cs->name;

  return (char*) "?";   /* this mimics find_type() */
}


static const CHARSET_INFO *get_internal_charset(uint32_t cs_number)
{
  CHARSET_INFO *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
  pthread_mutex_lock(&THR_LOCK_charset);
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
  pthread_mutex_unlock(&THR_LOCK_charset);
  return cs;
}


const CHARSET_INFO *get_charset(uint32_t cs_number, myf flags)
{
  const CHARSET_INFO *cs;
  if (cs_number == default_charset_info->number)
    return default_charset_info;

  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */

  if (!cs_number || cs_number >= array_elements(all_charsets)-1)
    return NULL;

  cs= get_internal_charset(cs_number);

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)], cs_string[23];
    strcpy(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    cs_string[0]='#';
    int10_to_str(cs_number, cs_string+1, 10);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_string, index_file);
  }
  return cs;
}

const CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags)
{
  uint32_t cs_number;
  const CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */

  cs_number=get_collation_number(cs_name);
  cs= cs_number ? get_internal_charset(cs_number) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    strcpy(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_COLLATION, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}


const CHARSET_INFO *get_charset_by_csname(const char *cs_name,
				    uint32_t cs_flags,
				    myf flags)
{
  uint32_t cs_number;
  const CHARSET_INFO *cs;

  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */

  cs_number= get_charset_number(cs_name, cs_flags);
  cs= cs_number ? get_internal_charset(cs_number) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    strcpy(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return(cs);
}


/**
  Resolve character set by the character set name (utf8, latin1, ...).

  The function tries to resolve character set by the specified name. If
  there is character set with the given name, it is assigned to the "cs"
  parameter and false is returned. If there is no such character set,
  "default_cs" is assigned to the "cs" and true is returned.

  @param[in] cs_name    Character set name.
  @param[in] default_cs Default character set.
  @param[out] cs        Variable to store character set.

  @return false if character set was resolved successfully; true if there
  is no character set with given name.
*/

bool resolve_charset(const char *cs_name,
                     const CHARSET_INFO *default_cs,
                     const CHARSET_INFO **cs)
{
  *cs= get_charset_by_csname(cs_name, MY_CS_PRIMARY, MYF(0));

  if (*cs == NULL)
  {
    *cs= default_cs;
    return true;
  }

  return false;
}


/**
  Resolve collation by the collation name (utf8_general_ci, ...).

  The function tries to resolve collation by the specified name. If there
  is collation with the given name, it is assigned to the "cl" parameter
  and false is returned. If there is no such collation, "default_cl" is
  assigned to the "cl" and true is returned.

  @param[out] cl        Variable to store collation.
  @param[in] cl_name    Collation name.
  @param[in] default_cl Default collation.

  @return false if collation was resolved successfully; true if there is no
  collation with given name.
*/

bool resolve_collation(const char *cl_name,
                       const CHARSET_INFO *default_cl,
                       const CHARSET_INFO **cl)
{
  *cl= get_charset_by_name(cl_name, MYF(0));

  if (*cl == NULL)
  {
    *cl= default_cl;
    return true;
  }

  return false;
}


#ifdef BACKSLASH_MBTAIL
static CHARSET_INFO *fs_cset_cache= NULL;

CHARSET_INFO *fs_character_set()
{
  if (!fs_cset_cache)
  {
    char buf[10]= "cp";
    GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE,
                  buf+2, sizeof(buf)-3);
    /*
      We cannot call get_charset_by_name here
      because fs_character_set() is executed before
      LOCK_THD_charset mutex initialization, which
      is used inside get_charset_by_name.
      As we're now interested in cp932 only,
      let's just detect it using strcmp().
    */
    fs_cset_cache= !strcmp(buf, "cp932") ?
                   &my_charset_cp932_japanese_ci : &my_charset_bin;
  }
  return fs_cset_cache;
}
#endif

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

size_t escape_quotes_for_drizzle(const CHARSET_INFO *charset_info,
                                 char *to, size_t to_length,
                                 const char *from, size_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + (to_length ? to_length-1 : 2*length);
  bool overflow= false;
#ifdef USE_MB
  bool use_mb_flag= use_mb(charset_info);
#endif
  for (end= from + length; from < end; from++)
  {
#ifdef USE_MB
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
#endif
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
  return overflow ? UINT32_MAX : (uint32_t) (to - to_start);
}
