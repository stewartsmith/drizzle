/* Copyright (C) 2000-2003 MySQL AB

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

/*
  Functions for read record cacheing with myisam
  Used for reading dynamic/compressed records from datafile.

  Can fetch data directly from file (outside cache),
  if reading a small chunk straight before the cached part (with possible
  overlap).

  Can be explicitly asked not to use cache (by not setting READING_NEXT in
  flag) - useful for occasional out-of-cache reads, when the next read is
  expected to hit the cache again.

  Allows "partial read" errors in the record header (when READING_HEADER flag
  is set) - unread part is zero'ed

  Note: out-of-cache reads are enabled for shared io_cache_st's too,
  as these reads will be cached by OS cache (and my_pread is always atomic)
*/


#include "myisam_priv.h"

#include <algorithm>

using namespace std;
using namespace drizzled;


int _mi_read_cache(internal::io_cache_st *info, unsigned char *buff, internal::my_off_t pos, uint32_t length,
		   int flag)
{
  uint32_t read_length,in_buff_length;
  internal::my_off_t offset;
  unsigned char *in_buff_pos;

  if (pos < info->pos_in_file)
  {
    read_length=length;
    if ((internal::my_off_t) read_length > (internal::my_off_t) (info->pos_in_file-pos))
      read_length=(uint) (info->pos_in_file-pos);
    info->seek_not_done=1;
    if (my_pread(info->file,buff,read_length,pos,MYF(MY_NABP)))
      return(1);
    if (!(length-=read_length))
      return(0);
    pos+=read_length;
    buff+=read_length;
  }
  if (pos >= info->pos_in_file &&
      (offset= (internal::my_off_t) (pos - info->pos_in_file)) <
      (internal::my_off_t) (info->read_end - info->request_pos))
  {
    in_buff_pos=info->request_pos+(uint) offset;
    in_buff_length= min(length, (uint32_t) (info->read_end-in_buff_pos));
    memcpy(buff,info->request_pos+(uint) offset,(size_t) in_buff_length);
    if (!(length-=in_buff_length))
      return(0);
    pos+=in_buff_length;
    buff+=in_buff_length;
  }
  else
    in_buff_length=0;
  if (flag & READING_NEXT)
  {
    if (pos != (info->pos_in_file +
		(uint) (info->read_end - info->request_pos)))
    {
      info->pos_in_file=pos;				/* Force start here */
      info->read_pos=info->read_end=info->request_pos;	/* Everything used */
      info->seek_not_done=1;
    }
    else
      info->read_pos=info->read_end;			/* All block used */
    if (!(*info->read_function)(info,buff,length))
      return(0);
    read_length=info->error;
  }
  else
  {
    info->seek_not_done=1;
    if ((read_length=my_pread(info->file,buff,length,pos,MYF(0))) == length)
      return(0);
  }
  if (!(flag & READING_HEADER) || (int) read_length == -1 ||
      read_length+in_buff_length < 3)
  {
    if (!errno || errno == -1)
      errno=HA_ERR_WRONG_IN_RECORD;
    return(1);
  }
  memset(buff+read_length, 0,
         MI_BLOCK_INFO_HEADER_LENGTH - in_buff_length - read_length);
  return(0);
} /* _mi_read_cache */
