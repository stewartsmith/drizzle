/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

#pragma once

#include <drizzled/plugin/plugin.h>
#include <drizzled/replication_services.h> /* For global transaction ID typedef */

#include <drizzled/visibility.h>

/**
 * @file Defines the API for a TransactionReader
 *
 * A command reader is a class which is able to read Transaction messages from some source
 */

namespace drizzled {
namespace plugin {

/**
 * Class which can read Transaction messages from some source
 */
class DRIZZLED_API TransactionReader : public Plugin
{
  TransactionReader();
  TransactionReader(const TransactionReader &);
  TransactionReader& operator=(const TransactionReader &);
public:
  explicit TransactionReader(std::string name_arg)
   : Plugin(name_arg, "TransactionReader") {}
  virtual ~TransactionReader() {}
  /**
   * Read and fill a Transaction message with the supplied
   * Transaction message global transaction ID.
   *
   * @param Global transaction ID to find
   * @param Pointer to a command message to fill
   *
   * @retval
   *  true if Transaction message was read successfully and the supplied pointer
   *  to message was filled
   * @retval
   *  false if not found or read successfully
   */
  virtual bool read(const ReplicationServices::GlobalTransactionId &to_read, 
                    message::Transaction *to_fill)= 0;
};

} /* end namespace plugin */
} /* end namespace drizzled */

