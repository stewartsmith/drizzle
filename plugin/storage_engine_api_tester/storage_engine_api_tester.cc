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

#include <config.h>
#include <drizzled/table.h>
#include <drizzled/error.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/session.h> // for mark_transaction_to_rollback
#include <string>
#include <map>
#include <fstream>
#include <drizzled/message/table.pb.h>
#include <drizzled/internal/m_string.h>

#include <drizzled/charset.h>

#include <boost/unordered_map.hpp>

#include "engine_state_history.h"

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

uint64_t next_cursor_id;

plugin::TransactionalStorageEngine *realEngine;

/* ERROR INJECTION For SEAPITESTER
   -------------------------------

   IF you add a new error injection, document it here!
   You test via error_injected variable.

   Conflicting error inject numbers will lead to tears
   (not Borsch, Vodka and Tears - that's quite nice).

   0 - DISABLED

   1 - doInsertRecord(): every 2nd row, LOCK_WAIT_TIMEOUT.
   2 - doInsertRecord(): every 2nd row, DEADLOCK.
   3 - rnd_next(): every 2nd row, LOCK_WAIT_TIMEOUT
   4 - doStartIndexScan returns an error.
 */
static uint32_t error_injected= 0;

#include <drizzled/function/math/int.h>
#include <drizzled/plugin/function.h>

class SEAPITesterErrorInjectFunc :public Item_int_func
{
public:
  int64_t val_int();
  SEAPITesterErrorInjectFunc() :Item_int_func() {}

  const char *func_name() const
  {
    return "seapitester_error_inject";
  }

  void fix_length_and_dec()
  {
    max_length= 4;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};


int64_t SEAPITesterErrorInjectFunc::val_int()
{
  assert(fixed == true);
  uint32_t err_to_inject= args[0]->val_int();

  error_injected= err_to_inject;

  return error_injected;
}

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
  engine_state_history.push_back(new_state);

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
    {
      cursor_state= "Cursor()";
      realCursor= NULL;
      id= ++next_cursor_id;
      CURSOR_NEW_STATE("Cursor()");
    }

  ~SEAPITesterCursor()
    { CURSOR_NEW_STATE("~Cursor()"); delete realCursor;}

  int close();
  int rnd_next(unsigned char *buf) {
    static int count= 0;
    CURSOR_NEW_STATE("::rnd_next()");

    if (error_injected == 3 && (count++ % 2))
    {
      user_session->markTransactionForRollback(false);
      return HA_ERR_LOCK_WAIT_TIMEOUT;
    }
    return realCursor->rnd_next(buf);
  }

  int rnd_pos(unsigned char* buf, unsigned char* pos) { CURSOR_NEW_STATE("::rnd_pos()"); return realCursor->rnd_pos(buf, pos); }
  void position(const unsigned char *record);
  int info(uint32_t flag);

  int reset();

  void get_auto_increment(uint64_t, uint64_t, uint64_t, uint64_t*, uint64_t*) {}
  int doStartTableScan(bool scan) { CURSOR_NEW_STATE("::doStartTableScan()"); return realCursor->doStartTableScan(scan); }
  int doEndTableScan() { CURSOR_NEW_STATE("::doEndTableScan()"); return realCursor->doEndTableScan(); }

  const char *index_type(uint32_t key_number);

  int doStartIndexScan(uint32_t, bool);
  int index_read(unsigned char *buf, const unsigned char *key_ptr,
                 uint32_t key_len, drizzled::ha_rkey_function find_flag);
  int index_read_idx_map(unsigned char * buf,
                         uint32_t index,
                         const unsigned char * key,
                         drizzled::key_part_map keypart_map,
                         drizzled::ha_rkey_function find_flag);

  int index_next(unsigned char * buf);
  int doEndIndexScan();
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);

  bool primary_key_is_clustered()
  {
    return realCursor->primary_key_is_clustered();
  }


  int doOpen(const identifier::Table &identifier, int mode, uint32_t test_if_locked);

  THR_LOCK_DATA **store_lock(Session *,
                                     THR_LOCK_DATA **to,
                             enum thr_lock_type);

  int external_lock(Session *session, int lock_type);

  int doInsertRecord(unsigned char *buf)
  {
    static int i=0;
    CURSOR_NEW_STATE("::doInsertRecord()");

    if (error_injected == 1 && (i++ % 2))
    {
      user_session->markTransactionForRollback(false);
      return HA_ERR_LOCK_WAIT_TIMEOUT;
    }

    if (error_injected == 2 && (i++ % 2))
    {
      user_session->markTransactionForRollback(true);
      return HA_ERR_LOCK_DEADLOCK;
    }

    return realCursor->doInsertRecord(buf);
  }

  int doUpdateRecord(const unsigned char *old_row, unsigned char *new_row)
  {
    CURSOR_NEW_STATE("::doUpdateRecord()");
    return realCursor->doUpdateRecord(old_row, new_row);
  }

  double scan_time()
  {
    CURSOR_NEW_STATE("::scan_time()");
    CURSOR_NEW_STATE("locked");
    return realCursor->scan_time();
  }

  int extra(enum ha_extra_function operation)
  {
    return realCursor->extra(operation);
  }

private:
  string cursor_state;
  void CURSOR_NEW_STATE(const string &new_state);
  Session* user_session;
  uint64_t id;
};

int SEAPITesterCursor::doOpen(const identifier::Table &identifier, int mode, uint32_t test_if_locked)
{
  CURSOR_NEW_STATE("::doOpen()");

  int r= realCursor->doOpen(identifier, mode, test_if_locked);

  ref_length= realCursor->ref_length;

  return r;
}

int SEAPITesterCursor::reset()
{
  CURSOR_NEW_STATE("::reset()");
  CURSOR_NEW_STATE("::doOpen()");

  return realCursor->reset();
}

int SEAPITesterCursor::close()
{
  CURSOR_NEW_STATE("::close()");
  CURSOR_NEW_STATE("Cursor()");

  return realCursor->close();
}

void SEAPITesterCursor::position(const unsigned char *record)
{
  CURSOR_NEW_STATE("::position()");

  /* We need to use the correct buffer for upper layer */
  realCursor->ref= ref;

  realCursor->position(record);
}

int SEAPITesterCursor::info(uint32_t flag)
{
  int r;
  CURSOR_NEW_STATE("::info()");
  CURSOR_NEW_STATE("locked");

  r= realCursor->info(flag);

  if (flag & (HA_STATUS_VARIABLE|HA_STATUS_AUTO|HA_STATUS_CONST))
  {
    stats= realCursor->stats;
  }

  if (flag & HA_STATUS_ERRKEY)
    errkey= realCursor->errkey;

  return r;
}

const char * SEAPITesterCursor::index_type(uint32_t key_number)
{
  CURSOR_NEW_STATE("::index_type()");
  return realCursor->index_type(key_number);
}

int SEAPITesterCursor::doStartIndexScan(uint32_t keynr, bool scan)
{
  int r;
  CURSOR_NEW_STATE("::doStartIndexScan()");

  if (error_injected == 4)
  {
    CURSOR_NEW_STATE("::doStartIndexScan() ERROR");
    CURSOR_NEW_STATE("locked");
    return HA_ERR_LOCK_DEADLOCK;
  }

  r= realCursor->doStartIndexScan(keynr, scan);

  active_index= realCursor->get_index();

  return r;
}

int SEAPITesterCursor::index_read(unsigned char *buf,
                                  const unsigned char *key_ptr,
                                  uint32_t key_len,
                                  drizzled::ha_rkey_function find_flag)
{
  CURSOR_NEW_STATE("::index_read()");
  CURSOR_NEW_STATE("::doStartIndexScan()");
  return realCursor->index_read(buf, key_ptr, key_len, find_flag);
}

int SEAPITesterCursor::index_read_idx_map(unsigned char * buf,
                                          uint32_t index,
                                          const unsigned char * key,
                                          drizzled::key_part_map keypart_map,
                                          drizzled::ha_rkey_function find_flag)
{
  CURSOR_NEW_STATE("::index_read_idx_map()");
  CURSOR_NEW_STATE("locked");
  return realCursor->index_read_idx_map(buf, index, key, keypart_map, find_flag);
}

int SEAPITesterCursor::index_next(unsigned char * buf)
{
  CURSOR_NEW_STATE("::index_next()");
  CURSOR_NEW_STATE("::doStartIndexScan()");
  return realCursor->index_next(buf);
}

int SEAPITesterCursor::doEndIndexScan()
{
  CURSOR_NEW_STATE("::doEndIndexScan()");
  CURSOR_NEW_STATE("locked");
  int r= realCursor->doEndIndexScan();

  active_index= realCursor->get_index();

  return r;
}

int SEAPITesterCursor::index_prev(unsigned char * buf)
{
  CURSOR_NEW_STATE("::index_prev()");
  CURSOR_NEW_STATE("::doStartIndexScan()");
  return realCursor->index_prev(buf);
}

int SEAPITesterCursor::index_first(unsigned char * buf)
{
  CURSOR_NEW_STATE("::index_first()");
  CURSOR_NEW_STATE("::doStartIndexScan()");
  return realCursor->index_first(buf);
}

int SEAPITesterCursor::index_last(unsigned char * buf)
{
  CURSOR_NEW_STATE("::index_last()");
  CURSOR_NEW_STATE("::doStartIndexScan()");
  return realCursor->index_last(buf);
}

int SEAPITesterCursor::external_lock(Session *session, int lock_type)
{
  CURSOR_NEW_STATE("::external_lock()");
  CURSOR_NEW_STATE("locked");

  user_session= session;

  return realCursor->external_lock(session, lock_type);
}

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
         << "Cursor " << this << "Cannot go from "
         << cursor_state << " to " << new_state << endl;
    assert(false);
  }

  cursor_state= new_state;

  std::string cursor_state_str("Cursor ");
  char nr[50];
  snprintf(nr, sizeof(nr), "%"PRIu64, this->id);
  cursor_state_str.append(nr);
  cursor_state_str.append(" ");
  cursor_state_str.append(cursor_state);

  engine_state_history.push_back(cursor_state_str);

  cerr << "\t\tCursor " << this << " STATE : " << cursor_state << endl;
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
  /* BUG: Currently flags are just copy&pasted from innobase. Instead, we
     need to have a call somewhere.
   */
  SEAPITester(const string &name_arg)
    : drizzled::plugin::TransactionalStorageEngine(name_arg,
                            HTON_NULL_IN_KEY |
                            HTON_CAN_INDEX_BLOBS |
                            HTON_PRIMARY_KEY_IN_READ_INDEX |
                            HTON_PARTIAL_COLUMN_READ |
                            HTON_TABLE_SCAN_ON_INDEX |
                            HTON_HAS_FOREIGN_KEYS |
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
                    const drizzled::identifier::Table &identifier,
                    const drizzled::message::Table& create_proto);

  int doDropTable(Session&, const identifier::Table &identifier);

  int doRenameTable(drizzled::Session& session,
                    const drizzled::identifier::Table& from,
                    const drizzled::identifier::Table& to)
    { return getRealEngine()->renameTable(session, from, to); }

  int doGetTableDefinition(Session& ,
                           const identifier::Table &,
                           drizzled::message::Table &);

  bool doDoesTableExist(Session&, const identifier::Table &identifier);

  void doGetTableIdentifiers(drizzled::CachedDirectory &,
                             const drizzled::identifier::Schema &,
                             drizzled::identifier::table::vector &);

  virtual int doStartTransaction(Session *session,
                                 start_transaction_option_t options);
  virtual void doStartStatement(Session *session);
  virtual void doEndStatement(Session *session);

  virtual int doSetSavepoint(Session*,
                             drizzled::NamedSavepoint &)
    {
      ENGINE_NEW_STATE("SET SAVEPOINT");
      ENGINE_NEW_STATE("In Transaction");
      return 0; }
  virtual int doRollbackToSavepoint(Session*,
                                     drizzled::NamedSavepoint &)
    {
      ENGINE_NEW_STATE("ROLLBACK TO SAVEPOINT");
      ENGINE_NEW_STATE("In Transaction");
      return 0; }
  virtual int doReleaseSavepoint(Session*,
                                 drizzled::NamedSavepoint &)
    {
      ENGINE_NEW_STATE("RELEASE SAVEPOINT");
      ENGINE_NEW_STATE("In Transaction");
      return 0; }
  virtual int doCommit(Session*, bool);

  virtual int doRollback(Session*, bool);

  uint32_t max_supported_record_length(void) const {
    ENGINE_NEW_STATE("::max_supported_record_length()");
    return getRealEngine()->max_supported_record_length();
  }

  uint32_t max_supported_keys(void) const {
    ENGINE_NEW_STATE("::max_supported_keys()");
    return getRealEngine()->max_supported_keys();
  }

  uint32_t max_supported_key_parts(void) const {
    ENGINE_NEW_STATE("::max_supported_key_parts()");
    return getRealEngine()->max_supported_key_parts();
  }

  uint32_t max_supported_key_length(void) const {
    ENGINE_NEW_STATE("::max_supported_key_length()");
    return getRealEngine()->max_supported_key_length();
  }

  uint32_t max_supported_key_part_length(void) const {
    ENGINE_NEW_STATE("::max_supported_key_part_length()");
    return getRealEngine()->max_supported_key_part_length();
  }

  /* just copied from innobase... */
  uint32_t index_flags(enum  ha_key_alg) const
  {
    return (HA_READ_NEXT |
            HA_READ_PREV |
            HA_READ_RANGE |
            HA_READ_ORDER |
            HA_KEYREAD_ONLY);
  }

};

bool SEAPITester::doDoesTableExist(Session &session, const identifier::Table &identifier)
{
  return getRealEngine()->doDoesTableExist(session, identifier);
}

void SEAPITester::doGetTableIdentifiers(drizzled::CachedDirectory &cd,
                                        const drizzled::identifier::Schema &si,
                                        drizzled::identifier::table::vector &ti)
{
  return getRealEngine()->doGetTableIdentifiers(cd, si, ti);
}

int SEAPITester::doCreateTable(Session& session,
                               Table& table,
                               const drizzled::identifier::Table &identifier,
                               const drizzled::message::Table& create_proto)
{
  ENGINE_NEW_STATE("::doCreateTable()");

  int r= getRealEngine()->doCreateTable(session, table, identifier, create_proto);

  ENGINE_NEW_STATE("::SEAPITester()");
  return r;
}

int SEAPITester::doDropTable(Session& session, const identifier::Table &identifier)
{
  return getRealEngine()->doDropTable(session, identifier);
}

int SEAPITester::doGetTableDefinition(Session& session,
                                      const identifier::Table &identifier,
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
    ENGINE_NEW_STATE("ROLLBACK");
    ENGINE_NEW_STATE("::SEAPITester()");
  }
  else
  {
    ENGINE_NEW_STATE("ROLLBACK STATEMENT");
    ENGINE_NEW_STATE("In Transaction");
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

  context.add(new plugin::Create_function<SEAPITesterErrorInjectFunc>("seapitester_error_inject"));

  engine_state_history_table_initialize(context);

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
  NULL, /* depends */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
