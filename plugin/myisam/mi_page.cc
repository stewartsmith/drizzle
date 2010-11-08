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

unsigned char *_mi_fetch_keypage(register MI_INFO *info, MI_KEYDEF *keyinfo,
			 internal::my_off_t page, int level,
                         unsigned char *buff, int return_buffer)
{
  unsigned char *tmp;
  uint32_t page_size;

  tmp=(unsigned char*) key_cache_read(info->s->getKeyCache(),
                             info->s->kfile, page, level, (unsigned char*) buff,
			     (uint) keyinfo->block_length,
			     (uint) keyinfo->block_length,
			     return_buffer);
  if (tmp == info->buff)
    info->buff_used=1;
  else if (!tmp)
  {
    info->last_keypage=HA_OFFSET_ERROR;
    mi_print_error(info->s, HA_ERR_CRASHED);
    errno=HA_ERR_CRASHED;
    return(0);
  }
  info->last_keypage=page;
  page_size=mi_getint(tmp);
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

int _mi_write_keypage(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		      internal::my_off_t page, int level, unsigned char *buff)
{
  register uint32_t length;

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
  return((key_cache_write(info->s->getKeyCache(),
                         info->s->kfile,page, level, (unsigned char*) buff,length,
			 (uint) keyinfo->block_length,
			 (int) ((info->lock_type != F_UNLCK) ||
				info->s->delay_key_write))));
} /* mi_write_keypage */


	/* Remove page from disk */

int _mi_dispose(register MI_INFO *info, MI_KEYDEF *keyinfo, internal::my_off_t pos,
                int level)
{
  internal::my_off_t old_link;
  unsigned char buff[8];

  old_link= info->s->state.key_del[keyinfo->block_size_index];
  info->s->state.key_del[keyinfo->block_size_index]= pos;
  mi_sizestore(buff,old_link);
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  return(key_cache_write(info->s->getKeyCache(),
                              info->s->kfile, pos , level, buff,
			      sizeof(buff),
			      (uint) keyinfo->block_length,
			      (int) (info->lock_type != F_UNLCK)));
} /* _mi_dispose */


	/* Make new page on disk */

internal::my_off_t _mi_new(register MI_INFO *info, MI_KEYDEF *keyinfo, int level)
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
  else
  {
    if (!key_cache_read(info->s->getKeyCache(),
                        info->s->kfile, pos, level,
			buff,
			(uint) sizeof(buff),
			(uint) keyinfo->block_length,0))
      pos= HA_OFFSET_ERROR;
    else
      info->s->state.key_del[keyinfo->block_size_index]= mi_sizekorr(buff);
  }
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  return(pos);
} /* _mi_new */
