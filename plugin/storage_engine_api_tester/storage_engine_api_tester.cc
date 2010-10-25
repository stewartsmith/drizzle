/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "config.h"
#include <drizzled/table.h>
#include <drizzled/error.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <string>
#include <map>
#include <fstream>
#include <drizzled/message/table.pb.h>
#include "drizzled/internal/m_string.h"

#include "drizzled/global_charset_info.h"

#include <boost/unordered_map.hpp>

using namespace std;
using namespace drizzled;

typedef boost::unordered_map<std::string, message::Table, util::insensitive_hash, util::insensitive_equal_to> TableMessageMap;
typedef boost::unordered_map<std::string, message::Table, util::insensitive_hash, util::insensitive_equal_to>::iterator TableMessageMapIterator;

string engine_state;
typedef multimap<string, string> state_multimap;
typedef multimap<string, string>::value_type state_pair;
typedef multimap<string, string>::iterator state_multimap_iter;
state_multimap engine_state_transitions;
state_multimap cursor_state_transitions;

/* This is a hack to make store_lock kinda work */
drizzled::THR_LOCK share_lock;

static inline void ENGINE_NEW_STATE(const string &new_state)
{
  state_multimap_iter cur= engine_state_transitions.find(engine_state);
  if (engine_state_transitions.count(engine_state) == 0)
  {
    cerr << "ERROR: Invalid engine state: " << engine_state << endl
         << "This should *NEVER* happen."
         << endl
         << "i.e. you've really screwed it up and you should be ashamed of "
         << "yourself." << endl;
    assert(engine_state_transitions.count(engine_state));
  }

  for(cur= engine_state_transitions.lower_bound(engine_state);
      cur != engine_state_transitions.upper_bound(engine_state);
      cur++)
  {
    if (new_state.compare((*cur).second) == 0)
      break;
  }

  if (cur == engine_state_transitions.end()
      || new_state.compare((*cur).second))
  {
    cerr << "ERROR: Invalid Storage Engine state transition!" << endl
         << "Cannot go from " << engine_state << " to " << new_state << endl;
    assert(false);
  }

  engine_state= new_state;

  cerr << "\tENGINE STATE : " << engine_state << endl;
}

static const string engine_name("STORAGE_ENGINE_API_TESTER");

class SEAPITesterCursor : public drizzled::Cursor
{
public:
  SEAPITesterCursor(drizzled::plugin::StorageEngine &engine_arg,
                    drizzled::TableShare &table_arg)
    : Cursor(engine_arg, table_arg)
    { cursor_state= "Cursor()"; }
  ~SEAPITesterCursor()
  {}

  int close() { return 0; }
  int rnd_next(unsigned char*) { CURSOR_NEW_STATE("::rnd_next()"); return HA_ERR_END_OF_FILE; }
  int rnd_pos(unsigned char*, unsigned char*) { CURSOR_NEW_STATE("::rnd_pos()"); return -1; }
  void position(const unsigned char*) { CURSOR_NEW_STATE("::position()"); return; }
  int info(uint32_t) { return 0; }
  void get_auto_increment(uint64_t, uint64_t, uint64_t, uint64_t*, uint64_t*) {}
  int doStartTableScan(bool) { CURSOR_NEW_STATE("::doStartTableScan()"); return HA_ERR_END_OF_FILE; }

  int open(const char*, int, uint32_t)
    { CURSOR_NEW_STATE("::open()"); lock.init(&share_lock); return 0;}

  drizzled::THR_LOCK_DATA lock;        /* MySQL lock */

  THR_LOCK_DATA **store_lock(Session *,
                                     THR_LOCK_DATA **to,
                             enum thr_lock_type);


private:
  string cursor_state;
  void CURSOR_NEW_STATE(const string &new_state);
};

THR_LOCK_DATA **SEAPITesterCursor::store_lock(Session *session,
                                              THR_LOCK_DATA **to,
                                              enum thr_lock_type lock_type)

{
  CURSOR_NEW_STATE("::store_lock()");

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK Table or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE)
        && !session_tablespace_op(session))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT)
      lock_type = TL_READ;

    lock.type=lock_type;
  }

  *to++= &lock;

  return(to);
}

void SEAPITesterCursor::CURSOR_NEW_STATE(const string &new_state)
{
  state_multimap_iter cur= cursor_state_transitions.find(cursor_state);
  if (cursor_state_transitions.count(cursor_state) == 0)
  {
    cerr << "ERROR: Invalid Cursor state: " << cursor_state << endl
         << "This should *NEVER* happen."
         << endl
         << "i.e. you've really screwed it up and you should be ashamed of "
         << "yourself." << endl;
    assert(cursor_state_transitions.count(cursor_state));
  }

  for(cur= cursor_state_transitions.lower_bound(cursor_state);
      cur != cursor_state_transitions.upper_bound(cursor_state);
      cur++)
  {
    if (new_state.compare((*cur).second) == 0)
      break;
  }

  if (cur == cursor_state_transitions.end()
      || new_state.compare((*cur).second))
  {
    cerr << "ERROR: Invalid Cursor state transition!" << endl
         << "Cannot go from " << cursor_state << " to " << new_state << endl;
    assert(false);
  }

  cursor_state= new_state;

  cerr << "\t\tCursor STATE : " << cursor_state << endl;
}

static const char *api_tester_exts[] = {
  NULL
};


class SEAPITester : public drizzled::plugin::TransactionalStorageEngine
{
public:
  SEAPITester(const string &name_arg)
    : drizzled::plugin::TransactionalStorageEngine(name_arg,
//                                                   HTON_SKIP_STORE_LOCK |
                                                   HTON_HAS_DOES_TRANSACTIONS)
  {
    ENGINE_NEW_STATE("::SEAPITester()");
  }

  ~SEAPITester()
  {
    ENGINE_NEW_STATE("::~SEAPITester()");
  }

  const char **bas_ext() const {
    return api_tester_exts;
  }

  virtual Cursor *create(TableShare &table)
  {
    return new SEAPITesterCursor(*this, table);
  }

  int doCreateTable(Session&,
                    Table&,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table& create_proto);

  int doDropTable(Session&, const TableIdentifier &identifier);

  int doRenameTable(drizzled::Session&,
                    const drizzled::TableIdentifier&,
                    const drizzled::TableIdentifier&)
    { return ENOENT; }

  int doGetTableDefinition(Session& ,
                           const TableIdentifier &,
                           drizzled::message::Table &);

  bool doDoesTableExist(Session&, const TableIdentifier &identifier);

  void doGetTableIdentifiers(drizzled::CachedDirectory &,
                             const drizzled::SchemaIdentifier &,
                             drizzled::TableIdentifiers &);

  virtual int doStartTransaction(Session *session,
                                 start_transaction_option_t options);
  virtual void doStartStatement(Session *session);
  virtual void doEndStatement(Session *session);

  virtual int doSetSavepoint(Session*,
                             drizzled::NamedSavepoint &)
    { return 0; }
  virtual int doRollbackToSavepoint(Session*,
                                     drizzled::NamedSavepoint &)
    { return 0; }
  virtual int doReleaseSavepoint(Session*,
                                 drizzled::NamedSavepoint &)
    { return 0; }
  virtual int doCommit(Session*, bool);

  virtual int doRollback(Session*, bool);


private:
  TableMessageMap table_messages;
};

bool SEAPITester::doDoesTableExist(Session&, const TableIdentifier &identifier)
{
  return table_messages.find(identifier.getPath()) != table_messages.end();
}

void SEAPITester::doGetTableIdentifiers(drizzled::CachedDirectory &,
                                        const drizzled::SchemaIdentifier &,
                                        drizzled::TableIdentifiers &)
{
}

int SEAPITester::doCreateTable(Session&,
                               Table&,
                               const drizzled::TableIdentifier &identifier,
                               drizzled::message::Table& create_proto)
{
  ENGINE_NEW_STATE("::doCreateTable()");

  if (table_messages.find(identifier.getPath()) != table_messages.end())
  {
    ENGINE_NEW_STATE("::SEAPITester()");
    return EEXIST;
  }

  table_messages.insert(make_pair(identifier.getPath(), create_proto));
  ENGINE_NEW_STATE("::SEAPITester()");
  return 0;
}

int SEAPITester::doDropTable(Session&, const TableIdentifier &identifier)
{
  if (table_messages.find(identifier.getPath()) == table_messages.end())
    return ENOENT;

  table_messages.erase(identifier.getPath());
  assert(table_messages.find(identifier.getPath()) == table_messages.end());
  return 0;
}

int SEAPITester::doGetTableDefinition(Session& ,
                                      const TableIdentifier &identifier,
                                      drizzled::message::Table &table)
{
  TableMessageMapIterator iter= table_messages.find(identifier.getPath());
  if (iter == table_messages.end())
  {
    return ENOENT;
  }

  table= (*iter).second;

  return EEXIST;
}

int SEAPITester::doStartTransaction(Session *,
                                    start_transaction_option_t )
{
  ENGINE_NEW_STATE("BEGIN");

  return 0;
}

void SEAPITester::doStartStatement(Session *)
{
  ENGINE_NEW_STATE("START STATEMENT");
}

void SEAPITester::doEndStatement(Session *)
{
  ENGINE_NEW_STATE("END STATEMENT");
}

int SEAPITester::doCommit(Session*, bool)
{
  ENGINE_NEW_STATE("COMMIT");
  ENGINE_NEW_STATE("::SEAPITester()");
  return 0;
}

int SEAPITester::doRollback(Session*, bool)
{
  ENGINE_NEW_STATE("ROLLBACK");
  ENGINE_NEW_STATE("::SEAPITester()");
  return 0;
}

static int seapi_tester_init(drizzled::module::Context &context)
{
  engine_state_transitions.insert(state_pair("INIT", "::SEAPITester()"));
  engine_state_transitions.insert(state_pair("::SEAPITester()", "::~SEAPITester()"));
  engine_state_transitions.insert(state_pair("::SEAPITester()", "::doCreateTable()"));
  engine_state_transitions.insert(state_pair("::doCreateTable()", "::SEAPITester()"));
/*  engine_state_transitions.insert(state_pair("::SEAPITester()", "::create()"));
    engine_state_transitions.insert(state_pair("::create()", "::SEAPITester()"));*/
  engine_state_transitions.insert(state_pair("::SEAPITester()", "BEGIN"));
  engine_state_transitions.insert(state_pair("BEGIN", "START STATEMENT"));

  /* really a bug */
  engine_state_transitions.insert(state_pair("BEGIN", "END STATEMENT"));
  /* also a bug */
  engine_state_transitions.insert(state_pair("::SEAPITester()", "COMMIT"));

  engine_state_transitions.insert(state_pair("BEGIN", "COMMIT"));
  engine_state_transitions.insert(state_pair("BEGIN", "ROLLBACK"));
  engine_state_transitions.insert(state_pair("START STATEMENT", "END STATEMENT"));
  engine_state_transitions.insert(state_pair("END STATEMENT", "START STATEMENT"));
  engine_state_transitions.insert(state_pair("END STATEMENT", "COMMIT"));
  engine_state_transitions.insert(state_pair("END STATEMENT", "ROLLBACK"));

  engine_state_transitions.insert(state_pair("COMMIT", "::SEAPITester()"));
  engine_state_transitions.insert(state_pair("ROLLBACK", "::SEAPITester()"));
  engine_state_transitions.insert(state_pair("::SEAPITester()", "::doGetTableDefinition()"));
  engine_state_transitions.insert(state_pair("::doGetTableDefinition()", "::SEAPITester()"));
  engine_state= "INIT";


  cursor_state_transitions.insert(state_pair("Cursor()", "::open()"));
  cursor_state_transitions.insert(state_pair("::open()", "::store_lock()"));
  cursor_state_transitions.insert(state_pair("::store_lock()", "::doStartTableScan()"));
  cursor_state_transitions.insert(state_pair("::open()", "::doStartTableScan()"));
  cursor_state_transitions.insert(state_pair("::doStartTableScan()", "::rnd_next()"));


  /* below just for autocommit statement. doesn't seem right to me */
  engine_state_transitions.insert(state_pair("::SEAPITester()", "START STATEMENT"));

  thr_lock_init(&share_lock); /* HACK for store_lock */

  context.add(new SEAPITester(engine_name));
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "SEAPITESTER",
  "1.0",
  "Stewart Smith",
  "Test the Storage Engine API callls are in correct order",
  PLUGIN_LICENSE_GPL,
  seapi_tester_init,     /* Plugin Init */
  NULL, /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
