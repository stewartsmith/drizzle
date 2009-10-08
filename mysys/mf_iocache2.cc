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

/*
  More functions to be used with IO_CACHE files
*/

#include "mysys/mysys_priv.h"
#include <mystrings/m_string.h>
#include <stdarg.h>
#include <mystrings/m_ctype.h>
#include <mysys/iocache.h>
#include <string>

using namespace std;

static my_off_t my_b_append_tell(IO_CACHE* info)
{
  /*
    Prevent optimizer from putting res in a register when debugging
    we need this to be able to see the value of res when the assert fails
  */
  volatile my_off_t res;

  /*
    We need to lock the append buffer mutex to keep flush_io_cache()
    from messing with the variables that we need in order to provide the
    answer to the question.
  */
  pthread_mutex_lock(&info->append_buffer_lock);
  res = info->end_of_file + (info->write_pos-info->append_read_pos);
  pthread_mutex_unlock(&info->append_buffer_lock);
  return res;
}

my_off_t my_b_safe_tell(IO_CACHE *info)
{
  if (unlikely(info->type == SEQ_READ_APPEND))
    return my_b_append_tell(info);
  return my_b_tell(info);
}

/*
  Make next read happen at the given position
  For write cache, make next write happen at the given position
*/

void my_b_seek(IO_CACHE *info,my_off_t pos)
{
  my_off_t offset;

  /*
    TODO:
       Verify that it is OK to do seek in the non-append
       area in SEQ_READ_APPEND cache
     a) see if this always works
     b) see if there is a better way to make it work
  */
  if (info->type == SEQ_READ_APPEND)
    flush_io_cache(info);

  offset=(pos - info->pos_in_file);

  if (info->type == READ_CACHE || info->type == SEQ_READ_APPEND)
  {
    /* TODO: explain why this works if pos < info->pos_in_file */
    if ((uint64_t) offset < (uint64_t) (info->read_end - info->buffer))
    {
      /* The read is in the current buffer; Reuse it */
      info->read_pos = info->buffer + offset;
      return;
    }
    else
    {
      /* Force a new read on next my_b_read */
      info->read_pos=info->read_end=info->buffer;
    }
  }
  else if (info->type == WRITE_CACHE)
  {
    /* If write is in current buffer, reuse it */
    if ((uint64_t) offset <
	(uint64_t) (info->write_end - info->write_buffer))
    {
      info->write_pos = info->write_buffer + offset;
      return;
    }
    flush_io_cache(info);
    /* Correct buffer end so that we write in increments of IO_SIZE */
    info->write_end=(info->write_buffer+info->buffer_length-
		     (pos & (IO_SIZE-1)));
  }
  info->pos_in_file=pos;
  info->seek_not_done=1;
  return;
}

/*
  Simple printf version.  Supports '%s', '%d', '%u', "%ld" and "%lu"
  Used for logging in MySQL
  returns number of written character, or (size_t) -1 on error
*/

size_t my_b_printf(IO_CACHE *info, const char* fmt, ...)
{
  size_t result;
  va_list args;
  va_start(args,fmt);
  result=my_b_vprintf(info, fmt, args);
  va_end(args);
  return result;
}


size_t my_b_vprintf(IO_CACHE *info, const char* fmt, va_list args)
{
  size_t out_length= 0;
  int32_t minimum_width; /* as yet unimplemented */
  int32_t minimum_width_sign;
  uint32_t precision; /* as yet unimplemented for anything but %b */
  bool is_zero_padded;
  basic_string<unsigned char> buffz;

  /*
    Store the location of the beginning of a format directive, for the
    case where we learn we shouldn't have been parsing a format string
    at all, and we don't want to lose the flag/precision/width/size
    information.
   */
  const char* backtrack;

  for (; *fmt != '\0'; fmt++)
  {
    /* Copy everything until '%' or end of string */
    const char *start=fmt;
    size_t length;

    for (; (*fmt != '\0') && (*fmt != '%'); fmt++) ;

    length= (size_t) (fmt - start);
    out_length+=length;
    if (my_b_write(info, (const unsigned char*) start, length))
      goto err;

    if (*fmt == '\0')				/* End of format */
      return out_length;

    /*
      By this point, *fmt must be a percent;  Keep track of this location and
      skip over the percent character.
    */
    assert(*fmt == '%');
    backtrack= fmt;
    fmt++;

    is_zero_padded= false;
    minimum_width_sign= 1;
    minimum_width= 0;
    precision= 0;
    /* Skip if max size is used (to be compatible with printf) */

process_flags:
    switch (*fmt)
    {
      case '-':
        minimum_width_sign= -1; fmt++; goto process_flags;
      case '0':
        is_zero_padded= true; fmt++; goto process_flags;
      case '#':
        /** @todo Implement "#" conversion flag. */  fmt++; goto process_flags;
      case ' ':
        /** @todo Implement " " conversion flag. */  fmt++; goto process_flags;
      case '+':
        /** @todo Implement "+" conversion flag. */  fmt++; goto process_flags;
    }

    if (*fmt == '*')
    {
      precision= (int) va_arg(args, int);
      fmt++;
    }
    else
    {
      while (my_isdigit(&my_charset_utf8_general_ci, *fmt)) {
        minimum_width=(minimum_width * 10) + (*fmt - '0');
        fmt++;
      }
    }
    minimum_width*= minimum_width_sign;

    if (*fmt == '.')
    {
      fmt++;
      if (*fmt == '*') {
        precision= (int) va_arg(args, int);
        fmt++;
      }
      else
      {
        while (my_isdigit(&my_charset_utf8_general_ci, *fmt)) {
          precision=(precision * 10) + (*fmt - '0');
          fmt++;
        }
      }
    }

    if (*fmt == 's')				/* String parameter */
    {
      register char *par = va_arg(args, char *);
      size_t length2 = strlen(par);
      /* TODO: implement precision */
      out_length+= length2;
      if (my_b_write(info, (const unsigned char*) par, length2))
	goto err;
    }
    else if (*fmt == 'b')                       /* Sized buffer parameter, only precision makes sense */
    {
      char *par = va_arg(args, char *);
      out_length+= precision;
      if (my_b_write(info, (const unsigned char*) par, precision))
        goto err;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      ssize_t length2;
      char buff[17];

      iarg = va_arg(args, int);
      if (*fmt == 'd')
	length2= (size_t) (int10_to_str((long) iarg,buff, -10) - buff);
      else
        length2= (uint32_t) (int10_to_str((long) (uint32_t) iarg,buff,10)- buff);

      /* minimum width padding */
      if (minimum_width > length2)
      {
        const size_t buflen = minimum_width - length2;
        buffz.clear();
        buffz.reserve(buflen);
        buffz.append(buflen, is_zero_padded ? '0' : ' ');
        my_b_write(info, buffz.data(), buflen);
      }

      out_length+= length2;
      if (my_b_write(info, (const unsigned char *)buff, length2))
	goto err;
    }
    else if ((*fmt == 'l' && fmt[1] == 'd') || fmt[1] == 'u')
      /* long parameter */
    {
      register long iarg;
      size_t length2;
      char buff[17];

      iarg = va_arg(args, long);
      if (*++fmt == 'd')
        length2= (size_t) (int10_to_str(static_cast<int32_t>(iarg),
                                        buff, -10) - buff);
      else
        length2= (size_t) (int10_to_str(static_cast<int32_t>(iarg),buff,10)- buff);
      out_length+= length2;
      if (my_b_write(info, (unsigned char*) buff, length2))
	goto err;
    }
    else
    {
      /* %% or unknown code */
      if (my_b_write(info, (const unsigned char*) backtrack, (size_t) (fmt-backtrack)))
        goto err;
      out_length+= fmt-backtrack;
    }
  }
  return out_length;

err:
  return (size_t) -1;
}
