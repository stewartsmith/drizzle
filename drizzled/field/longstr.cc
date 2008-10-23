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
#include <drizzled/field/longstr.h>
#include <drizzled/error.h>

/*
  Check if we lost any important data and send a truncation error/warning

  SYNOPSIS
    Field_longstr::report_if_important_data()
    ptr                      - Truncated rest of string
    end                      - End of truncated string

  RETURN VALUES
    0   - None was truncated (or we don't count cut fields)
    2   - Some bytes was truncated

  NOTE
    Check if we lost any important data (anything in a binary string,
    or any non-space in others). If only trailing spaces was lost,
    send a truncation note, otherwise send a truncation error.
*/

int
Field_longstr::report_if_important_data(const char *ptr, const char *end)
{
  if ((ptr < end) && table->in_use->count_cuted_fields)
  {
    if (test_if_important_data(field_charset, ptr, end))
    {
      if (table->in_use->abort_on_warning)
        set_warning(DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
      else
        set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    }
    else /* If we lost only spaces then produce a NOTE, not a WARNING */
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_WARN_DATA_TRUNCATED, 1);
    return 2;
  }
  return 0;
}

int Field_longstr::store_decimal(const my_decimal *d)
{
  char buff[DECIMAL_MAX_STR_LENGTH+1];
  String str(buff, sizeof(buff), &my_charset_bin);
  my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, &str);
  return store(str.ptr(), str.length(), str.charset());
}

uint32_t Field_longstr::max_data_length() const
{
  return field_length + (field_length > 255 ? 2 : 1);
}

