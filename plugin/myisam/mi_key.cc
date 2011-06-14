/* Copyright (C) 2000-2006 MySQL AB

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

/* Functions to handle keys */

#include "myisam_priv.h"
#include <drizzled/charset.h>
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif
#include <math.h>
#include <cassert>

using namespace drizzled;
using namespace std;

#define CHECK_KEYS                              /* Enable safety checks */

#define FIX_LENGTH(cs, pos, length, char_length)                            \
            do {                                                            \
              if (length > char_length)                                     \
                char_length= my_charpos(cs, pos, pos+length, char_length);  \
              drizzled::set_if_smaller(char_length,length);                           \
            } while(0)

static int _mi_put_key_in_record(MI_INFO *info,uint32_t keynr,unsigned char *record);

/*
  Make a intern key from a record

  SYNOPSIS
    _mi_make_key()
    info		MyiSAM handler
    keynr		key number
    key			Store created key here
    record		Record
    filepos		Position to record in the data file

  RETURN
    Length of key
*/

uint32_t _mi_make_key(register MI_INFO *info, uint32_t keynr, unsigned char *key,
                      const unsigned char *record, drizzled::internal::my_off_t filepos)
{
  unsigned char *pos;
  unsigned char *start;
  register HA_KEYSEG *keyseg;

  start=key;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->type ;keyseg++)
  {
    enum drizzled::ha_base_keytype type=(enum drizzled::ha_base_keytype) keyseg->type;
    uint32_t length=keyseg->length;
    uint32_t char_length;
    const drizzled::charset_info_st * const cs=keyseg->charset;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
      {
	*key++= 0;				/* NULL in key */
	continue;
      }
      *key++=1;					/* Not NULL */
    }

    char_length= ((cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen :
                  length);

    pos= (unsigned char*) record+keyseg->start;

    if (keyseg->flag & HA_SPACE_PACK)
    {
      length= cs->cset->lengthsp(cs, (char*) pos, length);

      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, pos, char_length);
      key+=char_length;
      continue;
    }
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint32_t pack_length= (keyseg->bit_start == 1 ? 1 : 2);
      uint32_t tmp_length= (pack_length == 1 ? (uint) *(unsigned char*) pos :
                        uint2korr(pos));
      pos+= pack_length;			/* Skip VARCHAR length */
      drizzled::set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, pos, char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint32_t tmp_length=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy(&pos, pos+keyseg->bit_start, sizeof(char*));
      drizzled::set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, pos, char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
      if (type == drizzled::HA_KEYTYPE_DOUBLE)
      {
	double nr;
	float8get(nr,pos);
	if (isnan(nr))
	{
	  memset(key, 0, length);
	  key+=length;
	  continue;
	}
      }
      pos+=length;
      while (length--)
      {
	*key++ = *--pos;
      }
      continue;
    }
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy(key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  _mi_dpointer(info,key,filepos);
  return((uint) (key-start));		/* Return keylength */
} /* _mi_make_key */


/*
  Pack a key to intern format from given format (c_rkey)

  SYNOPSIS
    _mi_pack_key()
    info		MyISAM handler
    uint32_t keynr		key number
    key			Store packed key here
    old			Not packed key
    keypart_map         bitmap of used keyparts
    last_used_keyseg	out parameter.  May be NULL

   RETURN
     length of packed key

     last_use_keyseg    Store pointer to the keyseg after the last used one
*/

uint32_t _mi_pack_key(register MI_INFO *info, uint32_t keynr, unsigned char *key, unsigned char *old,
                      drizzled::key_part_map keypart_map, HA_KEYSEG **last_used_keyseg)
{
  unsigned char *start_key=key;
  HA_KEYSEG *keyseg;

  /* only key prefixes are supported */
  assert(((keypart_map+1) & keypart_map) == 0);

  for (keyseg= info->s->keyinfo[keynr].seg ; keyseg->type && keypart_map;
       old+= keyseg->length, keyseg++)
  {
    enum drizzled::ha_base_keytype type= (enum drizzled::ha_base_keytype) keyseg->type;
    uint32_t length= keyseg->length;
    uint32_t char_length;
    unsigned char *pos;
    const drizzled::charset_info_st * const cs=keyseg->charset;
    keypart_map>>= 1;
    if (keyseg->null_bit)
    {
      if (!(*key++= (char) 1-*old++))			/* Copy null marker */
      {
        if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
          old+= 2;
	continue;					/* Found NULL */
      }
    }
    char_length= (cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen : length;
    pos=old;
    if (keyseg->flag & HA_SPACE_PACK)
    {
      unsigned char *end=pos+length;

      if (type != drizzled::HA_KEYTYPE_BINARY)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      length=(uint) (end-pos);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, pos, char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
    {
      /* Length of key-part used with mi_rkey() always 2 */
      uint32_t tmp_length=uint2korr(pos);
      pos+=2;
      drizzled::set_if_smaller(length,tmp_length);	/* Safety */
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      old+=2;					/* Skip length */
      memcpy(key, pos, char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
      pos+=length;
      while (length--)
	*key++ = *--pos;
      continue;
    }
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy(key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  if (last_used_keyseg)
    *last_used_keyseg= keyseg;

  return((uint) (key-start_key));
} /* _mi_pack_key */



/*
  Store found key in record

  SYNOPSIS
    _mi_put_key_in_record()
    info		MyISAM handler
    keynr		Key number that was used
    record 		Store key here

    Last read key is in info->lastkey

 NOTES
   Used when only-keyread is wanted

 RETURN
   0   ok
   1   error
*/

static int _mi_put_key_in_record(register MI_INFO *info, uint32_t keynr,
				 unsigned char *record)
{
  register unsigned char *key;
  unsigned char *pos,*key_end;
  register HA_KEYSEG *keyseg;
  unsigned char *blob_ptr;

  blob_ptr= (unsigned char*) info->lastkey2;             /* Place to put blob parts */
  key=(unsigned char*) info->lastkey;                    /* KEy that was read */
  key_end=key+info->lastkey_length;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->null_bit)
    {
      if (!*key++)
      {
	record[keyseg->null_pos]|= keyseg->null_bit;
	continue;
      }
      record[keyseg->null_pos]&= ~keyseg->null_bit;
    }

    if (keyseg->flag & HA_SPACE_PACK)
    {
      uint32_t length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      pos= record+keyseg->start;

      memcpy(pos, key, length);
      keyseg->charset->cset->fill(keyseg->charset,
                                  (char*) pos + length,
                                  keyseg->length - length,
                                  ' ');
      key+=length;
      continue;
    }

    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint32_t length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      /* Store key length */
      if (keyseg->bit_start == 1)
        *(unsigned char*) (record+keyseg->start)= (unsigned char) length;
      else
        int2store(record+keyseg->start, length);
      /* And key data */
      memcpy(record+keyseg->start + keyseg->bit_start, key, length);
      key+= length;
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint32_t length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      memcpy(record+keyseg->start+keyseg->bit_start,
	     &blob_ptr,sizeof(char*));
      memcpy(blob_ptr,key,length);
      blob_ptr+=length;

      /* The above changed info->lastkey2. Inform mi_rnext_same(). */
      info->update&= ~HA_STATE_RNEXT_SAME;

      _my_store_blob_length(record+keyseg->start,
			    (uint) keyseg->bit_start,length);
      key+=length;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {
      unsigned char *to=  record+keyseg->start+keyseg->length;
      unsigned char *end= key+keyseg->length;
#ifdef CHECK_KEYS
      if (end > key_end)
	goto err;
#endif
      do
      {
	 *--to= *key++;
      } while (key != end);
      continue;
    }
    else
    {
#ifdef CHECK_KEYS
      if (key+keyseg->length > key_end)
	goto err;
#endif
      memcpy(record+keyseg->start, key, keyseg->length);
      key+= keyseg->length;
    }
  }
  return(0);

err:
  return(1);				/* Crashed row */
} /* _mi_put_key_in_record */


	/* Here when key reads are used */

int _mi_read_key_record(MI_INFO *info, drizzled::internal::my_off_t filepos, unsigned char *buf)
{
  fast_mi_writeinfo(info);
  if (filepos != HA_OFFSET_ERROR)
  {
    if (info->lastinx >= 0)
    {				/* Read only key */
      if (_mi_put_key_in_record(info,(uint) info->lastinx,buf))
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
	errno=HA_ERR_CRASHED;
	return -1;
      }
      info->update|= HA_STATE_AKTIV; /* We should find a record */
      return 0;
    }
    errno=HA_ERR_WRONG_INDEX;
  }
  return(-1);				/* Wrong data to read */
}


/*
  Save current key tuple to record and call index condition check function

  SYNOPSIS
    mi_check_index_cond()
      info    MyISAM handler
      keynr   Index we're running a scan on
      record  Record buffer to use (it is assumed that index check function
              will look for column values there)

  RETURN
    -1  Error
    0   Index condition is not satisfied, continue scanning
    1   Index condition is satisfied
    2   Index condition is not satisfied, end the scan.
*/

int mi_check_index_cond(register MI_INFO *info, uint32_t keynr, unsigned char *record)
{
  if (_mi_put_key_in_record(info, keynr, record))
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    errno=HA_ERR_CRASHED;
    return -1;
  }
  return info->index_cond_func(info->index_cond_func_arg);
}


/*
  Retrieve auto_increment info

  SYNOPSIS
    retrieve_auto_increment()
    info			MyISAM handler
    record			Row to update

  IMPLEMENTATION
    For signed columns we don't retrieve the auto increment value if it's
    less than zero.
*/

uint64_t retrieve_auto_increment(MI_INFO *info,const unsigned char *record)
{
  uint64_t value= 0;			/* Store unsigned values here */
  int64_t s_value= 0;			/* Store signed values here */
  HA_KEYSEG *keyseg= info->s->keyinfo[info->s->base.auto_key-1].seg;
  const unsigned char *key= (unsigned char*) record + keyseg->start;

  switch (keyseg->type) {
  case drizzled::HA_KEYTYPE_BINARY:
    value=(uint64_t)  *(unsigned char*) key;
    break;
  case drizzled::HA_KEYTYPE_LONG_INT:
    s_value= (int64_t) sint4korr(key);
    break;
  case drizzled::HA_KEYTYPE_ULONG_INT:
    value=(uint64_t) uint4korr(key);
    break;
  case drizzled::HA_KEYTYPE_DOUBLE:                       /* This shouldn't be used */
  {
    double f_1;
    float8get(f_1,key);
    /* Ignore negative values */
    value = (f_1 < 0.0) ? 0 : (uint64_t) f_1;
    break;
  }
  case drizzled::HA_KEYTYPE_LONGLONG:
    s_value= sint8korr(key);
    break;
  case drizzled::HA_KEYTYPE_ULONGLONG:
    value= uint8korr(key);
    break;
  default:
    assert(0);
    value=0;                                    /* Error */
    break;
  }

  /*
    The following code works becasue if s_value < 0 then value is 0
    and if s_value == 0 then value will contain either s_value or the
    correct value.
  */
  return (s_value > 0) ? (uint64_t) s_value : value;
}
