/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/statement.h>

namespace drizzled {
namespace statement {

class StartTransaction : public Statement
{
  /* Options used in START TRANSACTION statement */
  start_transaction_option_t start_transaction_opt;

public:
  StartTransaction(Session *in_session, start_transaction_option_t opt= START_TRANS_NO_OPTIONS) :
    Statement(in_session),
    start_transaction_opt(opt)
  {
    set_command(SQLCOM_BEGIN);
  }

  bool execute();

  /* we kinda cheat here as START TRANSACTION will start a transaction,
     so having an autocommit=off implicit start txn doesn't make sense. */
  bool isTransactional()
  {
    return false;
  }
};

} /* namespace statement */

} /* namespace drizzled */

