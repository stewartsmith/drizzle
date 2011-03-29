/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/**
 * @file Simple named savepoint class.
 */

#pragma once

#include <drizzled/transaction_context.h>
#include <string>

namespace drizzled {

/**
 * This is a class which stores information about
 * a named savepoint in a transaction
 */
class NamedSavepoint
{
public:
  /**
   * Constructor
   */
  NamedSavepoint(const char *in_name, size_t in_name_length) :
    name(in_name, in_name_length),
    resource_contexts(),
    transaction_message(NULL)
  {}

  ~NamedSavepoint();

  NamedSavepoint(const NamedSavepoint &other);

  void setResourceContexts(TransactionContext::ResourceContexts &new_contexts)
  {
    resource_contexts.assign(new_contexts.begin(), new_contexts.end());
  }
  const TransactionContext::ResourceContexts &getResourceContexts() const
  {
    return resource_contexts;
  }
  TransactionContext::ResourceContexts &getResourceContexts()
  {
    return resource_contexts;
  }
  const std::string &getName() const
  {
    return name;
  }
  const std::string &getName()
  {
    return name;
  }
  message::Transaction *getTransactionMessage() const
  {
    return transaction_message;
  }
  void setTransactionMessage(message::Transaction *in_transaction_message)
  {
    transaction_message= in_transaction_message;
  }
  NamedSavepoint &operator=(const NamedSavepoint &other)
  {
    if (this == &other)
      return *this;

    name.assign(other.getName());
    const TransactionContext::ResourceContexts &other_resource_contexts= other.getResourceContexts();
    resource_contexts.assign(other_resource_contexts.begin(),
                             other_resource_contexts.end());
    return *this;
  }
private:
  std::string name;
  TransactionContext::ResourceContexts resource_contexts;
  message::Transaction *transaction_message;
  NamedSavepoint();
};

} /* namespace drizzled */

