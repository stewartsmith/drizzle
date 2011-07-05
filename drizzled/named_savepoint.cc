/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com> 
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

#include <config.h>
#include <string>
#include <drizzled/named_savepoint.h>
#include <drizzled/message/transaction.pb.h>

namespace drizzled
{
  NamedSavepoint::NamedSavepoint(const NamedSavepoint &other)
  {
    name= other.getName();
    resource_contexts= other.getResourceContexts();
    transaction_message= other.getTransactionMessage() ? new message::Transaction(*other.getTransactionMessage()) : NULL;
  }

  NamedSavepoint::~NamedSavepoint()
  {
    delete transaction_message;
  }

} /* namespace drizzled */
