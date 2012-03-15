/* Copyright (C) 2002-2006 MySQL AB

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Place - Suite 330, Boston,
   MA 02110-1301, USA */

#pragma once

#include <drizzled/charset.h>
#include <plugin/myisam/myisampack.h>

/*
  There is a hard limit for the maximum number of keys as there are only
  8 bits in the index file header for the number of keys in a table.
  This means that 0..255 keys can exist for a table. The idea of
  HA_MAX_POSSIBLE_KEY is to ensure that one can use myisamchk & tools on
  a MyISAM table for which one has more keys than MyISAM is normally
  compiled for. If you don't have this, you will get a core dump when
  running myisamchk compiled for 128 keys on a table with 255 keys.
*/

#define HA_MAX_POSSIBLE_KEY         255         /* For myisamchk */
/*
  The following defines can be increased if necessary.
  But beware the dependency of MI_MAX_POSSIBLE_KEY_BUFF and HA_MAX_KEY_LENGTH.
*/

#define HA_MAX_KEY_LENGTH           1332        /* Max length in bytes */
#define HA_MAX_KEY_SEG              16          /* Max segments for key */

#define HA_MAX_POSSIBLE_KEY_BUFF    (HA_MAX_KEY_LENGTH + 24+ 6+6)
#define HA_MAX_KEY_BUFF  (HA_MAX_KEY_LENGTH+HA_MAX_KEY_SEG*6+8+8)

typedef struct st_HA_KEYSEG		/* Key-portion */
{
  const drizzled::charset_info_st *charset;
  uint32_t start;				/* Start of key in record */
  uint32_t null_pos;			/* position to NULL indicator */
  uint16_t bit_pos;                       /* Position to bit part */
  uint16_t flag;
  uint16_t length;			/* Keylength */
  uint8_t  type;				/* Type of key (for sort) */
  uint8_t  language;
  uint8_t  null_bit;			/* bitmask to test for NULL */
  uint8_t  bit_start,bit_end;		/* if bit field */
  uint8_t  bit_length;                    /* Length of bit part */
} HA_KEYSEG;

#define get_key_length(length,key) \
{ if (*(unsigned char*) (key) != 255) \
    length= (uint) *(unsigned char*) ((key)++); \
  else \
  { length= mi_uint2korr((key)+1); (key)+=3; } \
}

#define get_key_length_rdonly(length,key) \
{ if (*(unsigned char*) (key) != 255) \
    length= ((uint) *(unsigned char*) ((key))); \
  else \
  { length= mi_uint2korr((key)+1); } \
}

#define get_key_pack_length(length,length_pack,key) \
{ if (*(unsigned char*) (key) != 255) \
  { length= (uint) *(unsigned char*) ((key)++); length_pack= 1; }\
  else \
  { length=mi_uint2korr((key)+1); (key)+= 3; length_pack= 3; } \
}

#define store_key_length_inc(key,length) \
{ if ((length) < 255) \
  { *(key)++= (length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); (key)+=3; } \
}

#define size_to_store_key_length(length) ((length) < 255 ? 1 : 3)

#define get_rec_bits(bit_ptr, bit_ofs, bit_len) \
  (((((uint16_t) (bit_ptr)[1] << 8) | (uint16_t) (bit_ptr)[0]) >> (bit_ofs)) & \
   ((1 << (bit_len)) - 1))

#define set_rec_bits(bits, bit_ptr, bit_ofs, bit_len) \
{ \
  (bit_ptr)[0]= ((bit_ptr)[0] & ~(((1 << (bit_len)) - 1) << (bit_ofs))) | \
                ((bits) << (bit_ofs)); \
  if ((bit_ofs) + (bit_len) > 8) \
    (bit_ptr)[1]= ((bit_ptr)[1] & ~((1 << ((bit_len) - 8 + (bit_ofs))) - 1)) | \
                  ((bits) >> (8 - (bit_ofs))); \
}

#define clr_rec_bits(bit_ptr, bit_ofs, bit_len) \
  set_rec_bits(0, bit_ptr, bit_ofs, bit_len)

extern int ha_compare_text(const drizzled::charset_info_st * const, unsigned char *, uint, unsigned char *, uint, bool, bool);

extern HA_KEYSEG *ha_find_null(HA_KEYSEG *keyseg, unsigned char *a);
void my_handler_error_register(void);
extern int ha_key_cmp(HA_KEYSEG *keyseg, unsigned char *a,unsigned char *b,
                      uint32_t key_length,uint32_t nextflag,uint32_t *diff_length);

/*
  Inside an in-memory data record, memory pointers to pieces of the
  record (like BLOBs) are stored in their native byte order and in
  this amount of bytes.
*/
#define portable_sizeof_char_ptr 8

