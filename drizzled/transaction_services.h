/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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

#ifndef DRIZZLED_REPLICATOR_H
#define DRIZZLED_REPLICATOR_H

#include <vector>


/* some forward declarations needed */
class Session;
class Table;

namespace drizzled
{
  namespace plugin
  {
    class Replicator;
    class Applier;
  }
  namespace message
  {
    class Command;
  }
}

void add_replicator(drizzled::plugin::Replicator *repl);
int replicator_finalizer (st_plugin_int *plugin);

/**
 * This is a class which manages transforming internal 
 * transactional events into GPB messages and sending those
 * events out through registered replicators and appliers.
 */
namespace drizzled
{
class TransactionServices
{
private:
  /** Our collection of replicator plugins */
  std::vector<drizzled::plugin::Replicator *> replicators;
  /** Our collection of applier plugins */
  std::vector<drizzled::plugin::Applier *> appliers;
  /** 
   * Helper method which attaches a transaction context
   * the supplied command based on the supplied Session's
   * transaction information.
   */
  void setCommandTransactionContext(drizzled::message::Command *in_command, Session *in_session) const;
  /**
   * Helper method which pushes a constructed message out
   * to the registered replicator and applier plugins.
   *
   * @param Message to push out
   */
  void push(drizzled::message::Command *to_push);
public:
  /**
   * Attaches a replicator to our internal collection of
   * replicators.
   *
   * @param Pointer to a replicator to attach/register
   */
  void attachReplicator(drizzled::plugin::Replicator *in_replicator);
  /**
   * Detaches/unregisters a replicator with our internal
   * collection of replicators.
   *
   * @param Pointer to the replicator to detach
   */
  void detachReplicator(drizzled::plugin::Replicator *in_replicator);
  /**
   * Attaches a applier to our internal collection of
   * appliers.
   *
   * @param Pointer to a applier to attach/register
   */
  void attachApplier(drizzled::plugin::Applier *in_applier);
  /**
   * Detaches/unregisters a applier with our internal
   * collection of appliers.
   *
   * @param Pointer to the applier to detach
   */
  void detachApplier(drizzled::plugin::Applier *in_applier);
  /**
   * Creates a new StartTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session starting the transaction
   */
  void startTransaction(Session *in_session);
  /**
   * Creates a new CommitTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void commitTransaction(Session *in_session);
  /**
   * Creates a new RollbackTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void rollbackTransaction(Session *in_session);
  /**
   * Creates a new InsertRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has inserted a record
   * @param Pointer to the Table containing insert information
   */
  void insertRecord(Session *in_session, Table *in_table);
  /**
   * Creates a new UpdateRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has updated a record
   * @param Pointer to the Table containing update information
   */
  void updateRecord(Session *in_session, Table *in_table, const unsigned char *, const unsigned char *);
  /**
   * Creates a new DeleteRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has deleted a record
   * @param Pointer to the Table containing delete information
   */
  void deleteRecord(Session *in_session, Table *in_table);
  /**
   * Creates a new RawSql GPB message and pushes it to 
   * replicators.
   *
   * @TODO With a real data dictionary, this really shouldn't
   * be needed.  CREATE TABLE would map to insertRecord call
   * on the I_S, etc.  Not sure what to do with administrative
   * commands like CHECK TABLE, though..
   *
   * @param Pointer to the Session which issued the statement
   * @param Query string
   * @param Length of the query string
   */
  void rawStatement(Session *in_session, const char *in_query, size_t in_query_len);
};

} /* end namespace drizzled */

#ifdef oldcode
/* todo, fill in this API */
/* these are the functions called by the rest of the drizzle server
   to do whatever this plugin does. */
bool replicator_session_init (Session *session);
bool replicator_write_row(Session *session, Table *table);
bool replicator_update_row(Session *session, Table *table,
                           const unsigned char *before,
                           const unsigned char *after);
bool replicator_delete_row(Session *session, Table *table);

/* The below control transactions */
bool replicator_end_transaction(Session *session, bool autocommit, bool commit);
bool replicator_prepare(Session *session);
bool replicator_statement(Session *session, const char *query, size_t query_length);
#endif /* oldcode */
#endif /* DRIZZLED_REPLICATOR_H */
