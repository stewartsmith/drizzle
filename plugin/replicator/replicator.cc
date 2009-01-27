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

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/plugin_replicator.h>
#include <drizzled/serialize/serialize.h>

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

static bool isEnabled;
static char *log_directory= NULL;
int log_file= -1;

static bool write_to_disk(int file, drizzle::EventList *list)
{
  std::string buffer;
  size_t length;
  size_t written;

  list->SerializePartialToString(&buffer);

  length= buffer.length();

  cout << "Writing record of " << length << "." << endl;

  if ((written= write(file, &length, sizeof(uint64_t))) != sizeof(uint64_t))
  {
    cerr << "Only wrote " << written << " out of " << length << "." << endl;
    return true;
  }

  if ((written= write(file, buffer.c_str(), length)) != length)
  {
    cerr << "Only wrote " << written << " out of " << length << "." << endl;
    return true;
  }

  return false;
}

static bool statement(Session *session, const char *query, size_t)
{
  using namespace drizzle;

  drizzle::EventList list;

  if (isEnabled == false)
    return false;
  cerr << "Got into statement" <<endl;

  drizzle::Event *record= list.add_event();
  record->set_type(Event::DDL);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);
  record->set_sql(query);

  return write_to_disk(log_file, &list);
}

static bool session_init(Session *session)
{
  using namespace drizzle;

  if (isEnabled == false)
    return false;

  drizzle::EventList *list= new drizzle::EventList;
  session->setReplicationData(list);

  drizzle::Event *record= list->add_event();

  record->set_type(Event::DDL);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);
  record->set_sql("BEGIN");

  return false;
}

static bool row_insert(Session *session, Table *)
{
  using namespace drizzle;

  if (isEnabled == false)
    return false;

  drizzle::EventList *list= (drizzle::EventList *)session->getReplicationData();
  drizzle::Event *record= list->add_event();

  record->set_type(Event::INSERT);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);
  record->set_sql(session->query);

  return false;
}

static bool row_update(Session *session, Table *, 
                          const unsigned char *, 
                          const unsigned char *)
{
  using namespace drizzle;

  if (isEnabled == false)
    return false;

  drizzle::EventList *list= (drizzle::EventList *)session->getReplicationData();
  drizzle::Event *record= list->add_event();

  record->set_type(Event::UPDATE);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);
  record->set_sql(session->query);

  return false;
}

static bool row_delete(Session *session, Table *)
{
  using namespace drizzle;

  if (isEnabled == false)
    return false;

  drizzle::EventList *list= (drizzle::EventList *)session->getReplicationData();
  drizzle::Event *record= list->add_event();

  record->set_type(Event::DELETE);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);
  record->set_sql(session->query);

  return false;
}

static bool end_transaction(Session *session, bool autocommit, bool commit)
{
  bool error;
  using namespace drizzle;

  if (isEnabled == false)
    return false;

  cerr << "Got into end" <<endl;

  drizzle::EventList *list= (drizzle::EventList *)session->getReplicationData();
  drizzle::Event *record= list->add_event();

  record->set_type(Event::DELETE);
  record->set_autocommit(true);
  record->set_server_id("localhost");
  record->set_query_id(10);
  record->set_transaction_id("junk");
  record->set_schema(session->db);

  if (commit)
  {
    if (autocommit)
      record->set_sql("COMMIT");
    else
      record->set_sql("AUTOCOMMIT");
  }
  else
    record->set_sql("ROLLBACK");

  error= write_to_disk(log_file, list);

  session->setReplicationData(NULL);
  delete(list);

  return error;
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


  if (isEnabled)
  {
    using std::string;
    string logname;

    logname.append(log_directory ? log_directory : "/tmp");
    logname.append("/replication_log");

    if ((log_file= open(logname.c_str(), O_TRUNC|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU)) == -1)
    {
      cerr << "Can not open file: " << logname.c_str() << endl;
      exit(0);
    }
  }

  return 0;
}

static int deinit(void *)
{
  if (log_file != -1)
    close(log_file);

  return 0;
}

static DRIZZLE_SYSVAR_BOOL(
  enabled,
  isEnabled,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable Replicator"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_STR(
  directory,
  log_directory,
  PLUGIN_VAR_READONLY,
  N_("Directory to place replication logs."),
  NULL, /* check func */
  NULL, /* update func*/
  NULL /* default */);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(directory),
  DRIZZLE_SYSVAR(enabled),
  NULL,
};

drizzle_declare_plugin(replicator)
{
  DRIZZLE_REPLICATOR_PLUGIN,
  "replicator",
  "0.1",
  "Brian Aker",
  "Basic replication module",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
