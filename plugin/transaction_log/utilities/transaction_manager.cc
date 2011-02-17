/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 David Shrewsbury <shrewsbury.dave@gmail.com>
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
 * @file
 * Implementation of the TransactionManager class.
 */

#include <config.h>
#include "transaction_manager.h"

using namespace std;
using namespace drizzled;


bool TransactionManager::store(const message::Transaction &transaction)
{
  string msg;
  const message::TransactionContext trx_ctxt= transaction.transaction_context();
  uint64_t trx_id= trx_ctxt.transaction_id();
  transaction.SerializeToString(&msg);

  cache[trx_id].push_back(msg);
  return true;
}

bool TransactionManager::remove(uint64_t trx_id)
{
  cache.erase(trx_id);
  return true;
}

bool TransactionManager::contains(uint64_t trx_id)
{
  boost::unordered_map< uint64_t, vector<string> >::const_iterator it= cache.find(trx_id);

  if (it != cache.end())
    return true;

  return false;
}

uint32_t TransactionManager::getTransactionBufferSize(uint64_t trx_id)
{
  return static_cast<uint32_t>(cache[trx_id].size());
}

bool TransactionManager::getTransactionMessage(message::Transaction &trx,
                                               uint64_t trx_id,
                                               uint32_t position)
{
  trx.ParseFromString(cache[trx_id].at(position));
  return true;
}
