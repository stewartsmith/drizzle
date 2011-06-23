/* -*- mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <boost/lexical_cast.hpp>
#include <drizzled/field/epoch.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/current_session.h>
#include <drizzled/temporal.h>
#include <cmath>
#include <sstream>

namespace drizzled {
namespace field {

/**
  TIMESTAMP type holds datetime values in range from 1970-01-01 00:00:01 UTC to
  2038-01-01 00:00:00 UTC stored as number of seconds since Unix
  Epoch in UTC.

  Up to one of timestamps columns in the table can be automatically
  set on row update and/or have NOW() as default value.
  TABLE::timestamp_field points to Field object for such timestamp with
  auto-set-on-update. TABLE::time_stamp holds offset in record + 1 for this
  field, and is used by handler code which performs updates required.

  Actually SQL-99 says that we should allow niladic functions (like NOW())
  as defaults for any field. Current limitations (only NOW() and only
  for one TIMESTAMP field) are because of restricted binary .frm format
  and should go away in the future.

  Also because of this limitation of binary .frm format we use 5 different
  unireg_check values with TIMESTAMP field to distinguish various cases of
  DEFAULT or ON UPDATE values. These values are:

  TIMESTAMP_OLD_FIELD - old timestamp, if there was not any fields with
    auto-set-on-update (or now() as default) in this table before, then this
    field has NOW() as default and is updated when row changes, else it is
    field which has 0 as default value and is not automatically updated.
  TIMESTAMP_DN_FIELD - field with NOW() as default but not set on update
    automatically (TIMESTAMP DEFAULT NOW())
  TIMESTAMP_UN_FIELD - field which is set on update automatically but has not
    NOW() as default (but it may has 0 or some other const timestamp as
    default) (TIMESTAMP ON UPDATE NOW()).
  TIMESTAMP_DNUN_FIELD - field which has now() as default and is auto-set on
    update. (TIMESTAMP DEFAULT NOW() ON UPDATE NOW())
  NONE - field which is not auto-set on update with some other than NOW()
    default value (TIMESTAMP DEFAULT 0).

  Note that TIMESTAMP_OLD_FIELDs are never created explicitly now, they are
  left only for preserving ability to read old tables. Such fields replaced
  with their newer analogs in CREATE TABLE and in SHOW CREATE TABLE. This is
  because we want to prefer NONE unireg_check before TIMESTAMP_OLD_FIELD for
  "TIMESTAMP DEFAULT 'Const'" field. (Old timestamps allowed such
  specification too but ignored default value for first timestamp, which of
  course is non-standard.) In most cases user won't notice any change, only
  exception is different behavior of old/new timestamps during ALTER TABLE.
 */
  Epoch::Epoch(unsigned char *ptr_arg,
               unsigned char *null_ptr_arg,
               unsigned char null_bit_arg,
               enum utype unireg_check_arg,
               const char *field_name_arg,
               drizzled::TableShare *share) :
  Field_str(ptr_arg,
            MicroTimestamp::MAX_STRING_LENGTH - 1, /* no \0 */
            null_ptr_arg,
            null_bit_arg,
            field_name_arg,
            &my_charset_bin)
{
  unireg_check= unireg_check_arg;
  if (! share->getTimestampField() && unireg_check != NONE)
  {
    /* This timestamp has auto-update */
    share->setTimestampField(this);
    flags|= FUNCTION_DEFAULT_FLAG;
    if (unireg_check != TIMESTAMP_DN_FIELD)
      flags|= ON_UPDATE_NOW_FLAG;
  }
}

Epoch::Epoch(bool maybe_null_arg,
             const char *field_name_arg) :
  Field_str((unsigned char*) NULL,
            MicroTimestamp::MAX_STRING_LENGTH - 1, /* no \0 */
            maybe_null_arg ? (unsigned char*) "": 0,
            0,
            field_name_arg,
            &my_charset_bin)
{
  if (unireg_check != TIMESTAMP_DN_FIELD)
    flags|= ON_UPDATE_NOW_FLAG;
}

/**
  Get auto-set type for TIMESTAMP field.

  Returns value indicating during which operations this TIMESTAMP field
  should be auto-set to current timestamp.
*/
timestamp_auto_set_type Epoch::get_auto_set_type() const
{
  switch (unireg_check)
  {
  case TIMESTAMP_DN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_INSERT;
  case TIMESTAMP_UN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_UPDATE;
  case TIMESTAMP_OLD_FIELD:
    /*
      Although we can have several such columns in legacy tables this
      function should be called only for first of them (i.e. the one
      having auto-set property).
    */
    assert(getTable()->timestamp_field == this);
    /* Fall-through */
  case TIMESTAMP_DNUN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_BOTH;
  default:
    /*
      Normally this function should not be called for TIMESTAMPs without
      auto-set property.
    */
    assert(0);
    return TIMESTAMP_NO_AUTO_SET;
  }
}

int Epoch::store(const char *from,
                 uint32_t len,
                 const charset_info_st * const )
{
  Timestamp temporal;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (not temporal.from_string(from, (size_t) len))
  {
    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), from);
    return 1;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  uint64_t time_tmp= tmp;
  pack_num(time_tmp);
  return 0;
}

int Epoch::store(double from)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  uint64_t from_tmp= (uint64_t)from;

  Timestamp temporal;
  if (not temporal.from_int64_t(from_tmp))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  uint64_t tmp_micro= tmp;
  pack_num(tmp_micro);

  return 0;
}

int Epoch::store_decimal(const type::Decimal *value)
{
  double tmp;
  value->convert(tmp);

  return store(tmp);
}

int Epoch::store(int64_t from, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  /* 
   * Try to create a DateTime from the supplied integer.  Throw an error
   * if unable to create a valid DateTime.  
   */
  Timestamp temporal;
  if (not temporal.from_int64_t(from))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  uint64_t tmp64= tmp;
  pack_num(tmp64);

  return 0;
}

double Epoch::val_real(void) const
{
  return (double) Epoch::val_int();
}

int64_t Epoch::val_int(void) const
{
  uint64_t temp;

  ASSERT_COLUMN_MARKED_FOR_READ;

  unpack_num(temp);

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  /* We must convert into a "timestamp-formatted integer" ... */
  int64_t result;
  temporal.to_int64_t(&result);
  return result;
}

String *Epoch::val_str(String *val_buffer, String *) const
{
  uint64_t temp= 0;
  char *to;
  int to_len= field_length + 1;

  val_buffer->alloc(to_len);
  to= (char *) val_buffer->ptr();

  unpack_num(temp);

  val_buffer->set_charset(&my_charset_bin);	/* Safety */

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  int rlen;
  rlen= temporal.to_string(to, to_len);
  assert(rlen < to_len);

  val_buffer->length(rlen);
  return val_buffer;
}

bool Epoch::get_date(type::Time &ltime, uint32_t) const
{
  uint64_t temp;
  type::Time::epoch_t time_temp;

  unpack_num(temp);
  time_temp= temp;
  
  ltime.reset();

  ltime.store(time_temp);

  return 0;
}

bool Epoch::get_time(type::Time &ltime) const
{
  return Epoch::get_date(ltime, 0);
}

int Epoch::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  uint64_t a,b;

  unpack_num(a, a_ptr);
  unpack_num(b, b_ptr);

  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


void Epoch::sort_string(unsigned char *to,uint32_t )
{
#ifdef WORDS_BIGENDIAN
  if (!getTable() || !getTable()->getShare()->db_low_byte_first)
  {
    to[0] = ptr[0];
    to[1] = ptr[1];
    to[2] = ptr[2];
    to[3] = ptr[3];
    to[4] = ptr[4];
    to[5] = ptr[5];
    to[6] = ptr[6];
    to[7] = ptr[7];
  }
  else
#endif
  {
    to[0] = ptr[7];
    to[1] = ptr[6];
    to[2] = ptr[5];
    to[3] = ptr[4];
    to[4] = ptr[3];
    to[5] = ptr[2];
    to[6] = ptr[1];
    to[7] = ptr[0];
  }
}

void Epoch::set_time()
{
  Session *session= getTable() ? getTable()->in_use : current_session;
  time_t tmp= session->times.getCurrentTimestampEpoch();

  set_notnull();
  pack_num(static_cast<uint32_t>(tmp));
}

void Epoch::set_default()
{
  if (getTable()->timestamp_field == this &&
      unireg_check != TIMESTAMP_UN_FIELD)
  {
    set_time();
  }
  else
  {
    Field::set_default();
  }
}

long Epoch::get_timestamp(bool *null_value) const
{
  if ((*null_value= is_null()))
    return 0;

  uint64_t tmp;
  return unpack_num(tmp);
}

size_t Epoch::max_string_length()
{
  return sizeof(uint64_t);
}

} /* namespace field */
} /* namespace drizzled */
