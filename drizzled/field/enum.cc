/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <drizzled/server_includes.h>
#include <drizzled/field/enum.h>
#include <drizzled/error.h>

/****************************************************************************
** enum type.
** This is a string which only can have a selection of different values.
** If one uses this string in a number context one gets the type number.
****************************************************************************/

enum ha_base_keytype Field_enum::key_type() const
{
  switch (packlength) {
  default: return HA_KEYTYPE_BINARY;
  case 2: assert(1);
  case 3: assert(1);
  case 4: return HA_KEYTYPE_ULONG_INT;
  case 8: return HA_KEYTYPE_ULONGLONG;
  }
}

void Field_enum::store_type(uint64_t value)
{
  switch (packlength) {
  case 1: ptr[0]= (unsigned char) value;  break;
  case 2:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,(unsigned short) value);
  }
  else
#endif
    shortstore(ptr,(unsigned short) value);
  break;
  case 3: int3store(ptr,(long) value); break;
  case 4:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int4store(ptr,value);
  }
  else
#endif
    longstore(ptr,(long) value);
  break;
  case 8:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int8store(ptr,value);
  }
  else
#endif
    int64_tstore(ptr,value); break;
  }
}


/**
  @note
    Storing a empty string in a enum field gives a warning
    (if there isn't a empty value in the enum)
*/

int Field_enum::store(const char *from, uint32_t length, const CHARSET_INFO * const cs)
{
  int err= 0;
  uint32_t not_used;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion(length, cs, field_charset, &not_used))
  { 
    uint32_t dummy_errors;
    tmpstr.copy(from, length, cs, field_charset, &dummy_errors);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }

  /* Remove end space */
  length= field_charset->cset->lengthsp(field_charset, from, length);
  uint32_t tmp=find_type2(typelib, from, length, field_charset);
  if (!tmp)
  {
    if (length < 6) // Can't be more than 99999 enums
    {
      /* This is for reading numbers with LOAD DATA INFILE */
      char *end;
      tmp=(uint) my_strntoul(cs,from,length,10,&end,&err);
      if (err || end != from+length || tmp > typelib->count)
      {
	tmp=0;
	set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
      }
      if (!table->in_use->count_cuted_fields)
        err= 0;
    }
    else
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
  }
  store_type((uint64_t) tmp);
  return err;
}


int Field_enum::store(double nr)
{
  return Field_enum::store((int64_t) nr, false);
}


int Field_enum::store(int64_t nr,
                      bool unsigned_val __attribute__((unused)))
{
  int error= 0;
  if ((uint64_t) nr > typelib->count || nr == 0)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    if (nr != 0 || table->in_use->count_cuted_fields)
    {
      nr= 0;
      error= 1;
    }
  }
  store_type((uint64_t) (uint) nr);
  return error;
}


double Field_enum::val_real(void)
{
  return (double) Field_enum::val_int();
}


int64_t Field_enum::val_int(void)
{
  switch (packlength) {
  case 1:
    return (int64_t) ptr[0];
  case 2:
  {
    uint16_t tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=sint2korr(ptr);
    else
#endif
      shortget(tmp,ptr);
    return (int64_t) tmp;
  }
  case 3:
    return (int64_t) uint3korr(ptr);
  case 4:
  {
    uint32_t tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=uint4korr(ptr);
    else
#endif
      longget(tmp,ptr);
    return (int64_t) tmp;
  }
  case 8:
  {
    int64_t tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=sint8korr(ptr);
    else
#endif
      int64_tget(tmp,ptr);
    return tmp;
  }
  }
  return 0;					// impossible
}


/**
   Save the field metadata for enum fields.

   Saves the real type in the first byte and the pack length in the 
   second byte of the field metadata array at index of *metadata_ptr and
   *(metadata_ptr + 1).

   @param   metadata_ptr   First byte of field metadata

   @returns number of bytes written to metadata_ptr
*/
int Field_enum::do_save_field_metadata(unsigned char *metadata_ptr)
{
  *metadata_ptr= real_type();
  *(metadata_ptr + 1)= pack_length();
  return 2;
}


String *Field_enum::val_str(String *val_buffer __attribute__((unused)),
			    String *val_ptr)
{
  uint32_t tmp=(uint) Field_enum::val_int();
  if (!tmp || tmp > typelib->count)
    val_ptr->set("", 0, field_charset);
  else
    val_ptr->set((const char*) typelib->type_names[tmp-1],
		 typelib->type_lengths[tmp-1],
		 field_charset);
  return val_ptr;
}

int Field_enum::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  unsigned char *old= ptr;
  ptr= (unsigned char*) a_ptr;
  uint64_t a=Field_enum::val_int();
  ptr= (unsigned char*) b_ptr;
  uint64_t b=Field_enum::val_int();
  ptr= old;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_enum::sort_string(unsigned char *to,uint32_t length __attribute__((unused)))
{
  uint64_t value=Field_enum::val_int();
  to+=packlength-1;
  for (uint32_t i=0 ; i < packlength ; i++)
  {
    *to-- = (unsigned char) (value & 255);
    value>>=8;
  }
}


void Field_enum::sql_type(String &res) const
{
  char buffer[255];
  String enum_item(buffer, sizeof(buffer), res.charset());

  res.length(0);
  res.append(STRING_WITH_LEN("enum("));

  bool flag=0;
  uint32_t *len= typelib->type_lengths;
  for (const char **pos= typelib->type_names; *pos; pos++, len++)
  {
    uint32_t dummy_errors;
    if (flag)
      res.append(',');
    /* convert to res.charset() == utf8, then quote */
    enum_item.copy(*pos, *len, charset(), res.charset(), &dummy_errors);
    append_unescaped(&res, enum_item.ptr(), enum_item.length());
    flag= 1;
  }
  res.append(')');
}


Field *Field_enum::new_field(MEM_ROOT *root, Table *new_table,
                             bool keep_type)
{
  Field_enum *res= (Field_enum*) Field::new_field(root, new_table, keep_type);
  if (res)
    res->typelib= copy_typelib(root, typelib);
  return res;
}
