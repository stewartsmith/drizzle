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

/* Functions to check if a row is unique */

#include "myisam_priv.h"
#include <drizzled/charset.h>

using namespace drizzled;

bool mi_check_unique(MI_INFO *info,
                     MI_UNIQUEDEF *def,
                     unsigned char *record,
                     ha_checksum unique_hash,
                     internal::my_off_t disk_pos)
{
  internal::my_off_t lastpos=info->lastpos;
  MI_KEYDEF *key= &info->s->keyinfo[def->key];
  unsigned char *key_buff=info->lastkey2;

  mi_unique_store(record+key->seg->start, unique_hash);
  _mi_make_key(info,def->key,key_buff,record,0);

  /* The above changed info->lastkey2. Inform mi_rnext_same(). */
  info->update&= ~HA_STATE_RNEXT_SAME;

  if (_mi_search(info,info->s->keyinfo+def->key,key_buff,MI_UNIQUE_HASH_LENGTH,
		 SEARCH_FIND,info->s->state.key_root[def->key]))
  {
    info->page_changed=1;			/* Can't optimize read next */
    info->lastpos= lastpos;
    return(0);				/* No matching rows */
  }

  for (;;)
  {
    if (info->lastpos != disk_pos &&
	!(*info->s->compare_unique)(info,def,record,info->lastpos))
    {
      errno=HA_ERR_FOUND_DUPP_UNIQUE;
      info->errkey= (int) def->key;
      info->dupp_key_pos= info->lastpos;
      info->page_changed=1;			/* Can't optimize read next */
      info->lastpos=lastpos;
      return(1);				/* Found identical  */
    }
    if (_mi_search_next(info,info->s->keyinfo+def->key, info->lastkey,
			MI_UNIQUE_HASH_LENGTH, SEARCH_BIGGER,
			info->s->state.key_root[def->key]) ||
	memcmp(info->lastkey, key_buff, MI_UNIQUE_HASH_LENGTH))
    {
      info->page_changed=1;			/* Can't optimize read next */
      info->lastpos=lastpos;
      return(0);				/* end of tree */
    }
  }
}


/*
  Calculate a hash for a row

  TODO
    Add support for bit fields
*/

ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const unsigned char *record)
{
  const unsigned char *pos, *end;
  ha_checksum crc= 0;
  uint32_t seed1=0, seed2= 4;
  HA_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint32_t length=keyseg->length;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
      {
	/*
	  Change crc in a way different from an empty string or 0.
	  (This is an optimisation;  The code will work even if this isn't
	  done)
	*/
	crc=((crc << 8) + 511+
	     (crc >> (8*sizeof(ha_checksum)-8)));
	continue;
      }
    }
    pos= record+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint32_t pack_length=  keyseg->bit_start;
      uint32_t tmp_length= (pack_length == 1 ? (uint) *(unsigned char*) pos :
                        uint2korr(pos));
      pos+= pack_length;			/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint32_t tmp_length=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy(&pos,pos+keyseg->bit_start,sizeof(char*));
      if (!length || length > tmp_length)
	length=tmp_length;			/* The whole blob */
    }
    end= pos+length;
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2)
    {
      keyseg->charset->coll->hash_sort(keyseg->charset,
                                       (const unsigned char*) pos, length, &seed1,
                                       &seed2);
      crc^= seed1;
    }
    else
      while (pos != end)
	crc=((crc << 8) +
	     (((unsigned char)  *(unsigned char*) pos++))) +
	  (crc >> (8*sizeof(ha_checksum)-8));
  }
  return crc;
}


/*
  compare unique key for two rows

  TODO
    Add support for bit fields

  RETURN
    0   if both rows have equal unique value
    #   Rows are different
*/

int mi_unique_comp(MI_UNIQUEDEF *def, const unsigned char *a, const unsigned char *b,
		   bool null_are_equal)
{
  const unsigned char *pos_a, *pos_b, *end;
  HA_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint16_t a_length, b_length;
    a_length= b_length= keyseg->length;

    /* If part is NULL it's regarded as different */
    if (keyseg->null_bit)
    {
      uint32_t tmp;
      if ((tmp=(a[keyseg->null_pos] & keyseg->null_bit)) !=
	  (uint) (b[keyseg->null_pos] & keyseg->null_bit))
	return 1;
      if (tmp)
      {
	if (!null_are_equal)
	  return 1;
	continue;
      }
    }
    pos_a= a+keyseg->start;
    pos_b= b+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint32_t pack_length= keyseg->bit_start;
      if (pack_length == 1)
      {
        a_length= (uint) *(unsigned char*) pos_a++;
        b_length= (uint) *(unsigned char*) pos_b++;
      }
      else
      {
        a_length= uint2korr(pos_a);
        b_length= uint2korr(pos_b);
        pos_a+= 2;				/* Skip VARCHAR length */
        pos_b+= 2;
      }
      set_if_smaller(a_length, keyseg->length); /* Safety */
      set_if_smaller(b_length, keyseg->length); /* safety */
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      /* Only compare 'length' characters if length != 0 */
      a_length= _mi_calc_blob_length(keyseg->bit_start,pos_a);
      b_length= _mi_calc_blob_length(keyseg->bit_start,pos_b);
      /* Check that a and b are of equal length */
      if (keyseg->length)
      {
        /*
          This is used in some cases when we are not interested in comparing
          the whole length of the blob.
        */
        set_if_smaller(a_length, keyseg->length);
        set_if_smaller(b_length, keyseg->length);
      }
      memcpy(&pos_a,pos_a+keyseg->bit_start,sizeof(char*));
      memcpy(&pos_b,pos_b+keyseg->bit_start,sizeof(char*));
    }
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2)
    {
      if (ha_compare_text(keyseg->charset, (unsigned char *) pos_a, a_length,
                                           (unsigned char *) pos_b, b_length, 0, 1))
        return 1;
    }
    else
    {
      if (a_length != b_length)
        return 1;
      end= pos_a+a_length;
      while (pos_a != end)
      {
	if (*pos_a++ != *pos_b++)
	  return 1;
      }
    }
  }
  return 0;
}
