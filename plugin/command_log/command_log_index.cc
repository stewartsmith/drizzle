/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * Implementation of an index into a command log file
 */

#include "command_log_index.h"

#include <drizzled/gettext.h>

using namespace std;
using namespace drizzled;

CommandLogIndex::CommandLogIndex()
  : 
    min_transaction_id(0),
    max_transaction_id(0)
{
  (void) pthread_mutex_init(&lock, NULL);
}

CommandLogIndex::~CommandLogIndex()
{
  pthread_mutex_lock(&lock);
  trx_id_offset_map.clear();
  pthread_mutex_destroy(&lock);
}

bool CommandLogIndex::contains(const ReplicationServices::GlobalTransactionId &to_find)
{
  bool result;
  pthread_mutex_lock(&lock);
  result= (to_find >= min_transaction_id && to_find <= max_transaction_id);
  pthread_mutex_unlock(&lock);
  return result;
}

void CommandLogIndex::addRecord(const ReplicationServices::GlobalTransactionId &in_trx_id,
                                const off_t in_offset)
{
  pthread_mutex_lock(&lock);
  trx_id_offset_map[in_trx_id]= in_offset;
  max_transaction_id= in_trx_id;
  pthread_mutex_unlock(&lock);
}

off_t CommandLogIndex::getOffset(const ReplicationServices::GlobalTransactionId &in_trx_id)
{
  off_t result;
  pthread_mutex_lock(&lock);
  result= trx_id_offset_map[in_trx_id];
  pthread_mutex_unlock(&lock);
  return result;
}
