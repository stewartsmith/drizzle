/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

/*
  The actual probe names in DTrace scripts will replace '__' by '-'. Thus
  insert__row__start will be insert-row-start.

  Recommendations for adding new probes:

  - each probe should have the minimal set of arguments required to
  unambiguously identify the context in which the probe fires. Redundant
  probes (i.e. the ones that can be obtained in user scripts from previous
  probes' arguments or otherwise) may be added for convenience.

  - try to avoid computationally expensive probe arguments. If impossible,
  use *_ENABLED() macros to check if the probe is activated before
  performing expensive calculations for a probe argument.

  - all *-done probes should have a status argument wherever applicable to make
  it possible for user scripts to figure out whether the completed operation
  was successful or not.
  
  - for all status arguments, a non-zero value should be returned on error or
  failure, 0 should be returned on success.
*/

provider drizzle {
  
  /* The following ones fire when creating or closing a client connection */
  probe connection__start(unsigned long conn_id);
  probe connection__done(unsigned long conn_id);

  /*
   * Fire at the start/end of any client command processing (including SQL
   * queries).
  */
  probe command__start(unsigned long conn_id, int command);
  probe command__done(int status);
  
  /*
   * The following probes fire at the start/end of any SQL query processing,
   * respectively.
   *
   * query_start() has a lot of parameters that can be used to pick up
   * parameters for a lot of other probes here.  For simplicity reasons we also
   * add the query string to most other DTrace probes as well. Hostname is
   * either the hostname or the IP address of the Drizzle client.
   */
  probe query__start(const char *query,
                     unsigned long conn_id,
                     const char *db_name);
  probe query__done(int status); 

  /* Fire at the start/end of SQL query parsing */
  probe query__parse__start(const char *query);
  probe query__parse__done(int status);

  /*
   * This probe fires when the actual query execution starts
   */
  probe query__exec__start(const char *query,
                           unsigned long connid,
                           const char *db_name);
  probe query__exec__done(int status);

  /*
   * These probes fire in the query optimizer
   */
  probe query__opt__start(const char *query,
                          unsigned long connid);
  probe query__opt__done(int status);
  probe query__opt__choose__plan__start(const char *query,
                                        unsigned long connid);
  probe query__opt__choose__plan__done(int status);

  /* These probes fire when performing write operations towards any Cursor */
  probe insert__row__start(const char *db, const char *table);
  probe insert__row__done(int status);
  probe update__row__start(const char *db, const char *table);
  probe update__row__done(int status);
  probe delete__row__start(const char *db, const char *table);
  probe delete__row__done(int status);

  /*
   * These probes fire when calling external_lock for any Cursor 
   * depending on the lock type being acquired or released.
   */
  probe cursor__rdlock__start(const char *db, const char *table);
  probe cursor__wrlock__start(const char *db, const char *table);
  probe cursor__unlock__start(const char *db, const char *table);
  probe cursor__rdlock__done(int status);
  probe cursor__wrlock__done(int status);
  probe cursor__unlock__done(int status);
  
  /*
   * These probes fire when a filesort activity happens in a query.
   */
  probe filesort__start(const char *db, const char *table);
  probe filesort__done(int status, unsigned long rows);
  /*
   * The query types SELECT, INSERT, INSERT AS SELECT, UPDATE, DELETE
   * are all probed.
   * The start probe always contains the query text.
   */
  probe select__start(const char *query);
  probe select__done(int status, unsigned long rows);
  probe insert__start(const char *query);
  probe insert__done(int status, unsigned long rows);
  probe insert__select__start(const char *query);
  probe insert__select__done(int status, unsigned long rows);
  probe update__start(const char *query);
  probe update__done(int status,
                     unsigned long rowsmatches, unsigned long rowschanged);
  probe delete__start(const char *query);
  probe delete__done(int status, unsigned long rows);

};
