/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#define DRIZZLE_SERVER 1 /* for session variable max_allowed_packet */
#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/item/strfunc.h>
#include <drizzled/plugin_replicator.h>

static char anchor[100];

static bool statement(Session *, const char *query, size_t query_length)
{
  fprintf(stderr, "STATEMENT: %.*s\n", (uint32_t)query_length, query);

  return false;
}

static bool session_init(Session *session)
{
  fprintf(stderr, "Starting Session\n");
  session->setReplicationData(anchor);

  return false;
}

static bool row_insert(Session *session, Table *)
{
  fprintf(stderr, "INSERT: %.*s\n", (uint32_t)session->query_length, session->query);

  return false;
}

static bool row_update(Session *session, Table *, 
                          const unsigned char *, 
                          const unsigned char *)
{
  fprintf(stderr, "UPDATE: %.*s\n", (uint32_t)session->query_length, session->query);

  return false;
}

static bool row_delete(Session *session, Table *)
{
  fprintf(stderr, "DELETE: %.*s\n", (uint32_t)session->query_length, session->query);

  return false;
}

static bool end_transaction(Session *session, bool autocommit, bool commit)
{
  if (commit)
  {
    if (autocommit)
      fprintf(stderr, "COMMIT\n");
    else
      fprintf(stderr, "AUTOCOMMIT\n");
  }
  else
    fprintf(stderr, "ROLLBACK\n");

  session->setReplicationData(NULL);

  return false;
}

static int init(void *p)
{
  replicator_t *repl = (replicator_t *)p;

  repl->statement= statement;
  repl->session_init= session_init;
  repl->row_insert= row_insert;
  repl->row_delete= row_delete;
  repl->row_update= row_update;
  repl->end_transaction= end_transaction;

  return 0;
}

mysql_declare_plugin(replicator)
{
  DRIZZLE_REPLICATOR_PLUGIN,
  "replicator",
  "0.1",
  "Brian Aker",
  "Basic replication module",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
