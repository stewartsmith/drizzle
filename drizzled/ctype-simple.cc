/* Copyright (C) 2002 MySQL AB

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
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <stdarg.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

/*
  Returns the number of bytes required for strnxfrm().
*/

size_t my_strnxfrmlen_simple(const charset_info_st * const cs, size_t len)
{
  return len * (cs->strxfrm_multiply ? cs->strxfrm_multiply : 1);
}


/*
   We can't use vsprintf here as it's not guaranteed to return
   the length on all operating systems.
   This function is also not called in a safe environment, so the
   end buffer must be checked.
*/

size_t my_snprintf_8bit(const charset_info_st * const,
                        char* to, size_t n,
		     const char* fmt, ...)
{
  va_list args;
  int result;
  va_start(args,fmt);
  result= vsnprintf(to, n, fmt, args);
  va_end(args);
  return result;
}


long my_strntol_8bit(const charset_info_st * const cs,
		     const char *nptr, size_t l, int base,
		     char **endptr, int *err)
{
  int negative;
  uint32_t cutoff;
  uint32_t cutlim;
  uint32_t i;
  const char *s;
  unsigned char c;
  const char *save, *e;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for ( ; s<e && my_isspace(cs, *s) ; s++) {}

  if (s == e)
  {
    goto noconv;
  }

  /* Check for a sign.	*/
  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;
  cutoff = (UINT32_MAX) / (uint32_t) base;
  cutlim = (uint32_t) ((UINT32_MAX) % (uint32_t) base);

  overflow = 0;
  i = 0;
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint32_t) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (negative)
  {
    if (i  > (uint32_t) INT32_MIN)
      overflow = 1;
  }
  else if (i > INT32_MAX)
    overflow = 1;

  if (overflow)
  {
    err[0]= ERANGE;
    return negative ? INT32_MIN : INT32_MAX;
  }

  return (negative ? -((long) i) : (long) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


ulong my_strntoul_8bit(const charset_info_st * const cs,
		       const char *nptr, size_t l, int base,
		       char **endptr, int *err)
{
  int negative;
  uint32_t cutoff;
  uint32_t cutlim;
  uint32_t i;
  const char *s;
  unsigned char c;
  const char *save, *e;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for( ; s<e && my_isspace(cs, *s); s++) {}

  if (s==e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;
  cutoff = (UINT32_MAX) / (uint32_t) base;
  cutlim = (uint32_t) ((UINT32_MAX) % (uint32_t) base);
  overflow = 0;
  i = 0;

  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint32_t) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (overflow)
  {
    err[0]= ERANGE;
    return UINT32_MAX;
  }

  return (negative ? -((long) i) : (long) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


int64_t my_strntoll_8bit(const charset_info_st * const cs,
			  const char *nptr, size_t l, int base,
			  char **endptr,int *err)
{
  int negative;
  uint64_t cutoff;
  uint32_t cutlim;
  uint64_t i;
  const char *s, *e;
  const char *save;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for(; s<e && my_isspace(cs,*s); s++) {}

  if (s == e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X'|| s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;

  cutoff = (~(uint64_t) 0) / (unsigned long int) base;
  cutlim = (uint32_t) ((~(uint64_t) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for ( ; s != e; s++)
  {
    unsigned char c= *s;
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint64_t) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (negative)
  {
    if (i  > (uint64_t) INT64_MIN)
      overflow = 1;
  }
  else if (i > (uint64_t) INT64_MAX)
    overflow = 1;

  if (overflow)
  {
    err[0]= ERANGE;
    return negative ? INT64_MIN : INT64_MAX;
  }

  return (negative ? -((int64_t) i) : (int64_t) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


uint64_t my_strntoull_8bit(const charset_info_st * const cs,
			   const char *nptr, size_t l, int base,
			   char **endptr, int *err)
{
  int negative;
  uint64_t cutoff;
  uint32_t cutlim;
  uint64_t i;
  const char *s, *e;
  const char *save;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for(; s<e && my_isspace(cs,*s); s++) {}

  if (s == e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;

  cutoff = (~(uint64_t) 0) / (unsigned long int) base;
  cutlim = (uint32_t) ((~(uint64_t) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for ( ; s != e; s++)
  {
    unsigned char c= *s;

    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint64_t) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (overflow)
  {
    err[0]= ERANGE;
    return (~(uint64_t) 0);
  }

  return (negative ? -((int64_t) i) : (int64_t) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


/*
  Read double from string

  SYNOPSIS:
    my_strntod_8bit()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.
    end		result pointer to end of converted string
    err		Error number if failed conversion

  NOTES:
    If length is not INT32_MAX or str[length] != 0 then the given str must
    be writeable
    If length == INT32_MAX the str must be \0 terminated.

    It's implemented this way to save a buffer allocation and a memory copy.

  RETURN
    Value of number in string
*/


double my_strntod_8bit(const charset_info_st * const,
		       char *str, size_t length,
		       char **end, int *err)
{
  if (length == INT32_MAX)
    length= 65535;                          /* Should be big enough */
  *end= str + length;
  return internal::my_strtod(str, end, err);
}


/*
  This is a fast version optimized for the case of radix 10 / -10

  Assume len >= 1
*/

size_t my_long10_to_str_8bit(const charset_info_st * const,
                             char *dst, size_t len, int radix, long int val)
{
  char buffer[66];
  char *p, *e;
  long int new_val;
  uint32_t sign=0;
  unsigned long int uval = (unsigned long int) val;

  e = p = &buffer[sizeof(buffer)-1];
  *p= 0;

  if (radix < 0)
  {
    if (val < 0)
    {
      /* Avoid integer overflow in (-val) for INT64_MIN (BUG#31799). */
      uval= (unsigned long int)0 - uval;
      *dst++= '-';
      len--;
      sign= 1;
    }
  }

  new_val = (long) (uval / 10);
  *--p    = '0'+ (char) (uval - (unsigned long) new_val * 10);
  val     = new_val;

  while (val != 0)
  {
    new_val=val/10;
    *--p = '0' + (char) (val-new_val*10);
    val= new_val;
  }

  len= min(len, (size_t) (e-p));
  memcpy(dst, p, len);
  return len+sign;
}


size_t my_int64_t10_to_str_8bit(const charset_info_st * const,
                                char *dst, size_t len, int radix,
                                int64_t val)
{
  char buffer[65];
  char *p, *e;
  long long_val;
  uint32_t sign= 0;
  uint64_t uval = (uint64_t)val;

  if (radix < 0)
  {
    if (val < 0)
    {
      /* Avoid integer overflow in (-val) for INT64_MIN (BUG#31799). */
      uval = (uint64_t)0 - uval;
      *dst++= '-';
      len--;
      sign= 1;
    }
  }

  e = p = &buffer[sizeof(buffer)-1];
  *p= 0;

  if (uval == 0)
  {
    *--p= '0';
    len= 1;
    goto cnv;
  }

  while (uval > (uint64_t) LONG_MAX)
  {
    uint64_t quo= uval/(uint32_t) 10;
    uint32_t rem= (uint32_t) (uval- quo* (uint32_t) 10);
    *--p = '0' + rem;
    uval= quo;
  }

  long_val= (long) uval;
  while (long_val != 0)
  {
    long quo= long_val/10;
    *--p = (char) ('0' + (long_val - quo*10));
    long_val= quo;
  }

  len= min(len, (size_t) (e-p));
cnv:
  memcpy(dst, p, len);
  return len+sign;
}


/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

inline static int likeconv(const charset_info_st *cs, const char c) 
{
#ifdef LIKE_CMP_TOUPPER
  return (unsigned char) my_toupper(cs, c);
#else
  return cs->sort_order[(unsigned char)c];
#endif    
}


inline static const char* inc_ptr(const charset_info_st *cs, const char *str, const char *str_end)
{
  // (Strange this macro have been used. If str_end would actually
  // have been used it would have made sense. /Gustaf)
  (void)cs;
  (void)str_end;
  return str++; 
}

int my_wildcmp_8bit(const charset_info_st * const cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  int result= -1;			/* Not found, using wildcards */

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;

      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return 1;				/* No match */
      if (wildstr == wildend)
	return(str != str_end);		/* Match if both are at end */
      result=1;					/* Found an anchor char     */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return(result);
	inc_ptr(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
      unsigned char cmp;

      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	    return(-1);
	  inc_ptr(cs,str,str_end);
	  continue;
	}
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
	return 0;				/* Ok if w_many is last */
      if (str == str_end)
	return(-1);

      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;

      inc_ptr(cs,wildstr,wildend);	/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);
      do
      {
	while (str != str_end && (unsigned char) likeconv(cs,*str) != cmp)
	  str++;
	if (str++ == str_end) return(-1);
	{
	  int tmp=my_wildcmp_8bit(cs,str,str_end,wildstr,wildend,escape,w_one,
				  w_many);
	  if (tmp <= 0)
	    return(tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return(str != str_end ? 1 : 0);
}


/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from
**              min_str and max_str
** res_length	Length of min_str and max_str.
** min_str	Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

bool my_like_range_simple(const charset_info_st * const cs,
                             const char *ptr, size_t ptr_length,
                             char escape, char w_one, char w_many,
                             size_t res_length,
                             char *min_str,char *max_str,
                             size_t *min_length, size_t *max_length)
{
  const char *end= ptr + ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;
  size_t charlen= res_length / cs->mbmaxlen;

  for (; ptr != end && min_str != min_end && charlen > 0 ; ptr++, charlen--)
  {
    if (*ptr == escape && ptr+1 != end)
    {
      ptr++;					/* Skip escape */
      *min_str++= *max_str++ = *ptr;
      continue;
    }
    if (*ptr == w_one)				/* '_' in SQL */
    {
      *min_str++='\0';				/* This should be min char */
      *max_str++= (char) cs->max_sort_char;
      continue;
    }
    if (*ptr == w_many)				/* '%' in SQL */
    {
      /* Calculate length of keys */
      *min_length= ((cs->state & MY_CS_BINSORT) ?
                    (size_t) (min_str - min_org) :
                    res_length);
      *max_length= res_length;
      do
      {
	*min_str++= 0;
	*max_str++= (char) cs->max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++= *max_str++ = *ptr;
  }

 *min_length= *max_length = (size_t) (min_str - min_org);
  while (min_str != min_end)
    *min_str++= *max_str++ = ' ';      /* Because if key compression */
  return 0;
}


size_t my_scan_8bit(const charset_info_st * const cs, const char *str, const char *end, int sq)
{
  const char *str0= str;
  switch (sq)
  {
  case MY_SEQ_INTTAIL:
    if (*str == '.')
    {
      for(str++ ; str != end && *str == '0' ; str++) {}
      return (size_t) (str - str0);
    }
    return 0;

  case MY_SEQ_SPACES:
    for ( ; str < end ; str++)
    {
      if (!my_isspace(cs,*str))
        break;
    }
    return (size_t) (str - str0);
  default:
    return 0;
  }
}


void my_fill_8bit(const charset_info_st * const,
		  char *s, size_t l, int fill)
{
  memset(s, fill, l);
}


size_t my_numchars_8bit(const charset_info_st * const,
		        const char *b, const char *e)
{
  return (size_t) (e - b);
}


size_t my_numcells_8bit(const charset_info_st * const,
                        const char *b, const char *e)
{
  return (size_t) (e - b);
}


size_t my_charpos_8bit(const charset_info_st * const,
                       const char *, const char *, size_t pos)
{
  return pos;
}


size_t my_well_formed_len_8bit(const charset_info_st * const,
                               const char *start, const char *end,
                               size_t nchars, int *error)
{
  size_t nbytes= (size_t) (end-start);
  *error= 0;
  return min(nbytes, nchars);
}


size_t my_lengthsp_8bit(const charset_info_st * const,
                        const char *ptr, size_t length)
{
  const char *end;
  end= (const char *) internal::skip_trailing_space((const unsigned char *)ptr, length);
  return (size_t) (end-ptr);
}


uint32_t my_instr_simple(const charset_info_st * const cs,
                     const char *b, size_t b_length,
                     const char *s, size_t s_length,
                     my_match_t *match, uint32_t nmatch)
{
  const unsigned char *str, *search, *end, *search_end;

  if (s_length <= b_length)
  {
    if (!s_length)
    {
      if (nmatch)
      {
        match->beg= 0;
        match->end= 0;
        match->mb_len= 0;
      }
      return 1;		/* Empty string is always found */
    }

    str= (const unsigned char*) b;
    search= (const unsigned char*) s;
    end= (const unsigned char*) b+b_length-s_length+1;
    search_end= (const unsigned char*) s + s_length;

skip:
    while (str != end)
    {
      if (cs->sort_order[*str++] == cs->sort_order[*search])
      {
	const unsigned char *i,*j;

	i= str;
	j= search+1;

	while (j != search_end)
	  if (cs->sort_order[*i++] != cs->sort_order[*j++])
            goto skip;

	if (nmatch > 0)
	{
	  match[0].beg= 0;
	  match[0].end= (size_t) (str- (const unsigned char*)b-1);
	  match[0].mb_len= match[0].end;

	  if (nmatch > 1)
	  {
	    match[1].beg= match[0].end;
	    match[1].end= match[0].end+s_length;
	    match[1].mb_len= match[1].end-match[1].beg;
	  }
	}
	return 2;
      }
    }
  }
  return 0;
}


typedef struct
{
  int		nchars;
  MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
inline static int plane_number(uint16_t x)
{
  return ((x >> 8) % PLANE_NUM);
}

static int pcmp(const void * f, const void * s)
{
  const uni_idx *F= (const uni_idx*) f;
  const uni_idx *S= (const uni_idx*) s;
  int res;

  if (!(res=((S->nchars)-(F->nchars))))
    res=((F->uidx.from)-(S->uidx.to));
  return res;
}

static bool create_fromuni(charset_info_st *cs, cs_alloc_func alloc)
{
  uni_idx	idx[PLANE_NUM];
  int		i,n;

  /*
    Check that Unicode map is loaded.
    It can be not loaded when the collation is
    listed in Index.xml but not specified
    in the character set specific XML file.
  */
  if (!cs->tab_to_uni)
    return true;

  /* Clear plane statistics */
  memset(idx, 0, sizeof(idx));

  /* Count number of characters in each plane */
  for (i=0; i< 0x100; i++)
  {
    uint16_t wc=cs->tab_to_uni[i];
    int pl= plane_number(wc);

    if (wc || !i)
    {
      if (!idx[pl].nchars)
      {
        idx[pl].uidx.from=wc;
        idx[pl].uidx.to=wc;
      }else
      {
        idx[pl].uidx.from=wc<idx[pl].uidx.from?wc:idx[pl].uidx.from;
        idx[pl].uidx.to=wc>idx[pl].uidx.to?wc:idx[pl].uidx.to;
      }
      idx[pl].nchars++;
    }
  }

  /* Sort planes in descending order */
  qsort(&idx,PLANE_NUM,sizeof(uni_idx),&pcmp);

  for (i=0; i < PLANE_NUM; i++)
  {
    int ch,numchars;

    /* Skip empty plane */
    if (!idx[i].nchars)
      break;

    numchars=idx[i].uidx.to-idx[i].uidx.from+1;
    if (!(idx[i].uidx.tab=(unsigned char*) alloc(numchars * sizeof(*idx[i].uidx.tab))))
      return true;

    memset(idx[i].uidx.tab, 0, numchars*sizeof(*idx[i].uidx.tab));

    for (ch=1; ch < PLANE_SIZE; ch++)
    {
      uint16_t wc=cs->tab_to_uni[ch];
      if (wc >= idx[i].uidx.from && wc <= idx[i].uidx.to && wc)
      {
        int ofs= wc - idx[i].uidx.from;
        idx[i].uidx.tab[ofs]= ch;
      }
    }
  }

  /* Allocate and fill reverse table for each plane */
  n=i;
  if (!(cs->tab_from_uni= (MY_UNI_IDX*) alloc(sizeof(MY_UNI_IDX)*(n+1))))
    return true;

  for (i=0; i< n; i++)
    cs->tab_from_uni[i]= idx[i].uidx;

  /* Set end-of-list marker */
  memset(&cs->tab_from_uni[i], 0, sizeof(MY_UNI_IDX));
  return false;
}

bool my_cset_init_8bit(charset_info_st *cs, cs_alloc_func alloc)
{
  cs->caseup_multiply= 1;
  cs->casedn_multiply= 1;
  cs->pad_char= ' ';
  return create_fromuni(cs, alloc);
}

static void set_max_sort_char(charset_info_st *cs)
{
  unsigned char max_char;
  uint32_t  i;

  if (!cs->sort_order)
    return;

  max_char=cs->sort_order[(unsigned char) cs->max_sort_char];
  for (i= 0; i < 256; i++)
  {
    if ((unsigned char) cs->sort_order[i] > max_char)
    {
      max_char=(unsigned char) cs->sort_order[i];
      cs->max_sort_char= i;
    }
  }
}

bool my_coll_init_simple(charset_info_st *cs, cs_alloc_func)
{
  set_max_sort_char(cs);
  return false;
}


int64_t my_strtoll10_8bit(const charset_info_st * const,
                          const char *nptr, char **endptr, int *error)
{
  return internal::my_strtoll10(nptr, endptr, error);
}


int my_mb_ctype_8bit(const charset_info_st * const cs, int *ctype,
                   const unsigned char *s, const unsigned char *e)
{
  if (s >= e)
  {
    *ctype= 0;
    return MY_CS_TOOSMALL;
  }
  *ctype= cs->ctype[*s + 1];
  return 1;
}


#undef  UINT64_MAX
#define UINT64_MAX           (~(uint64_t) 0)

#define CUTOFF  (UINT64_MAX / 10)
#define CUTLIM  (UINT64_MAX % 10)
#define DIGITS_IN_ULONGLONG 20

static uint64_t d10[DIGITS_IN_ULONGLONG]=
{
  1,
  10,
  100,
  1000,
  10000,
  100000,
  1000000,
  10000000,
  100000000,
  1000000000,
  10000000000ULL,
  100000000000ULL,
  1000000000000ULL,
  10000000000000ULL,
  100000000000000ULL,
  1000000000000000ULL,
  10000000000000000ULL,
  100000000000000000ULL,
  1000000000000000000ULL,
  10000000000000000000ULL
};


/*

  Convert a string to uint64_t integer value
  with rounding.

  SYNOPSYS
    my_strntoull10_8bit()
      cs              in      pointer to character set
      str             in      pointer to the string to be converted
      length          in      string length
      unsigned_flag   in      whether the number is unsigned
      endptr          out     pointer to the stop character
      error           out     returned error code

  DESCRIPTION
    This function takes the decimal representation of integer number
    from string str and converts it to an signed or unsigned
    int64_t value.
    Space characters and tab are ignored.
    A sign character might precede the digit characters.
    The number may have any number of pre-zero digits.
    The number may have decimal point and exponent.
    Rounding is always done in "away from zero" style:
      0.5  ->   1
     -0.5  ->  -1

    The function stops reading the string str after "length" bytes
    or at the first character that is not a part of correct number syntax:

    <signed numeric literal> ::=
      [ <sign> ] <exact numeric literal> [ E [ <sign> ] <unsigned integer> ]

    <exact numeric literal> ::=
                        <unsigned integer> [ <period> [ <unsigned integer> ] ]
                      | <period> <unsigned integer>
    <unsigned integer>   ::= <digit>...

  RETURN VALUES
    Value of string as a signed/unsigned int64_t integer

    endptr cannot be NULL. The function will store the end pointer
    to the stop character here.

    The error parameter contains information how things went:
    0	     ok
    ERANGE   If the the value of the converted number is out of range
    In this case the return value is:
    - UINT64_MAX if unsigned_flag and the number was too big
    - 0 if unsigned_flag and the number was negative
    - INT64_MAX if no unsigned_flag and the number is too big
    - INT64_MIN if no unsigned_flag and the number it too big negative

    EDOM If the string didn't contain any digits.
    In this case the return value is 0.
*/

uint64_t
my_strntoull10rnd_8bit(const charset_info_st * const,
                       const char *str, size_t length, int unsigned_flag,
                       char **endptr, int *error)
{
  const char *dot, *end9, *beg, *end= str + length;
  uint64_t ull;
  ulong ul;
  unsigned char ch;
  int shift= 0, digits= 0, negative, addon;

  /* Skip leading spaces and tabs */
  for ( ; str < end && (*str == ' ' || *str == '\t') ; str++) {}

  if (str >= end)
    goto ret_edom;

  if ((negative= (*str == '-')) || *str=='+') /* optional sign */
  {
    if (++str == end)
      goto ret_edom;
  }

  beg= str;
  end9= (str + 9) > end ? end : (str + 9);
  /* Accumulate small number into ulong, for performance purposes */
  for (ul= 0 ; str < end9 && (ch= (unsigned char) (*str - '0')) < 10; str++)
  {
    ul= ul * 10 + ch;
  }

  if (str >= end) /* Small number without dots and expanents */
  {
    *endptr= (char*) str;
    if (negative)
    {
      if (unsigned_flag)
      {
        *error= ul ? ERANGE : 0;
        return 0;
      }
      else
      {
        *error= 0;
        return (uint64_t) (int64_t) -(long) ul;
      }
    }
    else
    {
      *error=0;
      return (uint64_t) ul;
    }
  }

  digits= str - beg;

  /* Continue to accumulate into uint64_t */
  for (dot= NULL, ull= ul; str < end; str++)
  {
    if ((ch= (unsigned char) (*str - '0')) < 10)
    {
      if (ull < CUTOFF || (ull == CUTOFF && ch <= CUTLIM))
      {
        ull= ull * 10 + ch;
        digits++;
        continue;
      }
      /*
        Adding the next digit would overflow.
        Remember the next digit in "addon", for rounding.
        Scan all digits with an optional single dot.
      */
      if (ull == CUTOFF)
      {
        ull= UINT64_MAX;
        addon= 1;
        str++;
      }
      else
        addon= (*str >= '5');
      if (!dot)
      {
        for ( ; str < end && (ch= (unsigned char) (*str - '0')) < 10; shift++, str++) {}
        if (str < end && *str == '.')
        {
          str++;
          for ( ; str < end && (ch= (unsigned char) (*str - '0')) < 10; str++) {}
        }
      }
      else
      {
        shift= dot - str;
        for ( ; str < end && (ch= (unsigned char) (*str - '0')) < 10; str++) {}
      }
      goto exp;
    }

    if (*str == '.')
    {
      if (dot)
      {
        /* The second dot character */
        addon= 0;
        goto exp;
      }
      else
      {
        dot= str + 1;
      }
      continue;
    }

    /* Unknown character, exit the loop */
    break;
  }
  shift= dot ? dot - str : 0; /* Right shift */
  addon= 0;

exp:    /* [ E [ <sign> ] <unsigned integer> ] */

  if (!digits)
  {
    str= beg;
    goto ret_edom;
  }

  if (str < end && (*str == 'e' || *str == 'E'))
  {
    str++;
    if (str < end)
    {
      int negative_exp, exponent;
      if ((negative_exp= (*str == '-')) || *str=='+')
      {
        if (++str == end)
          goto ret_sign;
      }
      for (exponent= 0 ;
           str < end && (ch= (unsigned char) (*str - '0')) < 10;
           str++)
      {
        exponent= exponent * 10 + ch;
      }
      shift+= negative_exp ? -exponent : exponent;
    }
  }

  if (shift == 0) /* No shift, check addon digit */
  {
    if (addon)
    {
      if (ull == UINT64_MAX)
        goto ret_too_big;
      ull++;
    }
    goto ret_sign;
  }

  if (shift < 0) /* Right shift */
  {
    uint64_t d, r;

    if (-shift >= DIGITS_IN_ULONGLONG)
      goto ret_zero; /* Exponent is a big negative number, return 0 */

    d= d10[-shift];
    r= (ull % d) * 2;
    ull /= d;
    if (r >= d)
      ull++;
    goto ret_sign;
  }

  if (shift > DIGITS_IN_ULONGLONG) /* Huge left shift */
  {
    if (!ull)
      goto ret_sign;
    goto ret_too_big;
  }

  for ( ; shift > 0; shift--, ull*= 10) /* Left shift */
  {
    if (ull > CUTOFF)
      goto ret_too_big; /* Overflow, number too big */
  }

ret_sign:
  *endptr= (char*) str;

  if (!unsigned_flag)
  {
    if (negative)
    {
      if (ull > (uint64_t) INT64_MIN)
      {
        *error= ERANGE;
        return (uint64_t) INT64_MIN;
      }
      *error= 0;
      return (uint64_t) -(int64_t) ull;
    }
    else
    {
      if (ull > (uint64_t) INT64_MAX)
      {
        *error= ERANGE;
        return (uint64_t) INT64_MAX;
      }
      *error= 0;
      return ull;
    }
  }

  /* Unsigned number */
  if (negative && ull)
  {
    *error= ERANGE;
    return 0;
  }
  *error= 0;
  return ull;

ret_zero:
  *endptr= (char*) str;
  *error= 0;
  return 0;

ret_edom:
  *endptr= (char*) str;
  *error= EDOM;
  return 0;

ret_too_big:
  *endptr= (char*) str;
  *error= ERANGE;
  return unsigned_flag ?
         UINT64_MAX :
         negative ? (uint64_t) INT64_MIN : (uint64_t) INT64_MAX;
}


/*
  Check if a constant can be propagated

  SYNOPSIS:
    my_propagate_simple()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.

  NOTES:
   Takes the string in the given charset and check
   if it can be safely propagated in the optimizer.

   create table t1 (
     s char(5) character set latin1 collate latin1_german2_ci);
   insert into t1 values (0xf6); -- o-umlaut
   select * from t1 where length(s)=1 and s='oe';

   The above query should return one row.
   We cannot convert this query into:
   select * from t1 where length('oe')=1 and s='oe';

   Currently we don't check the constant itself,
   and decide not to propagate a constant
   just if the collation itself allows tricky things
   like expansions and contractions. In the future
   we can write a more sophisticated functions to
   check the constants. For example, 'oa' can always
   be safety propagated in German2 because unlike
   'oe' it does not have any special meaning.

  RETURN
    1 if constant can be safely propagated
    0 if it is not safe to propagate the constant
*/



bool my_propagate_simple(const charset_info_st * const, const unsigned char *,
                         size_t)
{
  return 1;
}


bool my_propagate_complex(const charset_info_st * const, const unsigned char *,
                          size_t)
{
  return 0;
}

/*
  Apply DESC and REVERSE collation rules.

  SYNOPSIS:
    my_strxfrm_desc_and_reverse()
    str      - pointer to string
    strend   - end of string
    flags    - flags
    level    - which level, starting from 0.

  NOTES:
    Apply DESC or REVERSE or both flags.

    If DESC flag is given, then the weights
    come out NOTed or negated for that level.

    If REVERSE flags is given, then the weights come out in
    reverse order for that level, that is, starting with
    the last character and ending with the first character.

    If nether DESC nor REVERSE flags are give,
    the string is not changed.

*/
void my_strxfrm_desc_and_reverse(unsigned char *str, unsigned char *strend,
                                 uint32_t flags, uint32_t level)
{
  if (flags & (MY_STRXFRM_DESC_LEVEL1 << level))
  {
    if (flags & (MY_STRXFRM_REVERSE_LEVEL1 << level))
    {
      for (strend--; str <= strend;)
      {
        unsigned char tmp= *str;
        *str++= ~*strend;
        *strend--= ~tmp;
      }
    }
    else
    {
      for (; str < strend; str++)
        *str= ~*str;
    }
  }
  else if (flags & (MY_STRXFRM_REVERSE_LEVEL1 << level))
  {
    for (strend--; str < strend;)
    {
      unsigned char tmp= *str;
      *str++= *strend;
      *strend--= tmp;
    }
  }
}


size_t
my_strxfrm_pad_desc_and_reverse(const charset_info_st * const cs,
                                unsigned char *str, unsigned char *frmend, unsigned char *strend,
                                uint32_t nweights, uint32_t flags, uint32_t level)
{
  if (nweights && frmend < strend && (flags & MY_STRXFRM_PAD_WITH_SPACE))
  {
    uint32_t fill_length= min((uint32_t) (strend - frmend), nweights * cs->mbminlen);
    cs->cset->fill(cs, (char*) frmend, fill_length, cs->pad_char);
    frmend+= fill_length;
  }
  my_strxfrm_desc_and_reverse(str, frmend, flags, level);
  return frmend - str;
}

} /* namespace drizzled */
