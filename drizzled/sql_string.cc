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

/* This file is originally from the mysql distribution. Coded by monty */

#include <config.h>

#include <drizzled/definitions.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/memory/root.h>
#include <drizzled/charset.h>

#include <algorithm>

#include <drizzled/sql_string.h>

using namespace std;

namespace drizzled {

/*****************************************************************************
** String functions
*****************************************************************************/

String::String()
  : Ptr(NULL),
    str_length(0),
    Alloced_length(0),
    alloced(false),
    str_charset(&my_charset_bin)
{ }


String::String(size_t length_arg)
  : Ptr(NULL),
    str_length(0),
    Alloced_length(0),
    alloced(false),
    str_charset(&my_charset_bin)
{
  (void) real_alloc(length_arg);
}

String::String(const char *str, const charset_info_st * const cs)
  : Ptr(const_cast<char *>(str)),
    str_length(static_cast<size_t>(strlen(str))),
    Alloced_length(0),
    alloced(false),
    str_charset(cs)
{ }


String::String(const char *str, size_t len, const charset_info_st * const cs)
  : Ptr(const_cast<char *>(str)),
    str_length(len),
    Alloced_length(0),
    alloced(false),
    str_charset(cs)
{ }


String::String(char *str, size_t len, const charset_info_st * const cs)
  : Ptr(str),
    str_length(len),
    Alloced_length(len),
    alloced(false),
    str_charset(cs)
{ }


String::String(const String &str)
  : Ptr(str.Ptr),
    str_length(str.str_length),
    Alloced_length(str.Alloced_length),
    alloced(false),
    str_charset(str.str_charset)
{ }


void *String::operator new(size_t size, memory::Root *mem_root)
{
  return mem_root->alloc(size);
}

String::~String() { free(); }

void String::real_alloc(size_t arg_length)
{
  arg_length=ALIGN_SIZE(arg_length+1);
  str_length=0;
  if (Alloced_length < arg_length)
  {
    if (Alloced_length > 0)
      free();
    Ptr=(char*) malloc(arg_length);
    Alloced_length=arg_length;
    alloced=1;
  }
  Ptr[0]=0;
}


/*
** Check that string is big enough. Set string[alloc_length] to 0
** (for C functions)
*/

void String::realloc(size_t alloc_length)
{
  size_t len=ALIGN_SIZE(alloc_length+1);
  if (Alloced_length < len)
  {
    char *new_ptr;
    if (alloced)
    {
      new_ptr= (char*) ::realloc(Ptr,len);
      Ptr=new_ptr;
      Alloced_length=len;
    }
    else 
    {
      new_ptr= (char*) malloc(len);
      if (str_length)				// Avoid bugs in memcpy on AIX
        memcpy(new_ptr,Ptr,str_length);
      new_ptr[str_length]=0;
      Ptr=new_ptr;
      Alloced_length=len;
      alloced=1;
    }
  }
  Ptr[alloc_length]=0;			// This make other funcs shorter
}

void String::set_int(int64_t num, bool unsigned_flag, const charset_info_st * const cs)
{
  size_t l=20*cs->mbmaxlen+1;
  int base= unsigned_flag ? 10 : -10;

  alloc(l);
  str_length=(size_t) (cs->cset->int64_t10_to_str)(cs,Ptr,l,base,num);
  str_charset=cs;
}

void String::set_real(double num,size_t decimals, const charset_info_st * const cs)
{
  char buff[FLOATING_POINT_BUFFER];
  size_t len;

  str_charset=cs;
  if (decimals >= NOT_FIXED_DEC)
  {
    len= internal::my_gcvt(num, internal::MY_GCVT_ARG_DOUBLE, sizeof(buff) - 1, buff, NULL);
    copy(buff, len, cs);
    return;
  }
  len= internal::my_fcvt(num, decimals, buff, NULL);
  copy(buff, len, cs);
}


void String::copy()
{
  if (!alloced)
  {
    Alloced_length=0;				// Force realloc
    realloc(str_length);
  }
}

void String::copy(const String &str)
{
  alloc(str.str_length);
  str_length=str.str_length;
  memmove(Ptr, str.Ptr, str_length);		// May be overlapping
  Ptr[str_length]=0;
  str_charset=str.str_charset;
}

void String::copy(const std::string& arg, const charset_info_st * const cs)	// Allocate new string
{
  alloc(arg.size());

  if ((str_length= arg.size()))
    memcpy(Ptr, arg.c_str(), arg.size());

  Ptr[arg.size()]= 0;
  str_charset= cs;
}

void String::copy(const char *str,size_t arg_length, const charset_info_st* cs)
{
  alloc(arg_length);
  if ((str_length=arg_length))
    memcpy(Ptr,str,arg_length);
  Ptr[arg_length]=0;
  str_charset=cs;
}

/*
  Checks that the source string can be just copied to the destination string
  without conversion.

  SYNPOSIS

  needs_conversion()
  arg_length		Length of string to copy.
  from_cs		Character set to copy from
  to_cs			Character set to copy to
  size_t *offset	Returns number of unaligned characters.

  RETURN
   0  No conversion needed
   1  Either character set conversion or adding leading  zeros
      (e.g. for UCS-2) must be done

  NOTE
  to_cs may be NULL for "no conversion" if the system variable
  character_set_results is NULL.
*/

bool String::needs_conversion(size_t arg_length,
			      const charset_info_st * const from_cs,
			      const charset_info_st * const to_cs,
			      size_t *offset)
{
  *offset= 0;
  if (!to_cs ||
      (to_cs == &my_charset_bin) ||
      (to_cs == from_cs) ||
      my_charset_same(from_cs, to_cs) ||
      ((from_cs == &my_charset_bin) &&
       (!(*offset=(arg_length % to_cs->mbminlen)))))
    return false;
  return true;
}




void String::set_or_copy_aligned(const char *str,size_t arg_length, const charset_info_st* cs)
{
  /* How many bytes are in incomplete character */
  size_t offset= (arg_length % cs->mbminlen);

  assert(!offset); /* All characters are complete, just copy */

  set(str, arg_length, cs);
}

/*
  Set a string to the value of a latin1-string, keeping the original charset

  SYNOPSIS
    copy_or_set()
    str			String of a simple charset (latin1)
    arg_length		Length of string

  IMPLEMENTATION
    If string object is of a simple character set, set it to point to the
    given string.
    If not, make a copy and convert it to the new character set.

  RETURN
    0	ok
    1	Could not allocate result buffer

*/

void String::set_ascii(const char *str, size_t arg_length)
{
  if (str_charset->mbminlen == 1)
  {
    set(str, arg_length, str_charset);
    return;
  }
  copy(str, arg_length, str_charset);
}

void String::append(const String &s)
{
  if (s.length())
  {
    realloc(str_length+s.length());
    memcpy(Ptr+str_length,s.ptr(),s.length());
    str_length+=s.length();
  }
}


/*
  Append an ASCII string to the a string of the current character set
*/

void String::append(const char *s,size_t arg_length)
{
  if (!arg_length)
    return;

  /*
    For an ASCII compatinble string we can just append.
  */
  realloc(str_length+arg_length);
  memcpy(Ptr+str_length,s,arg_length);
  str_length+=arg_length;
}


/*
  Append a 0-terminated ASCII string
*/

void String::append(const char *s)
{
  append(s, strlen(s));
}


/*
  Append a string in the given charset to the string
  with character set recoding
*/

void String::append(const char *s,size_t arg_length, const charset_info_st * const)
{
  realloc(str_length + arg_length);
  memcpy(Ptr + str_length, s, arg_length);
  str_length+= arg_length;
}


void String::append_with_prefill(const char *s,size_t arg_length,
		 size_t full_length, char fill_char)
{
  int t_length= arg_length > full_length ? arg_length : full_length;

  realloc(str_length + t_length);
  t_length= full_length - arg_length;
  if (t_length > 0)
  {
    memset(Ptr+str_length, fill_char, t_length);
    str_length=str_length + t_length;
  }
  append(s, arg_length);
}

size_t String::numchars()
{
  return str_charset->cset->numchars(str_charset, Ptr, Ptr+str_length);
}

int String::charpos(int i,size_t offset)
{
  if (i <= 0)
    return i;
  return str_charset->cset->charpos(str_charset,Ptr+offset,Ptr+str_length,i);
}

int String::strstr(const String &s,size_t offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return ((int) offset);	// Empty string is always found

    const char *str = Ptr+offset;
    const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skip:
    while (str != end)
    {
      if (*str++ == *search)
      {
	char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (*i++ != *j++) goto skip;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}

/*
** Search string from end. Offset is offset to the end of string
*/

int String::strrstr(const String &s,size_t offset)
{
  if (s.length() <= offset && offset <= str_length)
  {
    if (!s.length())
      return offset;				// Empty string is always found
    const char *str = Ptr+offset-1;
    const char *search=s.ptr()+s.length()-1;

    const char *end=Ptr+s.length()-2;
    const char *search_end=s.ptr()-1;
skip:
    while (str != end)
    {
      if (*str-- == *search)
      {
	char *i,*j;
	i=(char*) str; j=(char*) search-1;
	while (j != search_end)
	  if (*i-- != *j--) goto skip;
	return (int) (i-Ptr) +1;
      }
    }
  }
  return -1;
}

/*
  Replace substring with string
  If wrong parameter or not enough memory, do nothing
*/

void String::replace(size_t offset,size_t arg_length,const String &to)
{
  replace(offset,arg_length,to.ptr(),to.length());
}

void String::replace(size_t offset,size_t arg_length,
                     const char *to, size_t to_length)
{
  long diff = (long) to_length-(long) arg_length;
  if (offset+arg_length <= str_length)
  {
    if (diff < 0)
    {
      if (to_length)
	memcpy(Ptr+offset,to,to_length);
      memmove(Ptr+offset+to_length, Ptr+offset+arg_length,
              str_length-offset-arg_length);
    }
    else
    {
      if (diff)
      {
	realloc(str_length+(size_t) diff);
	internal::bmove_upp((unsigned char*) Ptr+str_length+diff,
                            (unsigned char*) Ptr+str_length,
                            str_length-offset-arg_length);
      }
      if (to_length)
	memcpy(Ptr+offset,to,to_length);
    }
    str_length+=(size_t) diff;
  }
}



/*
  Compare strings according to collation, without end space.

  SYNOPSIS
    sortcmp()
    s		First string
    t		Second string
    cs		Collation

  NOTE:
    Normally this is case sensitive comparison

  RETURN
  < 0	s < t
  0	s == t
  > 0	s > t
*/


int sortcmp(const String *s,const String *t, const charset_info_st * const cs)
{
 return cs->coll->strnncollsp(cs,
                              (unsigned char *) s->ptr(),s->length(),
                              (unsigned char *) t->ptr(),t->length(), 0);
}


/*
  Compare strings byte by byte. End spaces are also compared.

  SYNOPSIS
    stringcmp()
    s		First string
    t		Second string

  NOTE:
    Strings are compared as a stream of unsigned chars

  RETURN
  < 0	s < t
  0	s == t
  > 0	s > t
*/


int stringcmp(const String *s,const String *t)
{
  size_t s_len= s->length(), t_len= t->length(), len= min(s_len,t_len);
  int cmp= memcmp(s->ptr(), t->ptr(), len);
  return (cmp) ? cmp : (int) (s_len - t_len);
}


String *copy_if_not_alloced(String *to,String *from,size_t from_length)
{
  if (from->Alloced_length >= from_length)
    return from;
  if (from->alloced || !to || from == to)
  {
    (void) from->realloc(from_length);
    return from;
  }
  to->realloc(from_length);
  if ((to->str_length= min(from->str_length,from_length)))
    memcpy(to->Ptr,from->Ptr,to->str_length);
  to->str_charset=from->str_charset;
  return to;
}


/****************************************************************************
  Help functions
****************************************************************************/

/*
  copy a string,
  with optional character set conversion,
  with optional left padding (for binary -> UCS2 conversion)

  SYNOPSIS
    well_formed_copy_nchars()
    to			     Store result here
    to_length                Maxinum length of "to" string
    to_cs		     Character set of "to" string
    from		     Copy from here
    from_length		     Length of from string
    from_cs		     From character set
    nchars                   Copy not more that nchars characters
    well_formed_error_pos    Return position when "from" is not well formed
                             or NULL otherwise.
    cannot_convert_error_pos Return position where a not convertable
                             character met, or NULL otherwise.
    from_end_pos             Return position where scanning of "from"
                             string stopped.
  NOTES

  RETURN
    length of bytes copied to 'to'
*/


size_t
well_formed_copy_nchars(const charset_info_st * const to_cs,
                        char *to, size_t to_length,
                        const charset_info_st * const from_cs,
                        const char *from, size_t from_length,
                        size_t nchars,
                        const char **well_formed_error_pos,
                        const char **cannot_convert_error_pos,
                        const char **from_end_pos)
{
  size_t res;

  assert((to_cs == &my_charset_bin) ||
         (from_cs == &my_charset_bin) ||
         (to_cs == from_cs) ||
         my_charset_same(from_cs, to_cs));

  if (to_length < to_cs->mbminlen || !nchars)
  {
    *from_end_pos= from;
    *cannot_convert_error_pos= NULL;
    *well_formed_error_pos= NULL;
    return 0;
  }

  if (to_cs == &my_charset_bin)
  {
    res= min(min(nchars, to_length), from_length);
    memmove(to, from, res);
    *from_end_pos= from + res;
    *well_formed_error_pos= NULL;
    *cannot_convert_error_pos= NULL;
  }
  else
  {
    int well_formed_error;
    size_t from_offset;

    if ((from_offset= (from_length % to_cs->mbminlen)) &&
        (from_cs == &my_charset_bin))
    {
      /*
        Copying from BINARY to UCS2 needs to prepend zeros sometimes:
        INSERT INTO t1 (ucs2_column) VALUES (0x01);
        0x01 -> 0x0001
      */
      size_t pad_length= to_cs->mbminlen - from_offset;
      memset(to, 0, pad_length);
      memmove(to + pad_length, from, from_offset);
      nchars--;
      from+= from_offset;
      from_length-= from_offset;
      to+= to_cs->mbminlen;
      to_length-= to_cs->mbminlen;
    }

    set_if_smaller(from_length, to_length);
    res= to_cs->cset->well_formed_len(to_cs, from, from + from_length,
                                      nchars, &well_formed_error);
    memmove(to, from, res);
    *from_end_pos= from + res;
    *well_formed_error_pos= well_formed_error ? from + res : NULL;
    *cannot_convert_error_pos= NULL;
    if (from_offset)
      res+= to_cs->mbminlen;
  }

  return res;
}




void String::print(String *str)
{
  char *st= (char*)Ptr, *end= st+str_length;
  for (; st < end; st++)
  {
    unsigned char c= *st;
    switch (c)
    {
    case '\\':
      str->append("\\\\", sizeof("\\\\")-1);
      break;
    case '\0':
      str->append("\\0", sizeof("\\0")-1);
      break;
    case '\'':
      str->append("\\'", sizeof("\\'")-1);
      break;
    case '\n':
      str->append("\\n", sizeof("\\n")-1);
      break;
    case '\r':
      str->append("\\r", sizeof("\\r")-1);
      break;
    case '\032': // Ctrl-Z
      str->append("\\Z", sizeof("\\Z")-1);
      break;
    default:
      str->append(c);
    }
  }
}

/*
  Quote the given identifier.
  If the given identifier is empty, it will be quoted.

  SYNOPSIS
  append_identifier()
  name                  the identifier to be appended
  name_length           length of the appending identifier
*/

/* Factor the extern out */
extern const charset_info_st *system_charset_info, *files_charset_info;

void String::append_identifier(const char *name, size_t in_length)
{
  const char *name_end;
  char quote_char;
  int q= '`';

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  reserve(in_length*2 + 2);
  quote_char= (char) q;
  append(&quote_char, 1, system_charset_info);

  for (name_end= name+in_length ; name < name_end ; name+= in_length)
  {
    unsigned char chr= (unsigned char) *name;
    in_length= my_mbcharlen(system_charset_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!in_length)
      in_length= 1;
    if (in_length == 1 && chr == (unsigned char) quote_char)
      append(&quote_char, 1, system_charset_info);
    append(name, in_length, system_charset_info);
  }
  append(&quote_char, 1, system_charset_info);
}

bool check_if_only_end_space(const charset_info_st * const cs, char *str,
                             char *end)
{
  return str+ cs->cset->scan(cs, str, end, MY_SEQ_SPACES) == end;
}

std::ostream& operator<<(std::ostream& output, const String &str)
{
  output << "String:(";
  output <<  const_cast<String&>(str).c_str();
  output << ", ";
  output << str.length();
  output << ")";

  return output;  // for multiple << operators.
}

} /* namespace drizzled */

bool operator==(const drizzled::String &s1, const drizzled::String &s2)
{
  return stringcmp(&s1,&s2) == 0;
}

bool operator!=(const drizzled::String &s1, const drizzled::String &s2)
{
  return !(s1 == s2);
}

