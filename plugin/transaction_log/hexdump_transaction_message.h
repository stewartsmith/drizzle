/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Defines the HEXDUMP_TRANSACTION_MESSAGE() UDF
 *
 * @details
 *
 * HEXDUMP_TRANSACTION_MESSAGE(filename, offset);
 *
 * prints a text representation of the raw bytes which make up
 * the transaction message in the logfile at the supplied offset
 */

#pragma once

#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

class HexdumpTransactionMessageFunction : public drizzled::Item_str_func
{
public:
  HexdumpTransactionMessageFunction() : Item_str_func() {}
  drizzled::String *val_str(drizzled::String*);

  void fix_length_and_dec();

  const char *func_name() const
  {
    return "hexdump_transaction_message";
  }

  bool check_argument_count(int n)
  {
    return (n == 2);
  }
};


