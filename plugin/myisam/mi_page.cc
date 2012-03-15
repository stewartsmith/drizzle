/* Copyright (C) 2000-2004, 2006 MySQL AB

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

/* Read and write key blocks */

#include "myisam_priv.h"

using namespace drizzled;

	/* Fetch a key-page in memory */

unsigned char *_mi_fetch_keypage(MI_INFO *info, MI_KEYDEF *keyinfo,
			 internal::my_off_t page, int, unsigned char *buff, int)
{
  if (not pread(info->s->kfile, buff, keyinfo->block_length, page))
  {
    info->last_keypage=HA_OFFSET_ERROR;
    mi_print_error(info->s, HA_ERR_CRASHED);
    errno=HA_ERR_CRASHED;
    return(0);
  }
  unsigned char* tmp= buff;
  if (tmp == info->buff)
    info->buff_used=1;
  info->last_keypage=page;
  uint32_t page_size=mi_getint(tmp);
  if (page_size < 4 || page_size > keyinfo->block_length)
  {
    info->last_keypage = HA_OFFSET_ERROR;
    mi_print_error(info->s, HA_ERR_CRASHED);
    errno = HA_ERR_CRASHED;
    tmp = 0;
  }
  return(tmp);
} /* _mi_fetch_keypage */


	/* Write a key-page on disk */

int _mi_write_keypage(MI_INFO *info, MI_KEYDEF *keyinfo,
		      internal::my_off_t page, int, unsigned char *buff)
{
  uint32_t length;

#ifndef FAST					/* Safety check */
  if (page < info->s->base.keystart ||
      page+keyinfo->block_length > info->state->key_file_length ||
      (page & (MI_MIN_KEY_BLOCK_LENGTH-1)))
  {
    errno=EINVAL;
    return((-1));
  }
#endif

  if ((length=keyinfo->block_length) > IO_SIZE*2 &&
      info->state->key_file_length != page+length)
    length= ((mi_getint(buff)+IO_SIZE-1) & (uint) ~(IO_SIZE-1));
#ifdef HAVE_VALGRIND
  {
    length=mi_getint(buff);
    memset(buff+length, 0, keyinfo->block_length-length);
    length=keyinfo->block_length;
  }
#endif
  return not pwrite(info->s->kfile, buff, length, page);
} /* mi_write_keypage */


	/* Remove page from disk */

int _mi_dispose(MI_INFO *info, MI_KEYDEF *keyinfo, internal::my_off_t pos, int)
{
  internal::my_off_t old_link;
  unsigned char buff[8];

  old_link= info->s->state.key_del[keyinfo->block_size_index];
  info->s->state.key_del[keyinfo->block_size_index]= pos;
  mi_sizestore(buff,old_link);
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  return not pwrite(info->s->kfile, buff, sizeof(buff), pos);
} /* _mi_dispose */


	/* Make new page on disk */

internal::my_off_t _mi_new(MI_INFO *info, MI_KEYDEF *keyinfo, int)
{
  internal::my_off_t pos;
  unsigned char buff[8];

  if ((pos= info->s->state.key_del[keyinfo->block_size_index]) ==
      HA_OFFSET_ERROR)
  {
    if (info->state->key_file_length >=
	info->s->base.max_key_file_length - keyinfo->block_length)
    {
      errno=HA_ERR_INDEX_FILE_FULL;
      return(HA_OFFSET_ERROR);
    }
    pos=info->state->key_file_length;
    info->state->key_file_length+= keyinfo->block_length;
  }
  else if (pread(info->s->kfile, buff, sizeof(buff), pos))
    info->s->state.key_del[keyinfo->block_size_index]= mi_sizekorr(buff);
  else
    pos= HA_OFFSET_ERROR;
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  return(pos);
} /* _mi_new */
