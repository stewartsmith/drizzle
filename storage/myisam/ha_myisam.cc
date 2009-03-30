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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */



#include <drizzled/server_includes.h>
#include <mysys/my_bit.h>
#include "myisampack.h"
#include "ha_myisam.h"
#include "myisamdef.h"
#include <drizzled/util/test.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/protocol.h>
#include <drizzled/table.h>
#include <drizzled/field/timestamp.h>

#include <string>

using namespace std;

static const string engine_name("MyISAM");

ulong myisam_recover_options= HA_RECOVER_NONE;
pthread_mutex_t THR_LOCK_myisam= PTHREAD_MUTEX_INITIALIZER;

static uint32_t repair_threads;
static uint32_t block_size;
static uint64_t max_sort_file_size;
static uint64_t sort_buffer_size;

/* bits in myisam_recover_options */
const char *myisam_recover_names[] =
{ "DEFAULT", "BACKUP", "FORCE", "QUICK", NULL};
TYPELIB myisam_recover_typelib= {array_elements(myisam_recover_names)-1,"",
                                 myisam_recover_names, NULL};

const char *myisam_stats_method_names[] = {"nulls_unequal", "nulls_equal",
                                           "nulls_ignored", NULL};
TYPELIB myisam_stats_method_typelib= {
  array_elements(myisam_stats_method_names) - 1, "",
  myisam_stats_method_names, NULL};


/*****************************************************************************
** MyISAM tables
*****************************************************************************/

class MyisamEngine : public StorageEngine
{
public:
  MyisamEngine(string name_arg) : StorageEngine(name_arg) {}

  virtual handler *create(TABLE_SHARE *table,
                          MEM_ROOT *mem_root)
  {
    return new (mem_root) ha_myisam(this, table);
  }
};

// collect errors printed by mi_check routines

static void mi_check_print_msg(MI_CHECK *param,	const char* msg_type,
                               const char *fmt, va_list args)
{
  Session* session = (Session*)param->session;
  Protocol *protocol= session->protocol;
  uint32_t length, msg_length;
  char msgbuf[MI_MAX_MSG_BUF];
  char name[NAME_LEN*2+2];

  msg_length= vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  if (!session->drizzleclient_vio_ok())
  {
    errmsg_printf(ERRMSG_LVL_ERROR, "%s",msgbuf);
    return;
  }

  if (param->testflag & (T_CREATE_MISSING_KEYS | T_SAFE_REPAIR |
			 T_AUTO_REPAIR))
  {
    my_message(ER_NOT_KEYFILE,msgbuf,MYF(MY_WME));
    return;
  }
  length= sprintf(name,"%s.%s",param->db_name,param->table_name);

  /*
    TODO: switch from protocol to push_warning here. The main reason we didn't
    it yet is parallel repair. Due to following trace:
    mi_check_print_msg/push_warning/sql_alloc/my_pthread_getspecific_ptr.

    Also we likely need to lock mutex here (in both cases with protocol and
    push_warning).
  */
  protocol->prepare_for_resend();
  protocol->store(name, length, system_charset_info);
  protocol->store(param->op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->write())
    errmsg_printf(ERRMSG_LVL_ERROR, "Failed on drizzleclient_net_write, writing to stderr instead: %s\n",
		    msgbuf);
  return;
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
    and *keydef_out are allocated with a my_multi_malloc, *keydef_out
    is freed automatically when *recinfo_out is freed.

  RETURN VALUE
    0  OK
    !0 error code
*/

int table2myisam(Table *table_arg, MI_KEYDEF **keydef_out,
                 MI_COLUMNDEF **recinfo_out, uint32_t *records_out)
{
  uint32_t i, j, recpos, minpos, fieldpos, temp_length, length;
  enum ha_base_keytype type= HA_KEYTYPE_BINARY;
  unsigned char *record;
  KEY *pos;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo, *recinfo_pos;
  HA_KEYSEG *keyseg;
  TABLE_SHARE *share= table_arg->s;
  uint32_t options= share->db_options_in_use;
  if (!(my_multi_malloc(MYF(MY_WME),
          recinfo_out, (share->fields * 2 + 2) * sizeof(MI_COLUMNDEF),
          keydef_out, share->keys * sizeof(MI_KEYDEF),
          &keyseg,
          (share->key_parts + share->keys) * sizeof(HA_KEYSEG),
          NULL)))
    return(HA_ERR_OUT_OF_MEM); /* purecov: inspected */
  keydef= *keydef_out;
  recinfo= *recinfo_out;
  pos= table_arg->key_info;
  for (i= 0; i < share->keys; i++, pos++)
  {
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
             type == HA_KEYTYPE_NUM ||
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
                                           (unsigned char*) table_arg->record[0]);
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
                                            share->blob_ptr_size);
      }
    }
    keyseg+= pos->key_parts;
  }
  if (table_arg->found_next_number_field)
    keydef[share->next_number_index].flag|= HA_AUTO_KEY;
  record= table_arg->record[0];
  recpos= 0;
  recinfo_pos= recinfo;
  while (recpos < (uint) share->stored_rec_length)
  {
    Field **field, *found= 0;
    minpos= share->reclength;
    length= 0;

    for (field= table_arg->field; *field; field++)
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
                                     (unsigned char*) table_arg->record[0]);
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

int check_definition(MI_KEYDEF *t1_keyinfo, MI_COLUMNDEF *t1_recinfo,
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
          t1_keysegs_j__type= HA_KEYTYPE_VARTEXT1; /* purecov: tested */
        else if ((t1_keysegs_j__type == HA_KEYTYPE_VARBINARY2) &&
                 (t2_keysegs[j].type == HA_KEYTYPE_VARBINARY1))
          t1_keysegs_j__type= HA_KEYTYPE_VARBINARY1; /* purecov: inspected */
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


extern "C" {

volatile int *killed_ptr(MI_CHECK *param)
{
  /* In theory Unsafe conversion, but should be ok for now */
  return (int*) &(((Session *)(param->session))->killed);
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
  pthread_mutex_lock(&file->s->intern_lock);
  if ((cur_session= file->in_use))
    errmsg_printf(ERRMSG_LVL_ERROR, _("Got an error from thread_id=%"PRIu64", %s:%d"),
                    cur_session->thread_id,
                    sfile, sline);
  else
    errmsg_printf(ERRMSG_LVL_ERROR, _("Got an error from unknown thread, %s:%d"), sfile, sline);
  if (message)
    errmsg_printf(ERRMSG_LVL_ERROR, "%s", message);
  list<Session *>::iterator it= file->s->in_use->begin();
  while (it != file->s->in_use->end())
  {
    errmsg_printf(ERRMSG_LVL_ERROR, "%s", _("Unknown thread accessing table"));
    ++it;
  }
  pthread_mutex_unlock(&file->s->intern_lock);
}

}

ha_myisam::ha_myisam(StorageEngine *engine_arg, TABLE_SHARE *table_arg)
  :handler(engine_arg, table_arg), file(0),
  int_table_flags(HA_NULL_IN_KEY |
                  HA_BINLOG_ROW_CAPABLE |
                  HA_BINLOG_STMT_CAPABLE |
                  HA_DUPLICATE_POS |
                  HA_CAN_INDEX_BLOBS |
                  HA_AUTO_PART_KEY |
                  HA_FILE_BASED |
                  HA_NO_TRANSACTIONS |
                  HA_HAS_RECORDS |
                  HA_STATS_RECORDS_IS_EXACT |
                  HA_NEED_READ_RANGE_BUFFER |
                  HA_MRR_CANT_SORT),
   can_enable_indexes(1)
{}

handler *ha_myisam::clone(MEM_ROOT *mem_root)
{
  ha_myisam *new_handler= static_cast <ha_myisam *>(handler::clone(mem_root));
  if (new_handler)
    new_handler->file->state= file->state;
  return new_handler;
}


static const char *ha_myisam_exts[] = {
  ".MYI",
  ".MYD",
  NULL
};

const char **ha_myisam::bas_ext() const
{
  return ha_myisam_exts;
}


const char *ha_myisam::index_type(uint32_t )
{
  return "BTREE";
}

/* Name is here without an extension */
int ha_myisam::open(const char *name, int mode, uint32_t test_if_locked)
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
  if (!(file=mi_open(name, mode, test_if_locked | HA_OPEN_FROM_SQL_LAYER)))
    return (my_errno ? my_errno : -1);
  if (!table->s->tmp_table) /* No need to perform a check for tmp table */
  {
    if ((my_errno= table2myisam(table, &keyinfo, &recinfo, &recs)))
    {
      /* purecov: begin inspected */
      goto err;
      /* purecov: end */
    }
    if (check_definition(keyinfo, recinfo, table->s->keys, recs,
                         file->s->keyinfo, file->s->rec,
                         file->s->base.keys, file->s->base.fields, true))
    {
      /* purecov: begin inspected */
      my_errno= HA_ERR_CRASHED;
      goto err;
      /* purecov: end */
    }
  }

  if (test_if_locked & (HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_TMP_TABLE))
    mi_extra(file, HA_EXTRA_NO_WAIT_LOCK, 0);

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    mi_extra(file, HA_EXTRA_WAIT_LOCK, 0);
  if (!table->s->db_record_offset)
    int_table_flags|=HA_REC_NOT_IN_SEQ;
  if (file->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    int_table_flags|=HA_HAS_CHECKSUM;

  keys_with_parts.clear_all();
  for (i= 0; i < table->s->keys; i++)
  {
    table->key_info[i].block_size= file->s->keyinfo[i].block_length;

    KEY_PART_INFO *kp= table->key_info[i].key_part;
    KEY_PART_INFO *kp_end= kp + table->key_info[i].key_parts;
    for (; kp != kp_end; kp++)
    {
      if (!kp->field->part_of_key.is_set(i))
      {
        keys_with_parts.set_bit(i);
        break;
      }
    }
  }
  my_errno= 0;
  goto end;
 err:
  this->close();
 end:
  /*
    Both recinfo and keydef are allocated by my_multi_malloc(), thus only
    recinfo must be freed.
  */
  if (recinfo)
    free((unsigned char*) recinfo);
  return my_errno;
}

int ha_myisam::close(void)
{
  MI_INFO *tmp=file;
  file=0;
  return mi_close(tmp);
}

int ha_myisam::write_row(unsigned char *buf)
{
  ha_statistic_increment(&SSV::ha_write_count);

  /*
    If we have an auto_increment column and we are writing a changed row
    or a new row, then update the auto_increment value in the record.
  */
  if (table->next_number_field && buf == table->record[0])
  {
    int error;
    if ((error= update_auto_increment()))
      return error;
  }
  return mi_write(file,buf);
}

int ha_myisam::check(Session* session, HA_CHECK_OPT* check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  int error;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;
  const char *old_proc_info= session->get_proc_info();

  session->set_proc_info("Checking table");
  myisamchk_init(&param);
  param.session = session;
  param.op_name =   "check";
  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.testflag = check_opt->flags | T_CHECK | T_SILENT;
  param.stats_method= (enum_mi_stats_method)session->variables.myisam_stats_method;

  if (!(table->db_stat & HA_READ_ONLY))
    param.testflag|= T_STATISTICS;
  param.using_global_keycache = 1;

  if (!mi_is_crashed(file) &&
      (((param.testflag & T_CHECK_ONLY_CHANGED) &&
	!(share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				  STATE_CRASHED_ON_REPAIR)) &&
	share->state.open_count == 0) ||
       ((param.testflag & T_FAST) && (share->state.open_count ==
				      (uint) (share->global_changed ? 1 : 0)))))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_status(&param, file);		// Not fatal
  error = chk_size(&param, file);
  if (!error)
    error |= chk_del(&param, file, param.testflag);
  if (!error)
    error = chk_key(&param, file);
  if (!error)
  {
    if ((!(param.testflag & T_QUICK) &&
	 ((share->options &
	   (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
	  (param.testflag & (T_EXTEND | T_MEDIUM)))) ||
	mi_is_crashed(file))
    {
      uint32_t old_testflag=param.testflag;
      param.testflag|=T_MEDIUM;
      if (!(error= init_io_cache(&param.read_cache, file->dfile,
                                 my_default_record_cache_size, READ_CACHE,
                                 share->pack.header_length, 1, MYF(MY_WME))))
      {
        error= chk_data_link(&param, file, param.testflag & T_EXTEND);
        end_io_cache(&(param.read_cache));
      }
      param.testflag= old_testflag;
    }
  }
  if (!error)
  {
    if ((share->state.changed & (STATE_CHANGED |
				 STATE_CRASHED_ON_REPAIR |
				 STATE_CRASHED | STATE_NOT_ANALYZED)) ||
	(param.testflag & T_STATISTICS) ||
	mi_is_crashed(file))
    {
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      pthread_mutex_lock(&share->intern_lock);
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      if (!(table->db_stat & HA_READ_ONLY))
	error=update_state_info(&param,file,UPDATE_TIME | UPDATE_OPEN_COUNT |
				UPDATE_STAT);
      pthread_mutex_unlock(&share->intern_lock);
      info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	   HA_STATUS_CONST);
    }
  }
  else if (!mi_is_crashed(file) && !session->killed)
  {
    mi_mark_crashed(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
  }

  session->set_proc_info(old_proc_info);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}


/*
  analyze the key distribution in the table
  As the table may be only locked for read, we have to take into account that
  two threads may do an analyze at the same time!
*/

int ha_myisam::analyze(Session *session,
                       HA_CHECK_OPT* )
{
  int error=0;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;

  myisamchk_init(&param);
  param.session = session;
  param.op_name=    "analyze";
  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.testflag= (T_FAST | T_CHECK | T_SILENT | T_STATISTICS |
                   T_DONT_CHECK_CHECKSUM);
  param.using_global_keycache = 1;
  param.stats_method= (enum_mi_stats_method)session->variables.myisam_stats_method;

  if (!(share->state.changed & STATE_NOT_ANALYZED))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_key(&param, file);
  if (!error)
  {
    pthread_mutex_lock(&share->intern_lock);
    error=update_state_info(&param,file,UPDATE_STAT);
    pthread_mutex_unlock(&share->intern_lock);
  }
  else if (!mi_is_crashed(file) && !session->killed)
    mi_mark_crashed(file);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}


int ha_myisam::repair(Session* session, HA_CHECK_OPT *check_opt)
{
  int error;
  MI_CHECK param;
  ha_rows start_records;

  if (!file) return HA_ADMIN_INTERNAL_ERROR;

  myisamchk_init(&param);
  param.session = session;
  param.op_name=  "repair";
  param.testflag= ((check_opt->flags & ~(T_EXTEND)) |
                   T_SILENT | T_FORCE_CREATE | T_CALC_CHECKSUM |
                   (check_opt->flags & T_EXTEND ? T_REP : T_REP_BY_SORT));
  param.sort_buffer_length=  (size_t)sort_buffer_size;

  // Release latches since this can take a long time
  ha_release_temporary_latches(session);

  start_records=file->state->records;
  while ((error=repair(session,param,0)) && param.retry_repair)
  {
    param.retry_repair=0;
    if (test_all_bits(param.testflag,
		      (uint) (T_RETRY_WITHOUT_QUICK | T_QUICK)))
    {
      param.testflag&= ~T_RETRY_WITHOUT_QUICK;
      errmsg_printf(ERRMSG_LVL_INFO, "Retrying repair of: '%s' without quick",
                            table->s->path.str);
      continue;
    }
    param.testflag&= ~T_QUICK;
    if ((param.testflag & T_REP_BY_SORT))
    {
      param.testflag= (param.testflag & ~T_REP_BY_SORT) | T_REP;
      errmsg_printf(ERRMSG_LVL_INFO, "Retrying repair of: '%s' with keycache",
                            table->s->path.str);
      continue;
    }
    break;
  }
  if (!error && start_records != file->state->records &&
      !(check_opt->flags & T_VERY_SILENT))
  {
    char llbuff[22],llbuff2[22];
    errmsg_printf(ERRMSG_LVL_INFO, "Found %s of %s rows when repairing '%s'",
                          llstr(file->state->records, llbuff),
                          llstr(start_records, llbuff2),
                          table->s->path.str);
  }
  return error;
}

int ha_myisam::optimize(Session* session, HA_CHECK_OPT *check_opt)
{
  int error;
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.session = session;
  param.op_name= "optimize";
  param.testflag= (check_opt->flags | T_SILENT | T_FORCE_CREATE |
                   T_REP_BY_SORT | T_STATISTICS | T_SORT_INDEX);
  param.sort_buffer_length= (size_t)sort_buffer_size;
  if ((error= repair(session,param,1)) && param.retry_repair)
  {
    errmsg_printf(ERRMSG_LVL_WARN, "Warning: Optimize table got errno %d on %s.%s, retrying",
                      my_errno, param.db_name, param.table_name);
    param.testflag&= ~T_REP_BY_SORT;
    error= repair(session,param,1);
  }
  return error;
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
    errmsg_printf(ERRMSG_LVL_INFO, "Retrying repair of: '%s' failed. "
                          "Please try REPAIR EXTENDED or myisamchk",
                          table->s->path.str);
    return(HA_ADMIN_FAILED);
  }

  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.tmpfile_createflag = O_RDWR | O_TRUNC;
  param.using_global_keycache = 1;
  param.session= session;
  param.out_flag= 0;
  param.sort_buffer_length= (size_t)sort_buffer_size;
  strcpy(fixed_name,file->filename);

  // Don't lock tables if we have used LOCK Table
  if (!session->locked_tables &&
      mi_lock_database(file, table->s->tmp_table ? F_EXTRA_LCK : F_WRLCK))
  {
    mi_check_print_error(&param,ER(ER_CANT_LOCK),my_errno);
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
      if (repair_threads > 1)
      {
        char buf[40];
        /* TODO: respect myisam_repair_threads variable */
        snprintf(buf, 40, "Repair with %d threads", my_count_bits(key_map));
        session->set_proc_info(buf);
        error = mi_repair_parallel(&param, file, fixed_name,
            param.testflag & T_QUICK);
        session->set_proc_info("Repair done"); // to reset proc_info, as
                                      // it was pointing to local buffer
      }
      else
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
			     llstr(rows,llbuff),
			     llstr(file->state->records,llbuff2));
    }
  }
  else
  {
    mi_mark_crashed_on_repair(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    update_state_info(&param, file, 0);
  }
  session->set_proc_info(old_proc_info);
  if (!session->locked_tables)
    mi_lock_database(file,F_UNLCK);
  return(error ? HA_ADMIN_FAILED :
	      !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}


/*
  Assign table indexes to a specific key cache.
*/

int ha_myisam::assign_to_keycache(Session* session, HA_CHECK_OPT *check_opt)
{
  KEY_CACHE *new_key_cache= check_opt->key_cache;
  const char *errmsg= 0;
  int error= HA_ADMIN_OK;
  uint64_t map;
  TableList *table_list= table->pos_in_table_list;

  table->keys_in_use_for_query.clear_all();

  if (table_list->process_index_hints(table))
    return(HA_ADMIN_FAILED);
  map= ~(uint64_t) 0;
  if (!table->keys_in_use_for_query.is_clear_all())
    /* use all keys if there's no list specified by the user through hints */
    map= table->keys_in_use_for_query.to_uint64_t();

  if ((error= mi_assign_to_key_cache(file, map, new_key_cache)))
  {
    char buf[STRING_BUFFER_USUAL_SIZE];
    snprintf(buf, sizeof(buf),
		"Failed to flush to index file (errno: %d)", error);
    errmsg= buf;
    error= HA_ADMIN_CORRUPT;
  }

  if (error != HA_ADMIN_OK)
  {
    /* Send error to user */
    MI_CHECK param;
    myisamchk_init(&param);
    param.session= session;
    param.op_name=    "assign_to_keycache";
    param.db_name=    table->s->db.str;
    param.table_name= table->s->table_name.str;
    param.testflag= 0;
    mi_check_print_error(&param, errmsg);
  }
  return(error);
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
    To be sure in these cases, call handler::delete_all_rows() before.

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
    Session *session=current_session;
    MI_CHECK param;
    const char *save_proc_info= session->get_proc_info();
    session->set_proc_info("Creating index");
    myisamchk_init(&param);
    param.op_name= "recreating_index";
    param.testflag= (T_SILENT | T_REP_BY_SORT | T_QUICK |
                     T_CREATE_MISSING_KEYS);
    param.myf_rw&= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length=  (size_t)sort_buffer_size;
    param.stats_method= (enum_mi_stats_method)session->variables.myisam_stats_method;
    if ((error= (repair(session,param,0) != HA_ADMIN_OK)) && param.retry_repair)
    {
      errmsg_printf(ERRMSG_LVL_WARN, "Warning: Enabling keys got errno %d on %s.%s, retrying",
                        my_errno, param.db_name, param.table_name);
      /* Repairing by sort failed. Now try standard repair method. */
      param.testflag&= ~(T_REP_BY_SORT | T_QUICK);
      error= (repair(session,param,0) != HA_ADMIN_OK);
      /*
        If the standard repair succeeded, clear all error messages which
        might have been set by the first repair. They can still be seen
        with SHOW WARNINGS then.
      */
      if (! error)
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
  Session *session= current_session;
  ulong size= cmin(session->variables.read_buff_size,
                  (ulong) (table->s->avg_row_length*rows));

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

  return;
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


bool ha_myisam::check_and_repair(Session *session)
{
  int error=0;
  int marked_crashed;
  char *old_query;
  uint32_t old_query_length;
  HA_CHECK_OPT check_opt;

  check_opt.init();
  check_opt.flags= T_MEDIUM | T_AUTO_REPAIR;
  // Don't use quick if deleted rows
  if (!file->state->del && (myisam_recover_options & HA_RECOVER_QUICK))
    check_opt.flags|=T_QUICK;
  errmsg_printf(ERRMSG_LVL_WARN, "Checking table:   '%s'",table->s->path.str);

  old_query= session->query;
  old_query_length= session->query_length;
  pthread_mutex_lock(&LOCK_thread_count);
  session->query=        table->s->table_name.str;
  session->query_length= table->s->table_name.length;
  pthread_mutex_unlock(&LOCK_thread_count);

  if ((marked_crashed= mi_is_crashed(file)) || check(session, &check_opt))
  {
    errmsg_printf(ERRMSG_LVL_WARN, "Recovering table: '%s'",table->s->path.str);
    check_opt.flags=
      ((myisam_recover_options & HA_RECOVER_BACKUP ? T_BACKUP_DATA : 0) |
       (marked_crashed                             ? 0 : T_QUICK) |
       (myisam_recover_options & HA_RECOVER_FORCE  ? 0 : T_SAFE_REPAIR) |
       T_AUTO_REPAIR);
    if (repair(session, &check_opt))
      error=1;
  }
  pthread_mutex_lock(&LOCK_thread_count);
  session->query= old_query;
  session->query_length= old_query_length;
  pthread_mutex_unlock(&LOCK_thread_count);
  return(error);
}

bool ha_myisam::is_crashed() const
{
  return (file->s->state.changed & STATE_CRASHED ||
	  (file->s->state.open_count));
}

int ha_myisam::update_row(const unsigned char *old_data, unsigned char *new_data)
{
  ha_statistic_increment(&SSV::ha_update_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return mi_update(file,old_data,new_data);
}

int ha_myisam::delete_row(const unsigned char *buf)
{
  ha_statistic_increment(&SSV::ha_delete_count);
  return mi_delete(file,buf);
}

#ifdef __cplusplus
extern "C" {
#endif

bool index_cond_func_myisam(void *arg)
{
  ha_myisam *h= (ha_myisam*)arg;
  /*if (h->in_range_read)*/
  if (h->end_range)
  {
    if (h->compare_key2(h->end_range) > 0)
      return 2; /* caller should return HA_ERR_END_OF_FILE already */
  }
  return (bool)h->pushed_idx_cond->val_int();
}

#ifdef __cplusplus
}
#endif


int ha_myisam::index_init(uint32_t idx, bool )
{
  active_index=idx;
  //in_range_read= false;
  if (pushed_idx_cond_keyno == idx)
    mi_set_index_cond_func(file, index_cond_func_myisam, this);
  return 0;
}


int ha_myisam::index_end()
{
  active_index=MAX_KEY;
  //pushed_idx_cond_keyno= MAX_KEY;
  mi_set_index_cond_func(file, NULL, 0);
  in_range_check_pushed_down= false;
  ds_mrr.dsmrr_close();
  return 0;
}


uint32_t ha_myisam::index_flags(uint32_t inx, uint32_t, bool) const
{
  return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
          0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
          HA_READ_ORDER | HA_KEYREAD_ONLY |
          (keys_with_parts.is_set(inx)?0:HA_DO_INDEX_COND_PUSHDOWN));
}


int ha_myisam::index_read_map(unsigned char *buf, const unsigned char *key,
                              key_part_map keypart_map,
                              enum ha_rkey_function find_flag)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_idx_map(unsigned char *buf, uint32_t index, const unsigned char *key,
                                  key_part_map keypart_map,
                                  enum ha_rkey_function find_flag)
{
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_last_map(unsigned char *buf, const unsigned char *key,
                                   key_part_map keypart_map)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map,
                    HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return(error);
}

int ha_myisam::index_next(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_next_count);
  int error=mi_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_prev(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_prev_count);
  int error=mi_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_first(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_first_count);
  int error=mi_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_last(unsigned char *buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_last_count);
  int error=mi_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next_same(unsigned char *buf,
			       const unsigned char *,
			       uint32_t )
{
  int error;
  assert(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_next_count);
  do
  {
    error= mi_rnext_same(file,buf);
  } while (error == HA_ERR_RECORD_DELETED);
  table->status=error ? STATUS_NOT_FOUND: 0;
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

  res= handler::read_range_first(start_key, end_key, eq_range_arg, sorted);

  //if (res)
  //  in_range_read= false;
  return res;
}


int ha_myisam::read_range_next()
{
  int res= handler::read_range_next();
  //if (res)
  //  in_range_read= false;
  return res;
}


int ha_myisam::rnd_init(bool scan)
{
  if (scan)
    return mi_scan_init(file);
  return mi_reset(file);                        // Free buffers
}

int ha_myisam::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  int error=mi_scan(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::restart_rnd_next(unsigned char *buf, unsigned char *pos)
{
  return rnd_pos(buf,pos);
}

int ha_myisam::rnd_pos(unsigned char *buf, unsigned char *pos)
{
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  int error=mi_rrnd(file, buf, my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


void ha_myisam::position(const unsigned char *)
{
  my_off_t row_position= mi_position(file);
  my_store_ptr(ref, ref_length, row_position);
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
    TABLE_SHARE *share= table->s;
    stats.max_data_file_length=  misam_info.max_data_file_length;
    stats.max_index_file_length= misam_info.max_index_file_length;
    stats.create_time= misam_info.create_time;
    ref_length= misam_info.reflength;
    share->db_options_in_use= misam_info.options;
    stats.block_size= block_size;        /* record block size */

    /* Update share */
    if (share->tmp_table == NO_TMP_TABLE)
      pthread_mutex_lock(&share->mutex);
    share->keys_in_use.set_prefix(share->keys);
    share->keys_in_use.intersect_extended(misam_info.key_map);
    share->keys_for_keyread.intersect(share->keys_in_use);
    share->db_record_offset= misam_info.record_offset;
    if (share->key_parts)
      memcpy(table->key_info[0].rec_per_key,
	     misam_info.rec_per_key,
	     sizeof(table->key_info[0].rec_per_key)*share->key_parts);
    if (share->tmp_table == NO_TMP_TABLE)
      pthread_mutex_unlock(&share->mutex);

   /*
     Set data_file_name and index_file_name to point at the symlink value
     if table is symlinked (Ie;  Real name is not same as generated name)
   */
    data_file_name= index_file_name= 0;
    fn_format(name_buff, file->filename, "", MI_NAME_DEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.data_file_name))
      data_file_name=misam_info.data_file_name;
    fn_format(name_buff, file->filename, "", MI_NAME_IEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.index_file_name))
      index_file_name=misam_info.index_file_name;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = misam_info.errkey;
    my_store_ptr(dup_ref, ref_length, misam_info.dupp_key_pos);
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
  pushed_idx_cond= NULL;
  pushed_idx_cond_keyno= MAX_KEY;
  mi_set_index_cond_func(file, NULL, 0);
  ds_mrr.dsmrr_close();
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

int ha_myisam::delete_table(const char *name)
{
  return mi_delete_table(name);
}


int ha_myisam::external_lock(Session *session, int lock_type)
{
  file->in_use= session;
  return mi_lock_database(file, !table->s->tmp_table ?
			  lock_type : ((lock_type == F_UNLCK) ?
				       F_UNLCK : F_EXTRA_LCK));
}

THR_LOCK_DATA **ha_myisam::store_lock(Session *,
				      THR_LOCK_DATA **to,
				      enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}

void ha_myisam::update_create_info(HA_CREATE_INFO *create_info)
{
  ha_myisam::info(HA_STATUS_AUTO | HA_STATUS_CONST);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    create_info->auto_increment_value= stats.auto_increment_value;
  }
  create_info->data_file_name=data_file_name;
  create_info->index_file_name=index_file_name;
}


int ha_myisam::create(const char *name, register Table *table_arg,
		      HA_CREATE_INFO *ha_create_info)
{
  int error;
  uint32_t create_flags= 0, create_records;
  char buff[FN_REFLEN];
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo;
  MI_CREATE_INFO create_info;
  TABLE_SHARE *share= table_arg->s;
  uint32_t options= share->db_options_in_use;
  if ((error= table2myisam(table_arg, &keydef, &recinfo, &create_records)))
    return(error); /* purecov: inspected */
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows= share->max_rows;
  create_info.reloc_rows= share->min_rows;
  create_info.with_auto_increment= share->next_number_key_offset == 0;
  create_info.auto_increment= (ha_create_info->auto_increment_value ?
                               ha_create_info->auto_increment_value -1 :
                               (uint64_t) 0);
  create_info.data_file_length= ((uint64_t) share->max_rows *
                                 share->avg_row_length);
  create_info.data_file_name= ha_create_info->data_file_name;
  create_info.index_file_name= ha_create_info->index_file_name;
  create_info.language= share->table_charset->number;

  if (ha_create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= HA_CREATE_TMP_TABLE;
  if (ha_create_info->options & HA_CREATE_KEEP_FILES)
    create_flags|= HA_CREATE_KEEP_FILES;
  if (options & HA_OPTION_PACK_RECORD)
    create_flags|= HA_PACK_RECORD;
  if (options & HA_OPTION_CHECKSUM)
    create_flags|= HA_CREATE_CHECKSUM;
  if (options & HA_OPTION_DELAY_KEY_WRITE)
    create_flags|= HA_CREATE_DELAY_KEY_WRITE;

  /* TODO: Check that the following fn_format is really needed */
  error= mi_create(fn_format(buff, name, "", "",
                             MY_UNPACK_FILENAME|MY_APPEND_EXT),
                   share->keys, keydef,
                   create_records, recinfo,
                   0, (MI_UNIQUEDEF*) 0,
                   &create_info, create_flags);
  free((unsigned char*) recinfo);
  return(error);
}


int ha_myisam::rename_table(const char * from, const char * to)
{
  return mi_rename(from,to);
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

  if (!table->s->next_number_key_offset)
  {						// Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    *first_value= stats.auto_increment_value;
    /* MyISAM has only table-level lock, so reserves to +inf */
    *nb_reserved_values= UINT64_MAX;
    return;
  }

  /* it's safe to call the following if bulk_insert isn't on */
  mi_flush_bulk_insert(file, table->s->next_number_index);

  (void) extra(HA_EXTRA_KEYREAD);
  key_copy(key, table->record[0],
           table->key_info + table->s->next_number_index,
           table->s->next_number_key_offset);
  error= mi_rkey(file, table->record[1], (int) table->s->next_number_index,
                 key, make_prev_keypart_map(table->s->next_number_keypart),
                 HA_READ_PREFIX_LAST);
  if (error)
    nr= 1;
  else
  {
    /* Get data from record[1] */
    nr= ((uint64_t) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
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


bool ha_myisam::check_if_incompatible_data(HA_CREATE_INFO *create_info,
					   uint32_t table_changes)
{
  uint32_t options= table->s->db_options_in_use;

  if (create_info->auto_increment_value != stats.auto_increment_value ||
      create_info->data_file_name != data_file_name ||
      create_info->index_file_name != index_file_name ||
      table_changes == IS_EQUAL_NO ||
      table_changes & IS_EQUAL_PACK_LENGTH) // Not implemented yet
    return COMPATIBLE_DATA_NO;

  if ((options & (HA_OPTION_PACK_RECORD | HA_OPTION_CHECKSUM |
		  HA_OPTION_DELAY_KEY_WRITE)) !=
      (create_info->table_options & (HA_OPTION_PACK_RECORD |
                                     HA_OPTION_CHECKSUM |
			      HA_OPTION_DELAY_KEY_WRITE)))
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

int myisam_deinit(void *p)
{

  MyisamEngine *engine= static_cast<MyisamEngine *>(p);
  delete engine;

  pthread_mutex_destroy(&THR_LOCK_myisam);

  return mi_panic(HA_PANIC_CLOSE);
}

static int myisam_init(void *p)
{
  StorageEngine **engine= static_cast<StorageEngine **>(p);
  
  *engine= new MyisamEngine(engine_name);
  (*engine)->state= SHOW_OPTION_YES;
  (*engine)->flags= HTON_CAN_RECREATE | HTON_SUPPORT_LOG_TABLES;

  return 0;
}



/****************************************************************************
 * MyISAM MRR implementation: use DS-MRR
 ***************************************************************************/

int ha_myisam::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                     uint32_t n_ranges, uint32_t mode,
                                     HANDLER_BUFFER *buf)
{
  return ds_mrr.dsmrr_init(this, &table->key_info[active_index],
                           seq, seq_init_param, n_ranges, mode, buf);
}

int ha_myisam::multi_range_read_next(char **range_info)
{
  return ds_mrr.dsmrr_next(this, range_info);
}

ha_rows ha_myisam::multi_range_read_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
                                               void *seq_init_param,
                                               uint32_t n_ranges, uint32_t *bufsz,
                                               uint32_t *flags, COST_VECT *cost)
{
  /*
    This call is here because there is no location where this->table would
    already be known.
    TODO: consider moving it into some per-query initialization call.
  */
  ds_mrr.init(this, table);
  return ds_mrr.dsmrr_info_const(keyno, seq, seq_init_param, n_ranges, bufsz,
                                 flags, cost);
}

int ha_myisam::multi_range_read_info(uint32_t keyno, uint32_t n_ranges, uint32_t keys,
                                     uint32_t *bufsz, uint32_t *flags, COST_VECT *cost)
{
  ds_mrr.init(this, table);
  return ds_mrr.dsmrr_info(keyno, n_ranges, keys, bufsz, flags, cost);
}

/* MyISAM MRR implementation ends */


/* Index condition pushdown implementation*/


Item *ha_myisam::idx_cond_push(uint32_t keyno_arg, Item* idx_cond_arg)
{
  pushed_idx_cond_keyno= keyno_arg;
  pushed_idx_cond= idx_cond_arg;
  in_range_check_pushed_down= true;
  if (active_index == pushed_idx_cond_keyno)
    mi_set_index_cond_func(file, index_cond_func_myisam, this);
  return NULL;
}

static DRIZZLE_SYSVAR_UINT(block_size, block_size,
                           PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                           N_("Block size to be used for MyISAM index pages."),
                           NULL, NULL, MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, 
                           MI_MAX_KEY_BLOCK_LENGTH, 0);

static DRIZZLE_SYSVAR_UINT(repair_threads, repair_threads,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Number of threads to use when repairing MyISAM tables. The value of "
                              "1 disables parallel repair."),
                           NULL, NULL, 1, 1, UINT32_MAX, 0);

static DRIZZLE_SYSVAR_ULONGLONG(max_sort_file_size, max_sort_file_size,
                                PLUGIN_VAR_RQCMDARG,
                                N_("Don't use the fast sort index method to created index if the temporary file would get bigger than this."),
                                NULL, NULL, INT32_MAX, 0, UINT64_MAX, 0);

static DRIZZLE_SYSVAR_ULONGLONG(sort_buffer_size, sort_buffer_size,
                                PLUGIN_VAR_RQCMDARG,
                                N_("The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE."),
                                NULL, NULL, 8192*1024, 1024, SIZE_MAX, 0);

extern uint32_t data_pointer_size;
static DRIZZLE_SYSVAR_UINT(data_pointer_size, data_pointer_size,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Default pointer size to be used for MyISAM tables."),
                           NULL, NULL, 6, 2, 7, 0);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(block_size),
  DRIZZLE_SYSVAR(repair_threads),
  DRIZZLE_SYSVAR(max_sort_file_size),
  DRIZZLE_SYSVAR(sort_buffer_size),
  DRIZZLE_SYSVAR(data_pointer_size),
  NULL
};


drizzle_declare_plugin(myisam)
{
  DRIZZLE_STORAGE_ENGINE_PLUGIN,
  "MyISAM",
  "1.0",
  "MySQL AB",
  "Default engine as of MySQL 3.23 with great performance",
  PLUGIN_LICENSE_GPL,
  myisam_init, /* Plugin Init */
  myisam_deinit, /* Plugin Deinit */
  NULL,                       /* status variables                */
  system_variables,           /* system variables */
  NULL                        /* config options                  */
}
drizzle_declare_plugin_end;
