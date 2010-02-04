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

#ifndef DRIZZLED_TRANSACTION_CONTEXT_H
#define DRIZZLED_TRANSACTION_CONTEXT_H

#include <vector>

namespace drizzled
{
class ResourceContext;

class TransactionContext
{
public:
  TransactionContext() :
    no_2pc(false),
    resource_contexts(),
    modified_non_trans_table(false)
  {}

  void reset() { no_2pc= false; modified_non_trans_table= false; resource_contexts.clear();}

  typedef std::vector<ResourceContext *> ResourceContexts;

  void setResourceContexts(ResourceContexts &new_contexts)
  {
    resource_contexts.assign(new_contexts.begin(), new_contexts.end());
  }

  ResourceContexts &getResourceContexts()
  {
    return resource_contexts;
  }
  /** Register a resource context in this transaction context */
  void registerResource(ResourceContext *resource)
  {
    resource_contexts.push_back(resource);
  }

  /* true is not all entries in the resource contexts support 2pc */
  bool no_2pc;
private:
  /* storage engines that registered in this transaction */
  ResourceContexts resource_contexts;
public:
  /*
    The purpose of this flag is to keep track of non-transactional
    tables that were modified in scope of:
    - transaction, when the variable is a member of
    Session::transaction.all
    - top-level statement or sub-statement, when the variable is a
    member of Session::transaction.stmt
    This member has the following life cycle:
    * stmt.modified_non_trans_table is used to keep track of
    modified non-transactional tables of top-level statements. At
    the end of the previous statement and at the beginning of the session,
    it is reset to false.  If such functions
    as mysql_insert, mysql_update, mysql_delete etc modify a
    non-transactional table, they set this flag to true.  At the
    end of the statement, the value of stmt.modified_non_trans_table
    is merged with all.modified_non_trans_table and gets reset.
    * all.modified_non_trans_table is reset at the end of transaction

    * Since we do not have a dedicated context for execution of a
    sub-statement, to keep track of non-transactional changes in a
    sub-statement, we re-use stmt.modified_non_trans_table.
    At entrance into a sub-statement, a copy of the value of
    stmt.modified_non_trans_table (containing the changes of the
    outer statement) is saved on stack. Then
    stmt.modified_non_trans_table is reset to false and the
    substatement is executed. Then the new value is merged with the
    saved value.
  */
  bool modified_non_trans_table;
};

} /* namespace drizzled */

#endif /* DRIZZLED_TRANSACTION_CONTEXT_H */
