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

string engine_state;
typedef multimap<string, string> state_multimap;
typedef multimap<string, string>::value_type state_pair;
typedef multimap<string, string>::iterator state_multimap_iter;
state_multimap engine_state_transitions;
state_multimap cursor_state_transitions;

void load_engine_state_transitions(state_multimap &states);
void load_cursor_state_transitions(state_multimap &states);

plugin::TransactionalStorageEngine *realEngine;

static plugin::TransactionalStorageEngine *getRealEngine()
{
  return down_cast<plugin::TransactionalStorageEngine*>(plugin::StorageEngine::findByName("INNODB"));
}

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
namespace drizzled {
class SEAPITesterCursor : public drizzled::Cursor
{
  friend class drizzled::Cursor;
public:
  drizzled::Cursor *realCursor;

  SEAPITesterCursor(drizzled::plugin::StorageEngine &engine_arg,
                    drizzled::Table &table_arg)
    : Cursor(engine_arg, table_arg)
    { cursor_state= "Cursor()"; realCursor= NULL;}

  ~SEAPITesterCursor()
    { delete realCursor;}

  int close() { CURSOR_NEW_STATE("::close()"); CURSOR_NEW_STATE("Cursor()");return realCursor->close(); }
  int rnd_next(unsigned char *buf) { CURSOR_NEW_STATE("::rnd_next()"); return realCursor->rnd_next(buf); }
  int rnd_pos(unsigned char*, unsigned char*) { CURSOR_NEW_STATE("::rnd_pos()"); return -1; }
  void position(const unsigned char*) { CURSOR_NEW_STATE("::position()"); return; }
  int info(uint32_t) { return 0; }
  void get_auto_increment(uint64_t, uint64_t, uint64_t, uint64_t*, uint64_t*) {}
  int doStartTableScan(bool scan) { CURSOR_NEW_STATE("::doStartTableScan()"); return realCursor->doStartTableScan(scan); }
  int doEndTableScan() { CURSOR_NEW_STATE("::doEndTableScan()"); return realCursor->doEndTableScan(); }

  int doOpen(const TableIdentifier &identifier, int mode, uint32_t test_if_locked)
    { CURSOR_NEW_STATE("::doOpen()"); return realCursor->doOpen(identifier, mode, test_if_locked);}

  THR_LOCK_DATA **store_lock(Session *,
                                     THR_LOCK_DATA **to,
                             enum thr_lock_type);

  int doInsertRecord(unsigned char *buf)
  {
    CURSOR_NEW_STATE("::doInsertRecord()");
    CURSOR_NEW_STATE("::store_lock()");
    return realCursor->doInsertRecord(buf);
  }

private:
  string cursor_state;
  void CURSOR_NEW_STATE(const string &new_state);
};

THR_LOCK_DATA **SEAPITesterCursor::store_lock(Session *session,
                                              THR_LOCK_DATA **to,
                                              enum thr_lock_type lock_type)

{
  CURSOR_NEW_STATE("::store_lock()");

  return realCursor->store_lock(session, to, lock_type);
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

} /* namespace drizzled */

static const char *api_tester_exts[] = {
  NULL
};

namespace drizzled {
  namespace plugin {
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

  virtual Cursor *create(Table &table)
  {
    SEAPITesterCursor *c= new SEAPITesterCursor(*this, table);
    Cursor *realCursor= getRealEngine()->create(table);
    c->realCursor= realCursor;

    return c;
  }

  int doCreateTable(Session&,
                    Table&,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table& create_proto);

  int doDropTable(Session&, const TableIdentifier &identifier);

  int doRenameTable(drizzled::Session& session,
                    const drizzled::TableIdentifier& from,
                    const drizzled::TableIdentifier& to)
    { return getRealEngine()->renameTable(session, from, to); }

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
};

bool SEAPITester::doDoesTableExist(Session &session, const TableIdentifier &identifier)
{
  return getRealEngine()->doDoesTableExist(session, identifier);
}

void SEAPITester::doGetTableIdentifiers(drizzled::CachedDirectory &cd,
                                        const drizzled::SchemaIdentifier &si,
                                        drizzled::TableIdentifiers &ti)
{
  return getRealEngine()->doGetTableIdentifiers(cd, si, ti);
}

int SEAPITester::doCreateTable(Session& session,
                               Table& table,
                               const drizzled::TableIdentifier &identifier,
                               drizzled::message::Table& create_proto)
{
  ENGINE_NEW_STATE("::doCreateTable()");

  int r= getRealEngine()->doCreateTable(session, table, identifier, create_proto);

  ENGINE_NEW_STATE("::SEAPITester()");
  return r;
}

int SEAPITester::doDropTable(Session& session, const TableIdentifier &identifier)
{
  return getRealEngine()->doDropTable(session, identifier);
}

int SEAPITester::doGetTableDefinition(Session& session,
                                      const TableIdentifier &identifier,
                                      drizzled::message::Table &table)
{
  return getRealEngine()->doGetTableDefinition(session, identifier, table);
}

int SEAPITester::doStartTransaction(Session *session,
                                    start_transaction_option_t opt)
{
  ENGINE_NEW_STATE("BEGIN");
  ENGINE_NEW_STATE("In Transaction");

  return getRealEngine()->startTransaction(session, opt);
}

void SEAPITester::doStartStatement(Session *session)
{
  ENGINE_NEW_STATE("START STATEMENT");
  return getRealEngine()->startStatement(session);
}

void SEAPITester::doEndStatement(Session *session)
{
  ENGINE_NEW_STATE("END STATEMENT");
  return getRealEngine()->endStatement(session);
}

int SEAPITester::doCommit(Session *session, bool all)
{
  if (all)
  {
    ENGINE_NEW_STATE("COMMIT");
    ENGINE_NEW_STATE("::SEAPITester()");
  }
  else
  {
    ENGINE_NEW_STATE("COMMIT STATEMENT");
    ENGINE_NEW_STATE("In Transaction");
  }
  return getRealEngine()->commit(session, all);
}

int SEAPITester::doRollback(Session *session, bool all)
{
  if (all)
  {
    ENGINE_NEW_STATE("ROLLBACK STATEMENT");
    ENGINE_NEW_STATE("In Transaction");
  }
  else
  {
    ENGINE_NEW_STATE("ROLLBACK");
    ENGINE_NEW_STATE("::SEAPITester()");
  }
  return getRealEngine()->rollback(session, all);
}

  } /* namespace plugin */
} /* namespace drizzled */

static int seapi_tester_init(drizzled::module::Context &context)
{
  load_engine_state_transitions(engine_state_transitions);
  load_cursor_state_transitions(cursor_state_transitions);
  engine_state= "INIT";

  context.add(new plugin::SEAPITester(engine_name));
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
