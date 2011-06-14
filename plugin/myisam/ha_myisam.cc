/* Copyright (C) 2000-2006 MySQL AB

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



#include <config.h>
#include <drizzled/internal/my_bit.h>
#include "myisampack.h"
#include "ha_myisam.h"
#include "myisam_priv.h"
#include <drizzled/option.h>
#include <drizzled/internal/my_bit.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/util/test.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/client.h>
#include <drizzled/table.h>
#include <drizzled/memory/multi_malloc.h>
#include <drizzled/plugin/daemon.h>
#include <drizzled/session/table_messages.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/key.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>

#include <boost/algorithm/string.hpp>
#include <boost/scoped_ptr.hpp>

#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <memory>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;

static const string engine_name("MyISAM");

boost::mutex THR_LOCK_myisam;

static uint32_t myisam_key_cache_block_size= KEY_CACHE_BLOCK_SIZE;
static uint32_t myisam_key_cache_size;
static uint32_t myisam_key_cache_division_limit;
static uint32_t myisam_key_cache_age_threshold;
static uint64_t max_sort_file_size;
typedef constrained_check<size_t, SIZE_MAX, 1024, 1024> sort_buffer_constraint;
static sort_buffer_constraint sort_buffer_size;

void st_mi_isam_share::setKeyCache()
{
  (void)init_key_cache(&key_cache,
                       myisam_key_cache_block_size,
                       myisam_key_cache_size,
                       myisam_key_cache_division_limit, 
                       myisam_key_cache_age_threshold);
}

/*****************************************************************************
** MyISAM tables
*****************************************************************************/

static const char *ha_myisam_exts[] = {
  ".MYI",
  ".MYD",
  NULL
};

class MyisamEngine : public plugin::StorageEngine
{
  MyisamEngine();
  MyisamEngine(const MyisamEngine&);
  MyisamEngine& operator=(const MyisamEngine&);
public:
  explicit MyisamEngine(string name_arg) :
    plugin::StorageEngine(name_arg,
                          HTON_CAN_INDEX_BLOBS |
                          HTON_STATS_RECORDS_IS_EXACT |
                          HTON_TEMPORARY_ONLY |
                          HTON_NULL_IN_KEY |
                          HTON_HAS_RECORDS |
                          HTON_DUPLICATE_POS |
                          HTON_AUTO_PART_KEY |
                          HTON_SKIP_STORE_LOCK)
  {
  }

  virtual ~MyisamEngine()
  { 
    mi_panic(HA_PANIC_CLOSE);
  }

  virtual Cursor *create(Table &table)
  {
    return new ha_myisam(*this, table);
  }

  const char **bas_ext() const {
    return ha_myisam_exts;
  }

  int doCreateTable(Session&,
                    Table& table_arg,
                    const identifier::Table &identifier,
                    const message::Table&);

  int doRenameTable(Session&, const identifier::Table &from, const identifier::Table &to);

  int doDropTable(Session&, const identifier::Table &identifier);

  int doGetTableDefinition(Session& session,
                           const identifier::Table &identifier,
                           message::Table &table_message);

  uint32_t max_supported_keys()          const { return MI_MAX_KEY; }
  uint32_t max_supported_key_length()    const { return MI_MAX_KEY_LENGTH; }
  uint32_t max_supported_key_part_length() const { return MI_MAX_KEY_LENGTH; }

  uint32_t index_flags(enum  ha_key_alg) const
  {
    return (HA_READ_NEXT |
            HA_READ_PREV |
            HA_READ_RANGE |
            HA_READ_ORDER |
            HA_KEYREAD_ONLY);
  }
  bool doDoesTableExist(Session& session, const identifier::Table &identifier);

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::table::vector &set_of_identifiers);
  bool validateCreateTableOption(const std::string &key, const std::string &state)
  {
    (void)state;
    if (boost::iequals(key, "ROW_FORMAT"))
    {
      return true;
    }

    return false;
  }
};

void MyisamEngine::doGetTableIdentifiers(drizzled::CachedDirectory&,
                                         const drizzled::identifier::Schema&,
                                         drizzled::identifier::table::vector&)
{
}

bool MyisamEngine::doDoesTableExist(Session &session, const identifier::Table &identifier)
{
  return session.getMessageCache().doesTableMessageExist(identifier);
}

int MyisamEngine::doGetTableDefinition(Session &session,
                                       const identifier::Table &identifier,
                                       message::Table &table_message)
{
  if (session.getMessageCache().getTableMessage(identifier, table_message))
    return EEXIST;
  return ENOENT;
}

/* 
  Convert to push_Warnings if you ever care about this, otherwise, it is a no-op.
*/

static void mi_check_print_msg(MI_CHECK *,	const char* ,
                               const char *, va_list )
{
}


/*
  Convert Table object to MyISAM key and column definition

  SYNOPSIS
    table2myisam()
      table_arg   in     Table object.
      keydef_out  out    MyISAM key definition.
      recinfo_out out    MyISAM column definition.
      records_out out    Number of fields.

  DESCRIPTION
    This function will allocate and initialize MyISAM key and column
    definition for further use in mi_create or for a check for underlying
    table conformance in merge engine.

    The caller needs to free *recinfo_out after use. Since *recinfo_out
    and *keydef_out are allocated with a multi_malloc, *keydef_out
    is freed automatically when *recinfo_out is freed.

  RETURN VALUE
    0  OK
    !0 error code
*/

static int table2myisam(Table *table_arg, MI_KEYDEF **keydef_out,
                        MI_COLUMNDEF **recinfo_out, uint32_t *records_out)
{
  uint32_t i, j, recpos, minpos, fieldpos, temp_length, length;
  enum ha_base_keytype type= HA_KEYTYPE_BINARY;
  unsigned char *record;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo, *recinfo_pos;
  HA_KEYSEG *keyseg;
  TableShare *share= table_arg->getMutableShare();
  uint32_t options= share->db_options_in_use;
  if (!(memory::multi_malloc(false,
          recinfo_out, (share->sizeFields() * 2 + 2) * sizeof(MI_COLUMNDEF),
          keydef_out, share->sizeKeys() * sizeof(MI_KEYDEF),
          &keyseg, (share->key_parts + share->sizeKeys()) * sizeof(HA_KEYSEG),
          NULL)))
    return(HA_ERR_OUT_OF_MEM);
  keydef= *keydef_out;
  recinfo= *recinfo_out;
  for (i= 0; i < share->sizeKeys(); i++)
  {
    KeyInfo *pos= &table_arg->key_info[i];
    keydef[i].flag= ((uint16_t) pos->flags & (HA_NOSAME));
    keydef[i].key_alg= HA_KEY_ALG_BTREE;
    keydef[i].block_length= pos->block_size;
    keydef[i].seg= keyseg;
    keydef[i].keysegs= pos->key_parts;
    for (j= 0; j < pos->key_parts; j++)
    {
      Field *field= pos->key_part[j].field;
      type= field->key_type();
      keydef[i].seg[j].flag= pos->key_part[j].key_part_flag;

      if (options & HA_OPTION_PACK_KEYS ||
          (pos->flags & (HA_PACK_KEY | HA_BINARY_PACK_KEY |
                         HA_SPACE_PACK_USED)))
      {
        if (pos->key_part[j].length > 8 &&
            (type == HA_KEYTYPE_TEXT ||
             (type == HA_KEYTYPE_BINARY && !field->zero_pack())))
        {
          /* No blobs here */
          if (j == 0)
            keydef[i].flag|= HA_PACK_KEY;
          if ((((int) (pos->key_part[j].length - field->decimals())) >= 4))
            keydef[i].seg[j].flag|= HA_SPACE_PACK;
        }
        else if (j == 0 && (!(pos->flags & HA_NOSAME) || pos->key_length > 16))
          keydef[i].flag|= HA_BINARY_PACK_KEY;
      }
      keydef[i].seg[j].type= (int) type;
      keydef[i].seg[j].start= pos->key_part[j].offset;
      keydef[i].seg[j].length= pos->key_part[j].length;
      keydef[i].seg[j].bit_start= keydef[i].seg[j].bit_end=
        keydef[i].seg[j].bit_length= 0;
      keydef[i].seg[j].bit_pos= 0;
      keydef[i].seg[j].language= field->charset()->number;

      if (field->null_ptr)
      {
        keydef[i].seg[j].null_bit= field->null_bit;
        keydef[i].seg[j].null_pos= (uint) (field->null_ptr-
                                           (unsigned char*) table_arg->getInsertRecord());
      }
      else
      {
        keydef[i].seg[j].null_bit= 0;
        keydef[i].seg[j].null_pos= 0;
      }
      if (field->type() == DRIZZLE_TYPE_BLOB)
      {
        keydef[i].seg[j].flag|= HA_BLOB_PART;
        /* save number of bytes used to pack length */
        keydef[i].seg[j].bit_start= (uint) (field->pack_length() -
                                            share->sizeBlobPtr());
      }
    }
    keyseg+= pos->key_parts;
  }
  if (table_arg->found_next_number_field)
    keydef[share->next_number_index].flag|= HA_AUTO_KEY;
  record= table_arg->getInsertRecord();
  recpos= 0;
  recinfo_pos= recinfo;

  while (recpos < (uint) share->sizeStoredRecord())
  {
    Field **field, *found= 0;
    minpos= share->getRecordLength();
    length= 0;

    for (field= table_arg->getFields(); *field; field++)
    {
      if ((fieldpos= (*field)->offset(record)) >= recpos &&
          fieldpos <= minpos)
      {
        /* skip null fields */
        if (!(temp_length= (*field)->pack_length_in_rec()))
          continue; /* Skip null-fields */

        if (! found || fieldpos < minpos ||
            (fieldpos == minpos && temp_length < length))
        {
          minpos= fieldpos;
          found= *field;
          length= temp_length;
        }
      }
    }
    if (recpos != minpos)
    { // Reserved space (Null bits?)
      memset(recinfo_pos, 0, sizeof(*recinfo_pos));
      recinfo_pos->type= (int) FIELD_NORMAL;
      recinfo_pos++->length= (uint16_t) (minpos - recpos);
    }
    if (!found)
      break;

    if (found->flags & BLOB_FLAG)
      recinfo_pos->type= (int) FIELD_BLOB;
    else if (found->type() == DRIZZLE_TYPE_VARCHAR)
      recinfo_pos->type= FIELD_VARCHAR;
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->type= (int) FIELD_SKIP_ZERO;
    else
      recinfo_pos->type= (int) ((length <= 3) ?  FIELD_NORMAL : FIELD_SKIP_PRESPACE);
    if (found->null_ptr)
    {
      recinfo_pos->null_bit= found->null_bit;
      recinfo_pos->null_pos= (uint) (found->null_ptr -
                                     (unsigned char*) table_arg->getInsertRecord());
    }
    else
    {
      recinfo_pos->null_bit= 0;
      recinfo_pos->null_pos= 0;
    }
    (recinfo_pos++)->length= (uint16_t) length;
    recpos= minpos + length;
  }
  *records_out= (uint) (recinfo_pos - recinfo);
  return(0);
}

int ha_myisam::reset_auto_increment(uint64_t value)
{
  file->s->state.auto_increment= value;
  return 0;
}

/*
  Check for underlying table conformance

  SYNOPSIS
    check_definition()
      t1_keyinfo       in    First table key definition
      t1_recinfo       in    First table record definition
      t1_keys          in    Number of keys in first table
      t1_recs          in    Number of records in first table
      t2_keyinfo       in    Second table key definition
      t2_recinfo       in    Second table record definition
      t2_keys          in    Number of keys in second table
      t2_recs          in    Number of records in second table
      strict           in    Strict check switch

  DESCRIPTION
    This function compares two MyISAM definitions. By intention it was done
    to compare merge table definition against underlying table definition.
    It may also be used to compare dot-frm and MYI definitions of MyISAM
    table as well to compare different MyISAM table definitions.

    For merge table it is not required that number of keys in merge table
    must exactly match number of keys in underlying table. When calling this
    function for underlying table conformance check, 'strict' flag must be
    set to false, and converted merge definition must be passed as t1_*.

    Otherwise 'strict' flag must be set to 1 and it is not required to pass
    converted dot-frm definition as t1_*.

  RETURN VALUE
    0 - Equal definitions.
    1 - Different definitions.

  TODO
    - compare FULLTEXT keys;
    - compare SPATIAL keys;
    - compare FIELD_SKIP_ZERO which is converted to FIELD_NORMAL correctly
      (should be corretly detected in table2myisam).
*/

static int check_definition(MI_KEYDEF *t1_keyinfo, MI_COLUMNDEF *t1_recinfo,
                            uint32_t t1_keys, uint32_t t1_recs,
                            MI_KEYDEF *t2_keyinfo, MI_COLUMNDEF *t2_recinfo,
                            uint32_t t2_keys, uint32_t t2_recs, bool strict)
{
  uint32_t i, j;
  if ((strict ? t1_keys != t2_keys : t1_keys > t2_keys))
  {
    return(1);
  }
  if (t1_recs != t2_recs)
  {
    return(1);
  }
  for (i= 0; i < t1_keys; i++)
  {
    HA_KEYSEG *t1_keysegs= t1_keyinfo[i].seg;
    HA_KEYSEG *t2_keysegs= t2_keyinfo[i].seg;
    if (t1_keyinfo[i].keysegs != t2_keyinfo[i].keysegs ||
        t1_keyinfo[i].key_alg != t2_keyinfo[i].key_alg)
    {
      return(1);
    }
    for (j=  t1_keyinfo[i].keysegs; j--;)
    {
      uint8_t t1_keysegs_j__type= t1_keysegs[j].type;

      /*
        Table migration from 4.1 to 5.1. In 5.1 a *TEXT key part is
        always HA_KEYTYPE_VARTEXT2. In 4.1 we had only the equivalent of
        HA_KEYTYPE_VARTEXT1. Since we treat both the same on MyISAM
        level, we can ignore a mismatch between these types.
      */
      if ((t1_keysegs[j].flag & HA_BLOB_PART) &&
          (t2_keysegs[j].flag & HA_BLOB_PART))
      {
        if ((t1_keysegs_j__type == HA_KEYTYPE_VARTEXT2) &&
            (t2_keysegs[j].type == HA_KEYTYPE_VARTEXT1))
          t1_keysegs_j__type= HA_KEYTYPE_VARTEXT1;
        else if ((t1_keysegs_j__type == HA_KEYTYPE_VARBINARY2) &&
                 (t2_keysegs[j].type == HA_KEYTYPE_VARBINARY1))
          t1_keysegs_j__type= HA_KEYTYPE_VARBINARY1;
      }

      if (t1_keysegs_j__type != t2_keysegs[j].type ||
          t1_keysegs[j].language != t2_keysegs[j].language ||
          t1_keysegs[j].null_bit != t2_keysegs[j].null_bit ||
          t1_keysegs[j].length != t2_keysegs[j].length)
      {
        return(1);
      }
    }
  }
  for (i= 0; i < t1_recs; i++)
  {
    MI_COLUMNDEF *t1_rec= &t1_recinfo[i];
    MI_COLUMNDEF *t2_rec= &t2_recinfo[i];
    /*
      FIELD_SKIP_ZERO can be changed to FIELD_NORMAL in mi_create,
      see NOTE1 in mi_create.c
    */
    if ((t1_rec->type != t2_rec->type &&
         !(t1_rec->type == (int) FIELD_SKIP_ZERO &&
           t1_rec->length == 1 &&
           t2_rec->type == (int) FIELD_NORMAL)) ||
        t1_rec->length != t2_rec->length ||
        t1_rec->null_bit != t2_rec->null_bit)
    {
      return(1);
    }
  }
  return(0);
}


volatile int *killed_ptr(MI_CHECK *param)
{
  /* In theory Unsafe conversion, but should be ok for now */
  return (int*) (((Session *)(param->session))->getKilledPtr());
}

void mi_check_print_error(MI_CHECK *param, const char *fmt,...)
{
  param->error_printed|=1;
  param->out_flag|= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "error", fmt, args);
  va_end(args);
}

void mi_check_print_info(MI_CHECK *param, const char *fmt,...)
{
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "info", fmt, args);
  va_end(args);
}

void mi_check_print_warning(MI_CHECK *param, const char *fmt,...)
{
  param->warning_printed=1;
  param->out_flag|= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "warning", fmt, args);
  va_end(args);
}

/**
  Report list of threads (and queries) accessing a table, thread_id of a
  thread that detected corruption, ource file name and line number where
  this corruption was detected, optional extra information (string).

  This function is intended to be used when table corruption is detected.

  @param[in] file      MI_INFO object.
  @param[in] message   Optional error message.
  @param[in] sfile     Name of source file.
  @param[in] sline     Line number in source file.

  @return void
*/

void _mi_report_crashed(MI_INFO *file, const char *message,
                        const char *sfile, uint32_t sline)
{
  Session *cur_session;
  if ((cur_session= file->in_use))
  {
    errmsg_printf(error::ERROR, _("Got an error from thread_id=%"PRIu64", %s:%d"),
                    cur_session->thread_id,
                    sfile, sline);
  }
  else
  {
    errmsg_printf(error::ERROR, _("Got an error from unknown thread, %s:%d"), sfile, sline);
  }

  if (message)
    errmsg_printf(error::ERROR, "%s", message);

  list<Session *>::iterator it= file->s->in_use->begin();
  while (it != file->s->in_use->end())
  {
    errmsg_printf(error::ERROR, "%s", _("Unknown thread accessing table"));
    ++it;
  }
}

ha_myisam::ha_myisam(plugin::StorageEngine &engine_arg,
                     Table &table_arg)
  : Cursor(engine_arg, table_arg),
  file(0),
  can_enable_indexes(true),
  is_ordered(true)
{ }

Cursor *ha_myisam::clone(memory::Root *mem_root)
{
  ha_myisam *new_handler= static_cast <ha_myisam *>(Cursor::clone(mem_root));
  if (new_handler)
    new_handler->file->state= file->state;
  return new_handler;
}

const char *ha_myisam::index_type(uint32_t )
{
  return "BTREE";
}

/* Name is here without an extension */
int ha_myisam::doOpen(const drizzled::identifier::Table &identifier, int mode, uint32_t test_if_locked)
{
  MI_KEYDEF *keyinfo;
  MI_COLUMNDEF *recinfo= 0;
  uint32_t recs;
  uint32_t i;

  /*
    If the user wants to have memory mapped data files, add an
    open_flag. Do not memory map temporary tables because they are
    expected to be inserted and thus extended a lot. Memory mapping is
    efficient for files that keep their size, but very inefficient for
    growing files. Using an open_flag instead of calling mi_extra(...
    HA_EXTRA_MMAP ...) after mi_open() has the advantage that the
    mapping is not repeated for every open, but just done on the initial
    open, when the MyISAM share is created. Everytime the server
    requires to open a new instance of a table it calls this method. We
    will always supply HA_OPEN_MMAP for a permanent table. However, the
    MyISAM storage engine will ignore this flag if this is a secondary
    open of a table that is in use by other threads already (if the
    MyISAM share exists already).
  */
  if (!(file= mi_open(identifier, mode, test_if_locked)))
    return (errno ? errno : -1);

  if (!getTable()->getShare()->getType()) /* No need to perform a check for tmp table */
  {
    if ((errno= table2myisam(getTable(), &keyinfo, &recinfo, &recs)))
    {
      goto err;
    }
    if (check_definition(keyinfo, recinfo, getTable()->getShare()->sizeKeys(), recs,
                         file->s->keyinfo, file->s->rec,
                         file->s->base.keys, file->s->base.fields, true))
    {
      errno= HA_ERR_CRASHED;
      goto err;
    }
  }

  assert(test_if_locked);
  if (test_if_locked & (HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_TMP_TABLE))
    mi_extra(file, HA_EXTRA_NO_WAIT_LOCK, 0);

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    mi_extra(file, HA_EXTRA_WAIT_LOCK, 0);
  if (!getTable()->getShare()->db_record_offset)
    is_ordered= false;


  keys_with_parts.reset();
  for (i= 0; i < getTable()->getShare()->sizeKeys(); i++)
  {
    getTable()->key_info[i].block_size= file->s->keyinfo[i].block_length;

    KeyPartInfo *kp= getTable()->key_info[i].key_part;
    KeyPartInfo *kp_end= kp + getTable()->key_info[i].key_parts;
    for (; kp != kp_end; kp++)
    {
      if (!kp->field->part_of_key.test(i))
      {
        keys_with_parts.set(i);
        break;
      }
    }
  }
  errno= 0;
  goto end;
 err:
  this->close();
 end:
  /*
    Both recinfo and keydef are allocated by multi_malloc(), thus only
    recinfo must be freed.
  */
  if (recinfo)
    free((unsigned char*) recinfo);
  return errno;
}

int ha_myisam::close(void)
{
  MI_INFO *tmp=file;
  file=0;
  return mi_close(tmp);
}

int ha_myisam::doInsertRecord(unsigned char *buf)
{
  /*
    If we have an auto_increment column and we are writing a changed row
    or a new row, then update the auto_increment value in the record.
  */
  if (getTable()->next_number_field && buf == getTable()->getInsertRecord())
  {
    int error;
    if ((error= update_auto_increment()))
      return error;
  }
  return mi_write(file,buf);
}


int ha_myisam::repair(Session *session, MI_CHECK &param, bool do_optimize)
{
  int error=0;
  uint32_t local_testflag= param.testflag;
  bool optimize_done= !do_optimize, statistics_done=0;
  const char *old_proc_info= session->get_proc_info();
  char fixed_name[FN_REFLEN];
  MYISAM_SHARE* share = file->s;
  ha_rows rows= file->state->records;

  /*
    Normally this method is entered with a properly opened table. If the
    repair fails, it can be repeated with more elaborate options. Under
    special circumstances it can happen that a repair fails so that it
    closed the data file and cannot re-open it. In this case file->dfile
    is set to -1. We must not try another repair without an open data
    file. (Bug #25289)
  */
  if (file->dfile == -1)
  {
    errmsg_printf(error::INFO, "Retrying repair of: '%s' failed. "
		  "Please try REPAIR EXTENDED or myisamchk",
		  getTable()->getShare()->getPath());
    return(HA_ADMIN_FAILED);
  }

  param.db_name=    getTable()->getShare()->getSchemaName();
  param.table_name= getTable()->getAlias();
  param.tmpfile_createflag = O_RDWR | O_TRUNC;
  param.using_global_keycache = 1;
  param.session= session;
  param.out_flag= 0;
  param.sort_buffer_length= static_cast<size_t>(sort_buffer_size);
  strcpy(fixed_name,file->filename);

  // Don't lock tables if we have used LOCK Table
  if (mi_lock_database(file, getTable()->getShare()->getType() ? F_EXTRA_LCK : F_WRLCK))
  {
    mi_check_print_error(&param,ER(ER_CANT_LOCK),errno);
    return(HA_ADMIN_FAILED);
  }

  if (!do_optimize ||
      ((file->state->del || share->state.split != file->state->records) &&
       (!(param.testflag & T_QUICK) ||
	!(share->state.changed & STATE_NOT_OPTIMIZED_KEYS))))
  {
    uint64_t key_map= ((local_testflag & T_CREATE_MISSING_KEYS) ?
			mi_get_mask_all_keys_active(share->base.keys) :
			share->state.key_map);
    uint32_t testflag=param.testflag;
    if (mi_test_if_sort_rep(file,file->state->records,key_map,0) &&
	(local_testflag & T_REP_BY_SORT))
    {
      local_testflag|= T_STATISTICS;
      param.testflag|= T_STATISTICS;		// We get this for free
      statistics_done=1;
      {
        session->set_proc_info("Repair by sorting");
        error = mi_repair_by_sort(&param, file, fixed_name,
            param.testflag & T_QUICK);
      }
    }
    else
    {
      session->set_proc_info("Repair with keycache");
      param.testflag &= ~T_REP_BY_SORT;
      error=  mi_repair(&param, file, fixed_name,
			param.testflag & T_QUICK);
    }
    param.testflag=testflag;
    optimize_done=1;
  }
  if (!error)
  {
    if ((local_testflag & T_SORT_INDEX) &&
	(share->state.changed & STATE_NOT_SORTED_PAGES))
    {
      optimize_done=1;
      session->set_proc_info("Sorting index");
      error=mi_sort_index(&param,file,fixed_name);
    }
    if (!statistics_done && (local_testflag & T_STATISTICS))
    {
      if (share->state.changed & STATE_NOT_ANALYZED)
      {
	optimize_done=1;
	session->set_proc_info("Analyzing");
	error = chk_key(&param, file);
      }
      else
	local_testflag&= ~T_STATISTICS;		// Don't update statistics
    }
  }
  session->set_proc_info("Saving state");
  if (!error)
  {
    if ((share->state.changed & STATE_CHANGED) || mi_is_crashed(file))
    {
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
    /*
      the following 'if', thought conceptually wrong,
      is a useful optimization nevertheless.
    */
    if (file->state != &file->s->state.state)
      file->s->state.state = *file->state;
    if (file->s->base.auto_key)
      update_auto_increment_key(&param, file, 1);
    if (optimize_done)
      error = update_state_info(&param, file,
				UPDATE_TIME | UPDATE_OPEN_COUNT |
				(local_testflag &
				 T_STATISTICS ? UPDATE_STAT : 0));
    info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	 HA_STATUS_CONST);
    if (rows != file->state->records && ! (param.testflag & T_VERY_SILENT))
    {
      char llbuff[22],llbuff2[22];
      mi_check_print_warning(&param,"Number of rows changed from %s to %s",
			     internal::llstr(rows,llbuff),
			     internal::llstr(file->state->records,llbuff2));
    }
  }
  else
  {
    mi_mark_crashed_on_repair(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    update_state_info(&param, file, 0);
  }
  session->set_proc_info(old_proc_info);
  mi_lock_database(file,F_UNLCK);

  return(error ? HA_ADMIN_FAILED :
	      !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}


/*
  Disable indexes, making it persistent if requested.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::disable_indexes(uint32_t mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    /* call a storage engine function to switch the key map */
    error= mi_disable_indexes(file);
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    mi_extra(file, HA_EXTRA_NO_KEYS, 0);
    info(HA_STATUS_CONST);                        // Read new key info
    error= 0;
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Enable indexes, making it persistent if requested.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes, which might have been disabled by disable_index() before.
    The modes without _SAVE work only if both data and indexes are empty,
    since the MyISAM repair would enable them persistently.
    To be sure in these cases, call Cursor::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    !=0  Error, among others:
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::enable_indexes(uint32_t mode)
{
  int error;

  if (mi_is_all_keys_active(file->s->state.key_map, file->s->base.keys))
  {
    /* All indexes are enabled already. */
    return 0;
  }

  if (mode == HA_KEY_SWITCH_ALL)
  {
    error= mi_enable_indexes(file);
    /*
       Do not try to repair on error,
       as this could make the enabled state persistent,
       but mode==HA_KEY_SWITCH_ALL forbids it.
    */
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    Session *session= getTable()->in_use;
    boost::scoped_ptr<MI_CHECK> param_ap(new MI_CHECK);
    MI_CHECK &param= *param_ap.get();
    const char *save_proc_info= session->get_proc_info();
    session->set_proc_info("Creating index");
    myisamchk_init(&param);
    param.op_name= "recreating_index";
    param.testflag= (T_SILENT | T_REP_BY_SORT | T_QUICK |
                     T_CREATE_MISSING_KEYS);
    param.myf_rw&= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length=  static_cast<size_t>(sort_buffer_size);
    param.stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;
    if ((error= (repair(session,param,0) != HA_ADMIN_OK)) && param.retry_repair)
    {
      errmsg_printf(error::WARN, "Warning: Enabling keys got errno %d on %s.%s, retrying",
                        errno, param.db_name, param.table_name);
      /* Repairing by sort failed. Now try standard repair method. */
      param.testflag&= ~(T_REP_BY_SORT | T_QUICK);
      error= (repair(session,param,0) != HA_ADMIN_OK);
      /*
        If the standard repair succeeded, clear all error messages which
        might have been set by the first repair. They can still be seen
        with SHOW WARNINGS then.
      */
      if (not error)
        session->clear_error();
    }
    info(HA_STATUS_CONST);
    session->set_proc_info(save_proc_info);
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Test if indexes are disabled.


  SYNOPSIS
    indexes_are_disabled()
      no parameters


  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_myisam::indexes_are_disabled(void)
{

  return mi_indexes_are_disabled(file);
}


/*
  prepare for a many-rows insert operation
  e.g. - disable indexes (if they can be recreated fast) or
  activate special bulk-insert optimizations

  SYNOPSIS
    start_bulk_insert(rows)
    rows        Rows to be inserted
                0 if we don't know

  NOTICE
    Do not forget to call end_bulk_insert() later!
*/

void ha_myisam::start_bulk_insert(ha_rows rows)
{
  Session *session= getTable()->in_use;
  ulong size= session->variables.read_buff_size;

  /* don't enable row cache if too few rows */
  if (! rows || (rows > MI_MIN_ROWS_TO_USE_WRITE_CACHE))
    mi_extra(file, HA_EXTRA_WRITE_CACHE, (void*) &size);

  can_enable_indexes= mi_is_all_keys_active(file->s->state.key_map,
                                            file->s->base.keys);

  /*
    Only disable old index if the table was empty and we are inserting
    a lot of rows.
    We should not do this for only a few rows as this is slower and
    we don't want to update the key statistics based of only a few rows.
  */
  if (file->state->records == 0 && can_enable_indexes &&
      (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES))
    mi_disable_non_unique_index(file,rows);
  else
    if (!file->bulk_insert &&
        (!rows || rows >= MI_MIN_ROWS_TO_USE_BULK_INSERT))
    {
      mi_init_bulk_insert(file,
                          (size_t)session->variables.bulk_insert_buff_size,
                          rows);
    }
}

/*
  end special bulk-insert optimizations,
  which have been activated by start_bulk_insert().

  SYNOPSIS
    end_bulk_insert()
    no arguments

  RETURN
    0     OK
    != 0  Error
*/

int ha_myisam::end_bulk_insert()
{
  mi_end_bulk_insert(file);
  int err=mi_extra(file, HA_EXTRA_NO_CACHE, 0);
  return err ? err : can_enable_indexes ?
                     enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE) : 0;
}



int ha_myisam::doUpdateRecord(const unsigned char *old_data, unsigned char *new_data)
{
  return mi_update(file,old_data,new_data);
}

int ha_myisam::doDeleteRecord(const unsigned char *buf)
{
  return mi_delete(file,buf);
}


int ha_myisam::doStartIndexScan(uint32_t idx, bool )
{
  active_index=idx;
  //in_range_read= false;
  return 0;
}


int ha_myisam::doEndIndexScan()
{
  active_index=MAX_KEY;
  return 0;
}


int ha_myisam::index_read_map(unsigned char *buf, const unsigned char *key,
                              key_part_map keypart_map,
                              enum ha_rkey_function find_flag)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map, find_flag);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_idx_map(unsigned char *buf, uint32_t index, const unsigned char *key,
                                  key_part_map keypart_map,
                                  enum ha_rkey_function find_flag)
{
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error=mi_rkey(file, buf, index, key, keypart_map, find_flag);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_last_map(unsigned char *buf, const unsigned char *key,
                                   key_part_map keypart_map)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map,
                    HA_READ_PREFIX_LAST);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return(error);
}

int ha_myisam::index_next(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_next_count);
  int error=mi_rnext(file,buf,active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_prev(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_prev_count);
  int error=mi_rprev(file,buf, active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_first(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_first_count);
  int error=mi_rfirst(file, buf, active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_last(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_last_count);
  int error=mi_rlast(file, buf, active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next_same(unsigned char *buf,
			       const unsigned char *,
			       uint32_t )
{
  int error;
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_next_count);
  do
  {
    error= mi_rnext_same(file,buf);
  } while (error == HA_ERR_RECORD_DELETED);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::read_range_first(const key_range *start_key,
		 	        const key_range *end_key,
			        bool eq_range_arg,
                                bool sorted /* ignored */)
{
  int res;
  //if (!eq_range_arg)
  //  in_range_read= true;

  res= Cursor::read_range_first(start_key, end_key, eq_range_arg, sorted);

  //if (res)
  //  in_range_read= false;
  return res;
}


int ha_myisam::read_range_next()
{
  int res= Cursor::read_range_next();
  //if (res)
  //  in_range_read= false;
  return res;
}


int ha_myisam::doStartTableScan(bool scan)
{
  if (scan)
    return mi_scan_init(file);
  return mi_reset(file);                        // Free buffers
}

int ha_myisam::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  int error=mi_scan(file, buf);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::rnd_pos(unsigned char *buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  int error=mi_rrnd(file, buf, internal::my_get_ptr(pos,ref_length));
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


void ha_myisam::position(const unsigned char *)
{
  internal::my_off_t row_position= mi_position(file);
  internal::my_store_ptr(ref, ref_length, row_position);
}

int ha_myisam::info(uint32_t flag)
{
  MI_ISAMINFO misam_info;
  char name_buff[FN_REFLEN];

  (void) mi_status(file,&misam_info,flag);
  if (flag & HA_STATUS_VARIABLE)
  {
    stats.records=           misam_info.records;
    stats.deleted=           misam_info.deleted;
    stats.data_file_length=  misam_info.data_file_length;
    stats.index_file_length= misam_info.index_file_length;
    stats.delete_length=     misam_info.delete_length;
    stats.check_time=        misam_info.check_time;
    stats.mean_rec_length=   misam_info.mean_reclength;
  }
  if (flag & HA_STATUS_CONST)
  {
    TableShare *share= getTable()->getMutableShare();
    stats.max_data_file_length=  misam_info.max_data_file_length;
    stats.max_index_file_length= misam_info.max_index_file_length;
    stats.create_time= misam_info.create_time;
    ref_length= misam_info.reflength;
    share->db_options_in_use= misam_info.options;
    stats.block_size= myisam_key_cache_block_size;        /* record block size */

    set_prefix(share->keys_in_use, share->sizeKeys());
    /*
     * Due to bug 394932 (32-bit solaris build failure), we need
     * to convert the uint64_t key_map member of the misam_info
     * structure in to a std::bitset so that we can logically and
     * it with the share->key_in_use key_map.
     */
    ostringstream ostr;
    string binary_key_map;
    uint64_t num= misam_info.key_map;
    /*
     * Convert the uint64_t to a binary
     * string representation of it.
     */
    while (num > 0)
    {
      uint64_t bin_digit= num % 2;
      ostr << bin_digit;
      num/= 2;
    }
    binary_key_map.append(ostr.str());
    /*
     * Now we have the binary string representation of the
     * flags, we need to fill that string representation out
     * with the appropriate number of bits. This is needed
     * since key_map is declared as a std::bitset of a certain bit
     * width that depends on the MAX_INDEXES variable. 
     */
    if (MAX_INDEXES <= 64)
    {
      size_t len= 72 - binary_key_map.length();
      string all_zeros(len, '0');
      binary_key_map.insert(binary_key_map.begin(),
                            all_zeros.begin(),
                            all_zeros.end());
    }
    else
    {
      size_t len= (MAX_INDEXES + 7) / 8 * 8;
      string all_zeros(len, '0');
      binary_key_map.insert(binary_key_map.begin(),
                            all_zeros.begin(),
                            all_zeros.end());
    }
    key_map tmp_map(binary_key_map);
    share->keys_in_use&= tmp_map;
    share->keys_for_keyread&= share->keys_in_use;
    share->db_record_offset= misam_info.record_offset;
    if (share->key_parts)
      memcpy(getTable()->key_info[0].rec_per_key,
	     misam_info.rec_per_key,
	     sizeof(getTable()->key_info[0].rec_per_key)*share->key_parts);
    assert(share->getType() != message::Table::STANDARD);

   /*
     Set data_file_name and index_file_name to point at the symlink value
     if table is symlinked (Ie;  Real name is not same as generated name)
   */
    data_file_name= index_file_name= 0;
    internal::fn_format(name_buff, file->filename, "", MI_NAME_DEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.data_file_name))
      data_file_name=misam_info.data_file_name;
    internal::fn_format(name_buff, file->filename, "", MI_NAME_IEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.index_file_name))
      index_file_name=misam_info.index_file_name;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = misam_info.errkey;
    internal::my_store_ptr(dup_ref, ref_length, misam_info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME)
    stats.update_time = misam_info.update_time;
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= misam_info.auto_increment;

  return 0;
}


int ha_myisam::extra(enum ha_extra_function operation)
{
  return mi_extra(file, operation, 0);
}

int ha_myisam::reset(void)
{
  return mi_reset(file);
}

/* To be used with WRITE_CACHE and EXTRA_CACHE */

int ha_myisam::extra_opt(enum ha_extra_function operation, uint32_t cache_size)
{
  return mi_extra(file, operation, (void*) &cache_size);
}

int ha_myisam::delete_all_rows()
{
  return mi_delete_all_rows(file);
}

int MyisamEngine::doDropTable(Session &session,
                              const identifier::Table &identifier)
{
  session.getMessageCache().removeTableMessage(identifier);

  return mi_delete_table(identifier.getPath().c_str());
}


int ha_myisam::external_lock(Session *session, int lock_type)
{
  file->in_use= session;
  return mi_lock_database(file, !getTable()->getShare()->getType() ?
			  lock_type : ((lock_type == F_UNLCK) ?
				       F_UNLCK : F_EXTRA_LCK));
}

int MyisamEngine::doCreateTable(Session &session,
                                Table& table_arg,
                                const identifier::Table &identifier,
                                const message::Table& create_proto)
{
  int error;
  uint32_t create_flags= 0, create_records;
  char buff[FN_REFLEN];
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo;
  MI_CREATE_INFO create_info;
  TableShare *share= table_arg.getMutableShare();
  uint32_t options= share->db_options_in_use;
  if ((error= table2myisam(&table_arg, &keydef, &recinfo, &create_records)))
    return(error);
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows= create_proto.options().max_rows();
  create_info.reloc_rows= create_proto.options().min_rows();
  create_info.with_auto_increment= share->next_number_key_offset == 0;
  create_info.auto_increment= (create_proto.options().has_auto_increment_value() ?
                               create_proto.options().auto_increment_value() -1 :
                               (uint64_t) 0);
  create_info.data_file_length= (create_proto.options().max_rows() *
                                 create_proto.options().avg_row_length());
  create_info.data_file_name= NULL;
  create_info.index_file_name=  NULL;
  create_info.language= share->table_charset->number;

  if (create_proto.type() == message::Table::TEMPORARY)
    create_flags|= HA_CREATE_TMP_TABLE;
  if (options & HA_OPTION_PACK_RECORD)
    create_flags|= HA_PACK_RECORD;

  /* TODO: Check that the following internal::fn_format is really needed */
  error= mi_create(internal::fn_format(buff, identifier.getPath().c_str(), "", "",
                                       MY_UNPACK_FILENAME|MY_APPEND_EXT),
                   share->sizeKeys(), keydef,
                   create_records, recinfo,
                   0, (MI_UNIQUEDEF*) 0,
                   &create_info, create_flags);
  free((unsigned char*) recinfo);

  session.getMessageCache().storeTableMessage(identifier, create_proto);

  return error;
}


int MyisamEngine::doRenameTable(Session &session, const identifier::Table &from, const identifier::Table &to)
{
  session.getMessageCache().renameTableMessage(from, to);

  return mi_rename(from.getPath().c_str(), to.getPath().c_str());
}


void ha_myisam::get_auto_increment(uint64_t ,
                                   uint64_t ,
                                   uint64_t ,
                                   uint64_t *first_value,
                                   uint64_t *nb_reserved_values)
{
  uint64_t nr;
  int error;
  unsigned char key[MI_MAX_KEY_LENGTH];

  if (!getTable()->getShare()->next_number_key_offset)
  {						// Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    *first_value= stats.auto_increment_value;
    /* MyISAM has only table-level lock, so reserves to +inf */
    *nb_reserved_values= UINT64_MAX;
    return;
  }

  /* it's safe to call the following if bulk_insert isn't on */
  mi_flush_bulk_insert(file, getTable()->getShare()->next_number_index);

  (void) extra(HA_EXTRA_KEYREAD);
  key_copy(key, getTable()->getInsertRecord(),
           &getTable()->key_info[getTable()->getShare()->next_number_index],
           getTable()->getShare()->next_number_key_offset);
  error= mi_rkey(file, getTable()->getUpdateRecord(), (int) getTable()->getShare()->next_number_index,
                 key, make_prev_keypart_map(getTable()->getShare()->next_number_keypart),
                 HA_READ_PREFIX_LAST);
  if (error)
    nr= 1;
  else
  {
    /* Get data from getUpdateRecord() */
    nr= ((uint64_t) getTable()->next_number_field->
         val_int_offset(getTable()->getShare()->rec_buff_length)+1);
  }
  extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
  /*
    MySQL needs to call us for next row: assume we are inserting ("a",null)
    here, we return 3, and next this statement will want to insert ("b",null):
    there is no reason why ("b",3+1) would be the good row to insert: maybe it
    already exists, maybe 3+1 is too large...
  */
  *nb_reserved_values= 1;
}


/*
  Find out how many rows there is in the given range

  SYNOPSIS
    records_in_range()
    inx			Index to use
    min_key		Start of range.  Null pointer if from first key
    max_key		End of range. Null pointer if to last key

  NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT		Include the key in the range
      HA_READ_AFTER_KEY		Don't include key in range

    max_key.flag can have one of the following values:
      HA_READ_BEFORE_KEY	Don't include key in range
      HA_READ_AFTER_KEY		Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR		Something is wrong with the index tree.
   0			There is no matching keys in the given range
   number > 0		There is approximately 'number' matching rows in
			the range.
*/

ha_rows ha_myisam::records_in_range(uint32_t inx, key_range *min_key,
                                    key_range *max_key)
{
  return (ha_rows) mi_records_in_range(file, (int) inx, min_key, max_key);
}


uint32_t ha_myisam::checksum() const
{
  return (uint)file->state->checksum;
}

static int myisam_init(module::Context &context)
{ 
  context.add(new MyisamEngine(engine_name));
  context.registerVariable(new sys_var_constrained_value<size_t>("sort-buffer-size",
                                                                 sort_buffer_size));
  context.registerVariable(new sys_var_uint64_t_ptr("max_sort_file_size",
                                                    &max_sort_file_size,
                                                    context.getOptions()["max-sort-file-size"].as<uint64_t>()));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("max-sort-file-size",
          po::value<uint64_t>(&max_sort_file_size)->default_value(INT32_MAX),
          _("Don't use the fast sort index method to created index if the temporary file would get bigger than this."));
  context("sort-buffer-size",
          po::value<sort_buffer_constraint>(&sort_buffer_size)->default_value(8192*1024),
          _("The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE."));
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "MyISAM",
  "2.0",
  "MySQL AB",
  "Default engine as of MySQL 3.23 with great performance",
  PLUGIN_LICENSE_GPL,
  myisam_init, /* Plugin Init */
  NULL,           /* depends */
  init_options                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;
