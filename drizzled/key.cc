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


/* Functions to handle keys and fields in forms */

#include <config.h>
#include <drizzled/table.h>
#include <drizzled/key.h>
#include <drizzled/field/blob.h>
#include <drizzled/util/test.h>
#include <drizzled/plugin/storage_engine.h>

#include <boost/dynamic_bitset.hpp>

#include <string>

#include <algorithm>

using namespace std;

namespace drizzled
{

/*
  Search after a key that starts with 'field'

  SYNOPSIS
    find_ref_key()
    key			First key to check
    key_count		How many keys to check
    record		Start of record
    field		Field to search after
    key_length		On partial match, contains length of fields before
			field
    keypart             key part # of a field

  NOTES
   Used when calculating key for NEXT_NUMBER

  IMPLEMENTATION
    If no key starts with field test if field is part of some key. If we find
    one, then return first key and set key_length to the number of bytes
    preceding 'field'.

  RETURN
   -1  field is not part of the key
   #   Key part for key matching key.
       key_length is set to length of key before (not including) field
*/

int find_ref_key(KeyInfo *key, uint32_t key_count, unsigned char *record, Field *field,
                 uint32_t *key_length, uint32_t *keypart)
{
  int i;
  KeyInfo *key_info;
  uint32_t fieldpos;

  fieldpos= field->offset(record);

  /* Test if some key starts as fieldpos */
  for (i= 0, key_info= key ;
       i < (int) key_count ;
       i++, key_info++)
  {
    if (key_info->key_part[0].offset == fieldpos)
    {                                  		/* Found key. Calc keylength */
      *key_length= *keypart= 0;
      return i;                                 /* Use this key */
    }
  }

  /* Test if some key contains fieldpos */
  for (i= 0, key_info= key;
       i < (int) key_count ;
       i++, key_info++)
  {
    uint32_t j;
    KeyPartInfo *key_part;
    *key_length=0;
    for (j=0, key_part=key_info->key_part ;
	 j < key_info->key_parts ;
	 j++, key_part++)
    {
      if (key_part->offset == fieldpos)
      {
        *keypart= j;
        return i;                               /* Use this key */
      }
      *key_length+= key_part->store_length;
    }
  }
  return(-1);					/* No key is ok */
}


void key_copy(unsigned char *to_key, unsigned char *from_record, KeyInfo *key_info,
              unsigned int key_length)
{
  uint32_t length;
  KeyPartInfo *key_part;

  if (key_length == 0)
    key_length= key_info->key_length;
  for (key_part= key_info->key_part; (int) key_length > 0; key_part++)
  {
    if (key_part->null_bit)
    {
      *to_key++= test(from_record[key_part->null_offset] &
		   key_part->null_bit);
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART ||
        key_part->key_part_flag & HA_VAR_LENGTH_PART)
    {
      key_length-= HA_KEY_BLOB_LENGTH;
      length= min((uint16_t)key_length, key_part->length);
      key_part->field->get_key_image(to_key, length);
      to_key+= HA_KEY_BLOB_LENGTH;
    }
    else
    {
      length= min((uint16_t)key_length, key_part->length);
      Field *field= key_part->field;
      const charset_info_st * const cs= field->charset();
      uint32_t bytes= field->get_key_image(to_key, length);
      if (bytes < length)
        cs->cset->fill(cs, (char*) to_key + bytes, length - bytes, ' ');
    }
    to_key+= length;
    key_length-= length;
  }
}


/**
  Zero the null components of key tuple.
*/

void key_zero_nulls(unsigned char *tuple, KeyInfo *key_info)
{
  KeyPartInfo *key_part= key_info->key_part;
  KeyPartInfo *key_part_end= key_part + key_info->key_parts;
  for (; key_part != key_part_end; key_part++)
  {
    if (key_part->null_bit && *tuple)
      memset(tuple+1, 0, key_part->store_length-1);
    tuple+= key_part->store_length;
  }
}


/*
  Restore a key from some buffer to record.

    This function converts a key into record format. It can be used in cases
    when we want to return a key as a result row.

  @param to_record   record buffer where the key will be restored to
  @param from_key    buffer that contains a key
  @param key_info    descriptor of the index
  @param key_length  specifies length of all keyparts that will be restored
*/

void key_restore(unsigned char *to_record, unsigned char *from_key, KeyInfo *key_info,
                 uint16_t key_length)
{
  uint32_t length;
  KeyPartInfo *key_part;

  if (key_length == 0)
  {
    key_length= key_info->key_length;
  }
  for (key_part= key_info->key_part ; (int) key_length > 0 ; key_part++)
  {
    unsigned char used_uneven_bits= 0;
    if (key_part->null_bit)
    {
      if (*from_key++)
	to_record[key_part->null_offset]|= key_part->null_bit;
      else
	to_record[key_part->null_offset]&= ~key_part->null_bit;
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART)
    {
      /*
        This in fact never happens, as we have only partial BLOB
        keys yet anyway, so it's difficult to find any sence to
        restore the part of a record.
        Maybe this branch is to be removed, but now we
        have to ignore GCov compaining.

        This may make more sense once we push down block lengths to the engine (aka partial retrieval).
      */
      uint32_t blob_length= uint2korr(from_key);
      Field_blob *field= (Field_blob*) key_part->field;

      field->setReadSet();
      from_key+= HA_KEY_BLOB_LENGTH;
      key_length-= HA_KEY_BLOB_LENGTH;
      field->set_ptr_offset(to_record - field->getTable()->getInsertRecord(),
                            (ulong) blob_length, from_key);
      length= key_part->length;
    }
    else if (key_part->key_part_flag & HA_VAR_LENGTH_PART)
    {
      Field *field= key_part->field;
      ptrdiff_t ptrdiff= to_record - field->getTable()->getInsertRecord();

      field->setReadSet();
      field->setWriteSet();
      field->move_field_offset(ptrdiff);
      key_length-= HA_KEY_BLOB_LENGTH;
      length= min(key_length, key_part->length);
      field->set_key_image(from_key, length);
      from_key+= HA_KEY_BLOB_LENGTH;
      field->move_field_offset(-ptrdiff);
    }
    else
    {
      length= min(key_length, key_part->length);
      /* skip the byte with 'uneven' bits, if used */
      memcpy(to_record + key_part->offset, from_key + used_uneven_bits
             , (size_t) length - used_uneven_bits);
    }
    from_key+= length;
    key_length-= length;
  }
}


/**
  Compare if a key has changed.

  @param table		Table
  @param key		key to compare to row
  @param idx		Index used
  @param key_length	Length of key

  @note
    In theory we could just call field->cmp() for all field types,
    but as we are only interested if a key has changed (not if the key is
    larger or smaller than the previous value) we can do things a bit
    faster by using memcmp() instead.

  @retval
    0	If key is equal
  @retval
    1	Key has changed
*/

bool key_cmp_if_same(Table *table,const unsigned char *key,uint32_t idx,uint32_t key_length)
{
  uint32_t store_length;
  KeyPartInfo *key_part;
  const unsigned char *key_end= key + key_length;;

  for (key_part=table->key_info[idx].key_part;
       key < key_end ;
       key_part++, key+= store_length)
  {
    uint32_t length;
    store_length= key_part->store_length;

    if (key_part->null_bit)
    {
      if (*key != test(table->getInsertRecord()[key_part->null_offset] &
		       key_part->null_bit))
	return 1;
      if (*key)
	continue;
      key++;
      store_length--;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART |
                                   HA_BIT_PART))
    {
      if (key_part->field->key_cmp(key, key_part->length))
	return 1;
      continue;
    }
    length= min((uint32_t) (key_end-key), store_length);
    if (key_part->field->type() == DRIZZLE_TYPE_VARCHAR)
    {
      const charset_info_st * const cs= key_part->field->charset();
      uint32_t char_length= key_part->length / cs->mbmaxlen;
      const unsigned char *pos= table->getInsertRecord() + key_part->offset;
      if (length > char_length)
      {
        char_length= my_charpos(cs, pos, pos + length, char_length);
        set_if_smaller(char_length, length);
      }
      if (cs->coll->strnncollsp(cs,
                                (const unsigned char*) key, length,
                                (const unsigned char*) pos, char_length, 0))
        return 1;
      continue;
    }
    if (memcmp(key,table->getInsertRecord()+key_part->offset,length))
      return 1;
  }
  return 0;
}

/*
  unpack key-fields from record to some buffer.

  This is used mainly to get a good error message.  We temporary
  change the column bitmap so that all columns are readable.

  @param
     to		Store value here in an easy to read form
  @param
     table	Table to use
  @param
     idx	Key number
*/

void key_unpack(String *to, const Table *table, uint32_t idx)
{
  KeyPartInfo *key_part,*key_part_end;
  Field *field;
  String tmp;

  to->length(0);
  for (key_part=table->key_info[idx].key_part,key_part_end=key_part+
	 table->key_info[idx].key_parts ;
       key_part < key_part_end;
       key_part++)
  {
    if (to->length())
      to->append('-');
    if (key_part->null_bit)
    {
      if (table->getInsertRecord()[key_part->null_offset] & key_part->null_bit)
      {
	to->append(STRING_WITH_LEN("NULL"));
	continue;
      }
    }
    if ((field= key_part->field))
    {
      const charset_info_st * const cs= field->charset();
      field->setReadSet();
      field->val_str_internal(&tmp);
      if (cs->mbmaxlen > 1 &&
          table->getField(key_part->fieldnr - 1)->field_length !=
          key_part->length)
      {
        /*
          Prefix key, multi-byte charset.
          For the columns of type CHAR(N), the above val_str()
          call will return exactly "key_part->length" bytes,
          which can break a multi-byte characters in the middle.
          Align, returning not more than "char_length" characters.
        */
        uint32_t charpos, char_length= key_part->length / cs->mbmaxlen;
        if ((charpos= my_charpos(cs, tmp.c_ptr(),
                                 tmp.c_ptr() + tmp.length(),
                                 char_length)) < key_part->length)
          tmp.length(charpos);
      }

      if (key_part->length < field->pack_length())
        tmp.length(min(tmp.length(), static_cast<size_t>(key_part->length)));
      to->append(tmp);
    }
    else
      to->append(STRING_WITH_LEN("???"));
  }
}


/*
  Check if key uses field that is marked in passed field bitmap.

  SYNOPSIS
    is_key_used()
      table   Table object with which keys and fields are associated.
      idx     Key to be checked.
      fields  Bitmap of fields to be checked.

  NOTE
    This function uses Table::tmp_set bitmap so the caller should care
    about saving/restoring its state if it also uses this bitmap.

  RETURN VALUE
    TRUE   Key uses field from bitmap
    FALSE  Otherwise
*/

bool is_key_used(Table *table, uint32_t idx, const boost::dynamic_bitset<>& fields)
{
  table->tmp_set.reset();
  table->mark_columns_used_by_index_no_reset(idx, table->tmp_set);
  if (table->tmp_set.is_subset_of(fields))
    return 1;

  /*
    If table handler has primary key as part of the index, check that primary
    key is not updated
  */
  if (idx != table->getShare()->getPrimaryKey() && table->getShare()->hasPrimaryKey() &&
      (table->cursor->getEngine()->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX)))
  {
    return is_key_used(table, table->getShare()->getPrimaryKey(), fields);
  }
  return 0;
}


/**
  Compare key in row to a given key.

  @param key_part		Key part handler
  @param key			Key to compare to value in table->getInsertRecord()
  @param key_length		length of 'key'

  @return
    The return value is SIGN(key_in_row - range_key):
    -   0		Key is equal to range or 'range' == 0 (no range)
    -  -1		Key is less than range
    -   1		Key is larger than range
*/

int key_cmp(KeyPartInfo *key_part, const unsigned char *key, uint32_t key_length)
{
  uint32_t store_length;

  for (const unsigned char *end=key + key_length;
       key < end;
       key+= store_length, key_part++)
  {
    int cmp;
    store_length= key_part->store_length;
    if (key_part->null_bit)
    {
      /* This key part allows null values; NULL is lower than everything */
      bool field_is_null= key_part->field->is_null();
      if (*key)                                 // If range key is null
      {
	/* the range is expecting a null value */
	if (!field_is_null)
	  return 1;                             // Found key is > range
        /* null -- exact match, go to next key part */
	continue;
      }
      else if (field_is_null)
	return -1;                              // NULL is less than any value
      key++;					// Skip null byte
      store_length--;
    }
    if ((cmp=key_part->field->key_cmp(key, key_part->length)) < 0)
      return -1;
    if (cmp > 0)
      return 1;
  }
  return 0;                                     // Keys are equal
}


} /* namespace drizzled */
