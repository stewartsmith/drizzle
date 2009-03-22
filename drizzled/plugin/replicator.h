/*
  -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
  *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

  *  Definitions required for Configuration Variables plugin

  *  Copyright (C) 2008 Mark Atwood
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

#ifndef DRIZZLED_PLUGIN_REPLICATOR_H
#define DRIZZLED_PLUGIN_REPLICATOR_H

/**
 * @file
 *
 * Defines the API for Replication
 */

/**
 * Base class for replication implementations.
 *
 *    if a method returns bool true, that means it failed.
 */
class Replicator
{
private:
  bool enabled;
protected:
  virtual bool session_init_hook(Session *) { return false; }
  virtual bool row_insert_hook(Session *, Table *) { return false; }
  virtual bool row_update_hook(Session *, Table *,
                               const unsigned char *,
                               const unsigned char *) { return false; }
  virtual bool row_delete_hook(Session *, Table *) { return false; }
  virtual bool end_transaction_hook(Session *, bool, bool) { return false; }
  virtual bool statement_hook(Session *, const char *, size_t) { return false; }
public:

  Replicator() : enabled(true) {}

  virtual ~Replicator() {}
  /**
   * Initialize session for replication
   */
  bool session_init(Session *session)
  {
    return session_init_hook(session);
  }

  /**
   * Row insert
   *
   * @param current Session
   * @param Table inserted
   */
  bool row_insert(Session *session, Table *table)
  {
    return row_insert_hook(session, table);
  }

  /**
   * Row update
   *
   * @param current Session
   * @param Table updated
   * @param before values
   * @param after values
   */
  bool row_update(Session *session, Table *table,
                          const unsigned char *before,
                          const unsigned char *after)
  {
    return row_update_hook(session, table, before, after);
  }

  /**
   * Row Delete
   *
   * @param current session
   * @param Table deleted from
   */
  bool row_delete(Session *session, Table *table)
  {
    return row_delete_hook(session, table);
  }

  /**
   * End Transaction
   *
   * @param current Session
   * @param is autocommit on?
   * @param did the transaction commit?
   */
  bool end_transaction(Session *session, bool autocommit, bool commit)
  {
    return end_transaction_hook(session, autocommit, commit);
  }

  /**
   * Raw statement
   *
   * @param current Session
   * @param query string
   * @param length of query string in bytes
   */
  bool statement(Session *session, const char *query, size_t query_length)
  {
    return statement_hook(session, query, query_length);
  }
};

#endif /* DRIZZLED_PLUGIN_REPLICATOR_H */
