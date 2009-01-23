/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include CSTDINT_H
#include <drizzled/session.h>
#include <drizzled/item/uint.h>
#include <drizzled/item/null.h>
#include <drizzled/item/float.h>
#include <drizzled/item/param.h>
#include CMATH_H
#include <drizzled/sql_string.h>

Item *Item_param::safe_charset_converter(const CHARSET_INFO * const tocs)
{
  if (const_item())
  {
    uint32_t cnv_errors;
    String *ostr= val_str(&cnvstr);
    cnvitem->str_value.copy(ostr->ptr(), ostr->length(),
                            ostr->charset(), tocs, &cnv_errors);
    if (cnv_errors)
       return NULL;
    cnvitem->str_value.mark_as_const();
    cnvitem->max_length= cnvitem->str_value.numchars() * tocs->mbmaxlen;
    return cnvitem;
  }
  return NULL;
}

/**
  Default function of Item_param::set_param_func, so in case
  of malformed packet the server won't SIGSEGV.
*/

static void
default_set_param_func(Item_param *param, unsigned char **, ulong)
{
  param->set_null();
}

Item_param::Item_param(uint32_t pos_in_query_arg) :
  state(NO_VALUE),
  item_result_type(STRING_RESULT),
  /* Don't pretend to be a literal unless value for this item is set. */
  item_type(PARAM_ITEM),
  param_type(DRIZZLE_TYPE_VARCHAR),
  pos_in_query(pos_in_query_arg),
  set_param_func(default_set_param_func),
  limit_clause_param(false)
{
  name= (char*) "?";
  /*
    Since we can't say whenever this item can be NULL or cannot be NULL
    before mysql_stmt_execute(), so we assuming that it can be NULL until
    value is set.
  */
  maybe_null= 1;
  cnvitem= new Item_string("", 0, &my_charset_bin, DERIVATION_COERCIBLE);
  cnvstr.set(cnvbuf, sizeof(cnvbuf), &my_charset_bin);
}

void Item_param::set_null()
{
  /* These are cleared after each execution by reset() method */
  null_value= 1;
  /*
    Because of NULL and string values we need to set max_length for each new
    placeholder value: user can submit NULL for any placeholder type, and
    string length can be different in each execution.
  */
  max_length= 0;
  decimals= 0;
  state= NULL_VALUE;
  item_type= Item::NULL_ITEM;
  return;
}

void Item_param::set_int(int64_t i, uint32_t max_length_arg)
{
  value.integer= (int64_t) i;
  state= INT_VALUE;
  max_length= max_length_arg;
  decimals= 0;
  maybe_null= 0;
  return;
}

void Item_param::set_double(double d)
{
  value.real= d;
  state= REAL_VALUE;
  max_length= DBL_DIG + 8;
  decimals= NOT_FIXED_DEC;
  maybe_null= 0;
  return;
}

/**
  Set decimal parameter value from string.

  @param str      character string
  @param length   string length

  @note
    As we use character strings to send decimal values in
    binary protocol, we use str2my_decimal to convert it to
    internal decimal value.
*/

void Item_param::set_decimal(char *str, ulong length)
{
  char *end;

  end= str+length;
  str2my_decimal((uint)E_DEC_FATAL_ERROR, str, &decimal_value, &end);
  state= DECIMAL_VALUE;
  decimals= decimal_value.frac;
  max_length= my_decimal_precision_to_length(decimal_value.precision(),
                                             decimals, unsigned_flag);
  maybe_null= 0;
  return;
}

/**
  Set parameter value from DRIZZLE_TIME value.

  @param tm              datetime value to set (time_type is ignored)
  @param type            type of datetime value
  @param max_length_arg  max length of datetime value as string

  @note
    If we value to be stored is not normalized, zero value will be stored
    instead and proper warning will be produced. This function relies on
    the fact that even wrong value sent over binary protocol fits into
    MAX_DATE_STRING_REP_LENGTH buffer.
*/
void Item_param::set_time(DRIZZLE_TIME *tm,
                          enum enum_drizzle_timestamp_type time_type,
                          uint32_t max_length_arg)
{
  value.time= *tm;
  value.time.time_type= time_type;

  if (value.time.year > 9999 || value.time.month > 12 ||
      value.time.day > 31 ||
      ((time_type != DRIZZLE_TIMESTAMP_TIME) && value.time.hour > 23) ||
      value.time.minute > 59 || value.time.second > 59)
  {
    char buff[MAX_DATE_STRING_REP_LENGTH];
    uint32_t length= my_TIME_to_str(&value.time, buff);
    make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 buff, length, time_type, 0);
    set_zero_time(&value.time, DRIZZLE_TIMESTAMP_ERROR);
  }

  state= TIME_VALUE;
  maybe_null= 0;
  max_length= max_length_arg;
  decimals= 0;
  return;
}


bool Item_param::set_str(const char *str, ulong length)
{
  /*
    Assign string with no conversion: data is converted only after it's
    been written to the binary log.
  */
  uint32_t dummy_errors;
  if (str_value.copy(str, length, &my_charset_bin, &my_charset_bin,
                     &dummy_errors))
    return(true);
  state= STRING_VALUE;
  max_length= length;
  maybe_null= 0;
  /* max_length and decimals are set after charset conversion */
  /* sic: str may be not null-terminated */
  return(false);
}

bool Item_param::set_longdata(const char *str, ulong length)
{
  /*
    If client character set is multibyte, end of long data packet
    may hit at the middle of a multibyte character.  Additionally,
    if binary log is open we must write long data value to the
    binary log in character set of client. This is why we can't
    convert long data to connection character set as it comes
    (here), and first have to concatenate all pieces together,
    write query to the binary log and only then perform conversion.
  */
  if (str_value.append(str, length, &my_charset_bin))
    return(true);
  state= LONG_DATA_VALUE;
  maybe_null= 0;

  return(false);
}


/**
  Set parameter value from user variable value.

  @param session   Current thread
  @param entry User variable structure (NULL means use NULL value)

  @retval
    0 OK
  @retval
    1 Out of memory
*/

bool Item_param::set_from_user_var(Session *session, const user_var_entry *entry)
{
  if (entry && entry->value)
  {
    item_result_type= entry->type;
    unsigned_flag= entry->unsigned_flag;
    if (limit_clause_param)
    {
      bool unused;
      set_int(entry->val_int(&unused), MY_INT64_NUM_DECIMAL_DIGITS); item_type= Item::INT_ITEM;
      return(!unsigned_flag && value.integer < 0 ? 1 : 0);
    }
    switch (item_result_type) {
    case REAL_RESULT:
      set_double(*(double*)entry->value);
      item_type= Item::REAL_ITEM;
      break;
    case INT_RESULT:
      set_int(*(int64_t*)entry->value, MY_INT64_NUM_DECIMAL_DIGITS);
      item_type= Item::INT_ITEM;
      break;
    case STRING_RESULT:
    {
      const CHARSET_INFO * const fromcs= entry->collation.collation;
      const CHARSET_INFO * const tocs= session->variables.getCollation();
      uint32_t dummy_offset;

      value.cs_info.character_set_of_placeholder=
        value.cs_info.character_set_client= fromcs;
      /*
        Setup source and destination character sets so that they
        are different only if conversion is necessary: this will
        make later checks easier.
      */
      value.cs_info.final_character_set_of_str_value=
        String::needs_conversion(0, fromcs, tocs, &dummy_offset) ?
        tocs : fromcs;
      /*
        Exact value of max_length is not known unless data is converted to
        charset of connection, so we have to set it later.
      */
      item_type= Item::STRING_ITEM;

      if (set_str((const char *)entry->value, entry->length))
        return(1);
      break;
    }
    case DECIMAL_RESULT:
    {
      const my_decimal *ent_value= (const my_decimal *)entry->value;
      my_decimal2decimal(ent_value, &decimal_value);
      state= DECIMAL_VALUE;
      decimals= ent_value->frac;
      max_length= my_decimal_precision_to_length(ent_value->precision(),
                                                 decimals, unsigned_flag);
      item_type= Item::DECIMAL_ITEM;
      break;
    }
    default:
      assert(0);
      set_null();
    }
  }
  else
    set_null();

  return(0);
}

/**
  Resets parameter after execution.

  @note
    We clear null_value here instead of setting it in set_* methods,
    because we want more easily handle case for long data.
*/

void Item_param::reset()
{
  /* Shrink string buffer if it's bigger than max possible CHAR column */
  if (str_value.alloced_length() > MAX_CHAR_WIDTH)
    str_value.free();
  else
    str_value.length(0);
  str_value_ptr.length(0);
  /*
    We must prevent all charset conversions until data has been written
    to the binary log.
  */
  str_value.set_charset(&my_charset_bin);
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  state= NO_VALUE;
  maybe_null= 1;
  null_value= 0;
  /*
    Don't reset item_type to PARAM_ITEM: it's only needed to guard
    us from item optimizations at prepare stage, when item doesn't yet
    contain a literal of some kind.
    In all other cases when this object is accessed its value is
    set (this assumption is guarded by 'state' and
    assertS(state != NO_VALUE) in all Item_param::get_*
    methods).
  */
  return;
}

int Item_param::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();

  switch (state) {
  case INT_VALUE:
    return field->store(value.integer, unsigned_flag);
  case REAL_VALUE:
    return field->store(value.real);
  case DECIMAL_VALUE:
    return field->store_decimal(&decimal_value);
  case TIME_VALUE:
    field->store_time(&value.time, value.time.time_type);
    return 0;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return field->store(str_value.ptr(), str_value.length(),
                        str_value.charset());
  case NULL_VALUE:
    return set_field_to_null_with_conversions(field, no_conversions);
  case NO_VALUE:
  default:
    assert(0);
  }
  return 1;
}

bool Item_param::get_time(DRIZZLE_TIME *res)
{
  if (state == TIME_VALUE)
  {
    *res= value.time;
    return 0;
  }
  /*
    If parameter value isn't supplied assertion will fire in val_str()
    which is called from Item::get_time().
  */
  return Item::get_time(res);
}


bool Item_param::get_date(DRIZZLE_TIME *res, uint32_t fuzzydate)
{
  if (state == TIME_VALUE)
  {
    *res= value.time;
    return 0;
  }
  return Item::get_date(res, fuzzydate);
}


double Item_param::val_real()
{
  switch (state) {
  case REAL_VALUE:
    return value.real;
  case INT_VALUE:
    return (double) value.integer;
  case DECIMAL_VALUE:
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
    return result;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
  {
    int dummy_err;
    char *end_not_used;
    return my_strntod(str_value.charset(), (char*) str_value.ptr(),
                      str_value.length(), &end_not_used, &dummy_err);
  }
  case TIME_VALUE:
    /*
      This works for example when user says SELECT ?+0.0 and supplies
      time value for the placeholder.
    */
    return uint64_t2double(TIME_to_uint64_t(&value.time));
  case NULL_VALUE:
    return 0.0;
  default:
    assert(0);
  }
  return 0.0;
}


int64_t Item_param::val_int()
{
  switch (state) {
  case REAL_VALUE:
    return (int64_t) rint(value.real);
  case INT_VALUE:
    return value.integer;
  case DECIMAL_VALUE:
  {
    int64_t i;
    my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &i);
    return i;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      int dummy_err;
      return my_strntoll(str_value.charset(), str_value.ptr(),
                         str_value.length(), 10, (char**) 0, &dummy_err);
    }
  case TIME_VALUE:
    return (int64_t) TIME_to_uint64_t(&value.time);
  case NULL_VALUE:
    return 0;
  default:
    assert(0);
  }
  return 0;
}


my_decimal *Item_param::val_decimal(my_decimal *dec)
{
  switch (state) {
  case DECIMAL_VALUE:
    return &decimal_value;
  case REAL_VALUE:
    double2my_decimal(E_DEC_FATAL_ERROR, value.real, dec);
    return dec;
  case INT_VALUE:
    int2my_decimal(E_DEC_FATAL_ERROR, value.integer, unsigned_flag, dec);
    return dec;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    string2my_decimal(E_DEC_FATAL_ERROR, &str_value, dec);
    return dec;
  case TIME_VALUE:
  {
    int64_t i= (int64_t) TIME_to_uint64_t(&value.time);
    int2my_decimal(E_DEC_FATAL_ERROR, i, 0, dec);
    return dec;
  }
  case NULL_VALUE:
    return 0;
  default:
    assert(0);
  }
  return 0;
}


String *Item_param::val_str(String* str)
{
  switch (state) {
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return &str_value_ptr;
  case REAL_VALUE:
    str->set_real(value.real, NOT_FIXED_DEC, &my_charset_bin);
    return str;
  case INT_VALUE:
    str->set(value.integer, &my_charset_bin);
    return str;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) <= 1)
      return str;
    return NULL;
  case TIME_VALUE:
  {
    if (str->reserve(MAX_DATE_STRING_REP_LENGTH))
      break;
    str->length((uint) my_TIME_to_str(&value.time, (char*) str->ptr()));
    str->set_charset(&my_charset_bin);
    return str;
  }
  case NULL_VALUE:
    return NULL;
  default:
    assert(0);
  }
  return str;
}

/* TODO: fact next two functions out */
/**
    Transforms a string into "" or its expression in 0x... form.
*/

static char *str_to_hex(char *to, const char *from, uint32_t len)
{
  if (len)
  {
    *to++= '0';
    *to++= 'x';
    to= octet2hex(to, from, len);
  }
  else
    to= strcpy(to, "\"\"")+2;
  return to;                               // pointer to end 0 of 'to'
}


/* Borrowed from libicu header */

#define U8_IS_SINGLE(c) (((c)&0x80)==0)
#define U8_LENGTH(c) \
    ((uint32_t)(c)<=0x7f ? 1 : \
        ((uint32_t)(c)<=0x7ff ? 2 : \
            ((uint32_t)(c)<=0xd7ff ? 3 : \
                ((uint32_t)(c)<=0xdfff || (uint32_t)(c)>0x10ffff ? 0 : \
                    ((uint32_t)(c)<=0xffff ? 3 : 4)\
                ) \
            ) \
        ) \
    )

/*
  Add escape characters to a string (blob?) to make it suitable for a insert
  to should at least have place for length*2+1 chars
  Returns the length of the to string
*/

uint32_t
drizzle_escape_string(char *to,const char *from, uint32_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + 2*length;
  bool overflow= false;
  for (end= from + length; from < end; from++)
  {
    uint32_t tmp_length;
    char escape= 0;
    if (!U8_IS_SINGLE(*from))
    {
      tmp_length= U8_LENGTH(*from);
      if (to + tmp_length > to_end)
      {
        overflow= true;
        break;
      }
      while (tmp_length--)
        *to++= *from++;
      from--;
      continue;
    }
    switch (*from) {
    case 0:                             /* Must be escaped for 'mysql' */
      escape= '0';
      break;
    case '\n':                          /* Must be escaped for logs */
      escape= 'n';
      break;
    case '\r':
      escape= 'r';
      break;
    case '\\':
      escape= '\\';
      break;
    case '\'':
      escape= '\'';
      break;
    case '"':                           /* Better safe than sorry */
      escape= '"';
      break;
    case '\032':                        /* This gives problems on Win32 */
      escape= 'Z';
      break;
    }
    if (escape)
    {
      if (to + 2 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= '\\';
      *to++= escape;
    }
    else
    {
      if (to + 1 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= *from;
    }
  }
  *to= 0;
  return overflow ? (size_t) -1 : (size_t) (to - to_start);
}

/**
  Append a version of the 'from' string suitable for use in a query to
  the 'to' string.  To generate a correct escaping, the character set
  information in 'csinfo' is used.
*/

static int
append_query_string(const CHARSET_INFO * const csinfo,
                    String const *from, String *to)
{
  char *beg, *ptr;
  uint32_t const orig_len= to->length();
  if (to->reserve(orig_len + from->length()*2+3))
    return 1;

  beg= to->c_ptr_quick() + to->length();
  ptr= beg;
  if (csinfo->escape_with_backslash_is_dangerous)
    ptr= str_to_hex(ptr, from->ptr(), from->length());
  else
  {
    *ptr++= '\'';
    ptr+= drizzle_escape_string(ptr, from->ptr(), from->length());
    *ptr++='\'';
  }
  to->length(orig_len + ptr - beg);
  return 0;
}


/**
  Return Param item values in string format, for generating the dynamic
  query used in update/binary logs.

  @todo
    - Change interface and implementation to fill log data in place
    and avoid one more memcpy/alloc between str and log string.
    - In case of error we need to notify replication
    that binary log contains wrong statement
*/

const String *Item_param::query_val_str(String* str) const
{
  switch (state) {
  case INT_VALUE:
    str->set_int(value.integer, unsigned_flag, &my_charset_bin);
    break;
  case REAL_VALUE:
    str->set_real(value.real, NOT_FIXED_DEC, &my_charset_bin);
    break;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) > 1)
      return &my_null_string;
    break;
  case TIME_VALUE:
    {
      char *buf, *ptr;
      str->length(0);
      /*
        TODO: in case of error we need to notify replication
        that binary log contains wrong statement
      */
      if (str->reserve(MAX_DATE_STRING_REP_LENGTH+3))
        break;

      /* Create date string inplace */
      buf= str->c_ptr_quick();
      ptr= buf;
      *ptr++= '\'';
      ptr+= (uint) my_TIME_to_str(&value.time, ptr);
      *ptr++= '\'';
      str->length((uint32_t) (ptr - buf));
      break;
    }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      str->length(0);
      append_query_string(value.cs_info.character_set_client, &str_value, str);
      break;
    }
  case NULL_VALUE:
    return &my_null_string;
  default:
    assert(0);
  }
  return str;
}


/**
  Convert string from client character set to the character set of
  connection.
*/

bool Item_param::convert_str_value(Session *session)
{
  bool rc= false;
  if (state == STRING_VALUE || state == LONG_DATA_VALUE)
  {
    /*
      Check is so simple because all charsets were set up properly
      in setup_one_conversion_function, where typecode of
      placeholder was also taken into account: the variables are different
      here only if conversion is really necessary.
    */
    if (value.cs_info.final_character_set_of_str_value !=
        value.cs_info.character_set_of_placeholder)
    {
      rc= session->convert_string(&str_value,
                              value.cs_info.character_set_of_placeholder,
                              value.cs_info.final_character_set_of_str_value);
    }
    else
      str_value.set_charset(value.cs_info.final_character_set_of_str_value);
    /* Here str_value is guaranteed to be in final_character_set_of_str_value */

    max_length= str_value.length();
    decimals= 0;
    /*
      str_value_ptr is returned from val_str(). It must be not alloced
      to prevent it's modification by val_str() invoker.
    */
    str_value_ptr.set(str_value.ptr(), str_value.length(),
                      str_value.charset());
    /* Synchronize item charset with value charset */
    collation.set(str_value.charset(), DERIVATION_COERCIBLE);
  }
  return rc;
}


bool Item_param::basic_const_item() const
{
  if (state == NO_VALUE || state == TIME_VALUE)
    return false;
  return true;
}


Item *
Item_param::clone_item()
{
  /* see comments in the header file */
  switch (state) {
  case NULL_VALUE:
    return new Item_null(name);
  case INT_VALUE:
    return (unsigned_flag ?
            new Item_uint(name, value.integer, max_length) :
            new Item_int(name, value.integer, max_length));
  case REAL_VALUE:
    return new Item_float(name, value.real, decimals, max_length);
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return new Item_string(name, str_value.c_ptr_quick(), str_value.length(),
                           str_value.charset());
  case TIME_VALUE:
    break;
  case NO_VALUE:
  default:
    assert(0);
  };
  return 0;
}


bool
Item_param::eq(const Item *arg, bool binary_cmp) const
{
  Item *item;
  if (!basic_const_item() || !arg->basic_const_item() || arg->type() != type())
    return false;
  /*
    We need to cast off const to call val_int(). This should be OK for
    a basic constant.
  */
  item= (Item*) arg;

  switch (state) {
  case NULL_VALUE:
    return true;
  case INT_VALUE:
    return value.integer == item->val_int() &&
           unsigned_flag == item->unsigned_flag;
  case REAL_VALUE:
    return value.real == item->val_real();
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return !sortcmp(&str_value, &item->str_value, collation.collation);
  default:
    break;
  }
  return false;
}

/* End of Item_param related */

void Item_param::print(String *str, enum_query_type)
{
  if (state == NO_VALUE)
  {
    str->append('?');
  }
  else
  {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    String tmp(buffer, sizeof(buffer), &my_charset_bin);
    const String *res;
    res= query_val_str(&tmp);
    str->append(*res);
  }
}
