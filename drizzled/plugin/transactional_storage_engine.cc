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

#include "config.h"

#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/resource_context.h"
#include "drizzled/session.h"

#include <vector>
#include <algorithm>
#include <functional>

using namespace std;

namespace drizzled
{

namespace plugin
{

static vector<TransactionalStorageEngine *> vector_of_transactional_engines;

TransactionalStorageEngine::TransactionalStorageEngine(const string name_arg,
                                     const bitset<HTON_BIT_SIZE> &flags_arg,
                                     bool two_phase_commit_arg)
    : StorageEngine(name_arg, flags_arg),
      two_phase_commit(two_phase_commit_arg)
{
}

TransactionalStorageEngine::~TransactionalStorageEngine()
{
}

void TransactionalStorageEngine::setTransactionReadWrite(Session& session)
{
  ResourceContext *resource_context= session.getResourceContext(this);

  /*
    When a storage engine method is called, the transaction must
    have been started, unless it's a DDL call, for which the
    storage engine starts the transaction internally, and commits
    it internally, without registering in the ha_list.
    Unfortunately here we can't know know for sure if the engine
    has registered the transaction or not, so we must check.
  */
  if (resource_context->isStarted())
  {
    resource_context->markModifiedData();
  }
}

/**
  @details
  This function should be called when MySQL sends rows of a SELECT result set
  or the EOF mark to the client. It releases a possible adaptive hash index
  S-latch held by session in InnoDB and also releases a possible InnoDB query
  FIFO ticket to enter InnoDB. To save CPU time, InnoDB allows a session to
  keep them over several calls of the InnoDB Cursor interface when a join
  is executed. But when we let the control to pass to the client they have
  to be released because if the application program uses mysql_use_result(),
  it may deadlock on the S-latch if the application on another connection
  performs another SQL query. In MySQL-4.1 this is even more important because
  there a connection can have several SELECT queries open at the same time.

  @param session           the thread handle of the current connection

  @return
    always 0
*/
int TransactionalStorageEngine::releaseTemporaryLatches(Session *session)
{
  for_each(vector_of_transactional_engines.begin(), vector_of_transactional_engines.end(),
           bind2nd(mem_fun(&TransactionalStorageEngine::doReleaseTemporaryLatches),session));
  return 0;
}

int TransactionalStorageEngine::startConsistentSnapshot(Session *session)
{
  for_each(vector_of_transactional_engines.begin(), vector_of_transactional_engines.end(),
           bind2nd(mem_fun(&TransactionalStorageEngine::doStartConsistentSnapshot),
                   session));
  return 0;
}

bool TransactionalStorageEngine::addPlugin(TransactionalStorageEngine *engine)
{
  vector_of_transactional_engines.push_back(engine);

  return StorageEngine::addPlugin(engine);
}

void TransactionalStorageEngine::removePlugin(TransactionalStorageEngine *)
{
  vector_of_transactional_engines.clear();
}

} /* namespace plugin */
} /* namespace drizzled */
