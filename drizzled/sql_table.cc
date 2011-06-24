/* Copyright (C) 2000-2004 MySQL AB

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

/* drop and alter of tables */

#include <config.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/show.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/item/int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/transaction_services.h>
#include <drizzled/transaction_services.h>
#include <drizzled/table_proto.h>
#include <drizzled/plugin/client.h>
#include <drizzled/identifier.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/charset.h>
#include <drizzled/definition/cache.h>
#include <drizzled/system_variables.h>
#include <drizzled/statement/alter_table.h>
#include <drizzled/sql_table.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/typelib.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>

#include <algorithm>
#include <sstream>

#include <boost/unordered_set.hpp>

using namespace std;

namespace drizzled {

bool is_primary_key(KeyInfo *key_info)
{
  static const char * primary_key_name="PRIMARY";
  return (strcmp(key_info->name, primary_key_name)==0);
}

const char* is_primary_key_name(const char* key_name)
{
  static const char * primary_key_name="PRIMARY";
  if (strcmp(key_name, primary_key_name)==0)
    return key_name;
  else
    return NULL;
}

static bool check_if_keyname_exists(const char *name,KeyInfo *start, KeyInfo *end);
static char *make_unique_key_name(const char *field_name,KeyInfo *start,KeyInfo *end);

static bool prepare_blob_field(Session *session, CreateField *sql_field);

void set_table_default_charset(HA_CREATE_INFO *create_info, const char *db)
{
  /*
    If the table character set was not given explicitly,
    let's fetch the database default character set and
    apply it to the table.
  */
  identifier::Schema identifier(db);
  if (create_info->default_table_charset == NULL)
    create_info->default_table_charset= plugin::StorageEngine::getSchemaCollation(identifier);
}

/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    rm_table_part2()
    session			Thread Cursor
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables

  @todo
    When logging to the binary log, we should log
    tmp_tables and transactional tables as separate statements if we
    are in a transaction;  This is needed to get these tables into the
    cached binary log that is only written on COMMIT.

   The current code only writes DROP statements that only uses temporary
   tables to the cache binary log.  This should be ok on most cases, but
   not all.

 RETURN
   0	ok
   1	Error
   -1	Thread was killed
*/

int rm_table_part2(Session *session, TableList *tables, bool if_exists,
                   bool drop_temporary)
{
  TableList *table;
  util::string::vector wrong_tables;
  int error= 0;
  bool foreign_key_error= false;

  do
  {
    boost::mutex::scoped_lock scopedLock(table::Cache::mutex());

    if (not drop_temporary && session->lock_table_names_exclusively(tables))
    {
      return 1;
    }

    /* Don't give warnings for not found errors, as we already generate notes */
    session->no_warnings_for_error= 1;

    for (table= tables; table; table= table->next_local)
    {
      identifier::Table tmp_identifier(table->getSchemaName(), table->getTableName());

      error= session->open_tables.drop_temporary_table(tmp_identifier);

      switch (error) {
      case  0:
        // removed temporary table
        continue;
      case -1:
        error= 1;
        break;
      default:
        // temporary table not found
        error= 0;
      }

      if (drop_temporary == false)
      {
        abort_locked_tables(session, tmp_identifier);
        table::Cache::removeTable(*session, tmp_identifier, RTFC_WAIT_OTHER_THREAD_FLAG | RTFC_CHECK_KILLED_FLAG);
        /*
          If the table was used in lock tables, remember it so that
          unlock_table_names can free it
        */
        Table *locked_table= drop_locked_tables(session, tmp_identifier);
        if (locked_table)
          table->table= locked_table;

        if (session->getKilled())
        {
          error= -1;
          break;
        }
      }
      identifier::Table identifier(table->getSchemaName(), table->getTableName(), table->getInternalTmpTable() ? message::Table::INTERNAL : message::Table::STANDARD);

      message::table::shared_ptr message= plugin::StorageEngine::getTableMessage(*session, identifier, true);

      if (drop_temporary || not plugin::StorageEngine::doesTableExist(*session, identifier))
      {
        // Table was not found on disk and table can't be created from engine
        if (if_exists)
        {
          push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                              ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
                              table->getTableName());
        }
        else
        {
          error= 1;
        }
      }
      else
      {
        drizzled::error_t local_error;

        /* Generate transaction event ONLY when we successfully drop */ 
        if (plugin::StorageEngine::dropTable(*session, identifier, local_error))
        {
          if (message) // If we have no definition, we don't know if the table should have been replicated
          {
            TransactionServices::dropTable(*session, identifier, *message, if_exists);
          }
        }
        else
        {
          if (local_error == HA_ERR_NO_SUCH_TABLE and if_exists)
          {
            error= 0;
            session->clear_error();
          }

          if (local_error == HA_ERR_ROW_IS_REFERENCED)
          {
            /* the table is referenced by a foreign key constraint */
            foreign_key_error= true;
          }
          error= local_error;
        }
      }

      if (error)
      {
        wrong_tables.push_back(table->getTableName());
      }
    }

    tables->unlock_table_names();

  } while (0);

  if (wrong_tables.size())
  {
    if (not foreign_key_error)
    {
      std::string table_error;

      BOOST_FOREACH(util::string::vector::reference iter, wrong_tables)
      {
        table_error+= iter;
        table_error+= ',';
      }
      table_error.resize(table_error.size() -1);

      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      table_error.c_str());
    }
    else
    {
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    }
    error= 1;
  }

  session->no_warnings_for_error= 0;

  return error;
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keys where all column are NOT NULL
  - UNIQUE keys that don't contain partial segments
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KeyInfo *a, KeyInfo *b)
{
  ulong a_flags= a->flags, b_flags= b->flags;

  if (a_flags & HA_NOSAME)
  {
    if (!(b_flags & HA_NOSAME))
      return -1;
    if ((a_flags ^ b_flags) & (HA_NULL_PART_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a_flags & (HA_NULL_PART_KEY)) ? 1 : -1;
    }
    if (is_primary_key(a))
      return -1;
    if (is_primary_key(b))
      return 1;
    /* Sort keys don't containing partial segments before others */
    if ((a_flags ^ b_flags) & HA_KEY_HAS_PART_KEY_SEG)
      return (a_flags & HA_KEY_HAS_PART_KEY_SEG) ? 1 : -1;
  }
  else if (b_flags & HA_NOSAME)
    return 1;					// Prefer b

  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
          (a->usable_key_parts > b->usable_key_parts) ? 1 :
          0);
}

/*
  Check TYPELIB (set or enum) for duplicates

  SYNOPSIS
    check_duplicates_in_interval()
    set_or_name   "SET" or "ENUM" string for warning message
    name	  name of the checked column
    typelib	  list of values for the column
    dup_val_count  returns count of duplicate elements

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    0             ok
    1             Error
*/

class typelib_set_member
{
public:
  string s;
  const charset_info_st * const cs;

  typelib_set_member(const char* value, unsigned int length,
                     const charset_info_st * const charset)
    : s(value, length),
      cs(charset)
  {}
};

static bool operator==(typelib_set_member const& a, typelib_set_member const& b)
{
  return (my_strnncoll(a.cs,
                       (const unsigned char*)a.s.c_str(), a.s.length(),
                       (const unsigned char*)b.s.c_str(), b.s.length())==0);
}


namespace
{
class typelib_set_member_hasher
{
  boost::hash<string> hasher;
public:
  std::size_t operator()(const typelib_set_member& t) const
  {
    return hasher(t.s);
  }
};
}

static bool check_duplicates_in_interval(const char *set_or_name,
                                         const char *name, TYPELIB *typelib,
                                         const charset_info_st * const cs,
                                         unsigned int *dup_val_count)
{
  TYPELIB tmp= *typelib;
  const char **cur_value= typelib->type_names;
  unsigned int *cur_length= typelib->type_lengths;
  *dup_val_count= 0;

  boost::unordered_set<typelib_set_member, typelib_set_member_hasher> interval_set;

  for ( ; tmp.count > 0; cur_value++, cur_length++)
  {
    tmp.type_names++;
    tmp.type_lengths++;
    tmp.count--;
    if (interval_set.count(typelib_set_member(*cur_value, *cur_length, cs)))
    {
      my_error(ER_DUPLICATED_VALUE_IN_TYPE, MYF(0),
               name,*cur_value,set_or_name);
      return 1;
    }
    else
      interval_set.insert(typelib_set_member(*cur_value, *cur_length, cs));
  }
  return 0;
}


/*
  Check TYPELIB (set or enum) max and total lengths

  SYNOPSIS
    calculate_interval_lengths()
    cs            charset+collation pair of the interval
    typelib       list of values for the column
    max_length    length of the longest item
    tot_length    sum of the item lengths

  DESCRIPTION
    After this function call:
    - ENUM uses max_length
    - SET uses tot_length.

  RETURN VALUES
    void
*/
static void calculate_interval_lengths(const charset_info_st * const cs,
                                       TYPELIB *interval,
                                       uint32_t *max_length,
                                       uint32_t *tot_length)
{
  const char **pos;
  uint32_t *len;
  *max_length= *tot_length= 0;
  for (pos= interval->type_names, len= interval->type_lengths;
       *pos ; pos++, len++)
  {
    uint32_t length= cs->cset->numchars(cs, *pos, *pos + *len);
    *tot_length+= length;
    set_if_bigger(*max_length, (uint32_t)length);
  }
}

/*
  Prepare a create_table instance for packing

  SYNOPSIS
    prepare_create_field()
    sql_field     field to prepare for packing
    blob_columns  count for BLOBs
    timestamps    count for timestamps

  DESCRIPTION
    This function prepares a CreateField instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/
int prepare_create_field(CreateField *sql_field,
                         uint32_t *blob_columns,
                         int *timestamps,
                         int *timestamps_with_niladic)
{
  unsigned int dup_val_count;

  /*
    This code came from mysql_prepare_create_table.
    Indent preserved to make patching easier
  */
  assert(sql_field->charset);

  switch (sql_field->sql_type) {
  case DRIZZLE_TYPE_BLOB:
    sql_field->length= 8; // Unireg field length
    (*blob_columns)++;
    break;

  case DRIZZLE_TYPE_ENUM:
    {
      if (check_duplicates_in_interval("ENUM",
				       sql_field->field_name,
				       sql_field->interval,
				       sql_field->charset,
				       &dup_val_count))
      {
	return 1;
      }
    }
    break;

  case DRIZZLE_TYPE_MICROTIME:
  case DRIZZLE_TYPE_TIMESTAMP:
    /* We should replace old TIMESTAMP fields with their newer analogs */
    if (sql_field->unireg_check == Field::TIMESTAMP_OLD_FIELD)
    {
      if (!*timestamps)
      {
        sql_field->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
        (*timestamps_with_niladic)++;
      }
      else
      {
        sql_field->unireg_check= Field::NONE;
      }
    }
    else if (sql_field->unireg_check != Field::NONE)
    {
      (*timestamps_with_niladic)++;
    }

    (*timestamps)++;

    break;

  case DRIZZLE_TYPE_BOOLEAN:
  case DRIZZLE_TYPE_DATE:  // Rest of string types
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_DECIMAL:
  case DRIZZLE_TYPE_DOUBLE:
  case DRIZZLE_TYPE_LONG:
  case DRIZZLE_TYPE_LONGLONG:
  case DRIZZLE_TYPE_NULL:
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_UUID:
  case DRIZZLE_TYPE_VARCHAR:
    break;
  }

  return 0;
}

static int prepare_create_table(Session *session,
                                HA_CREATE_INFO *create_info,
                                message::Table &create_proto,
                                AlterInfo *alter_info,
                                bool tmp_table,
                                uint32_t *db_options,
                                KeyInfo **key_info_buffer,
                                uint32_t *key_count,
                                int select_field_count)
{
  const char	*key_name;
  CreateField	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KeyInfo		*key_info;
  KeyPartInfo *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		dup_no;
  int		select_field_pos,auto_increment=0;
  List<CreateField>::iterator it(alter_info->create_list.begin());
  List<CreateField>::iterator it2(alter_info->create_list.begin());
  uint32_t total_uneven_bit_length= 0;

  plugin::StorageEngine *engine= plugin::StorageEngine::findByName(create_proto.engine().name());

  select_field_pos= alter_info->create_list.size() - select_field_count;
  null_fields=blob_columns=0;
  max_key_length= engine->max_key_length();

  for (int32_t field_no=0; (sql_field=it++) ; field_no++)
  {
    const charset_info_st *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;

    if (!sql_field->charset)
      sql_field->charset= create_info->default_table_charset;

    /*
      table_charset is set in ALTER Table if we want change character set
      for all varchar/char columns.
      But the table charset must not affect the BLOB fields, so don't
      allow to change my_charset_bin to somethig else.
    */
    if (create_info->table_charset && sql_field->charset != &my_charset_bin)
      sql_field->charset= create_info->table_charset;

    save_cs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
        !(sql_field->charset= get_charset_by_csname(sql_field->charset->csname, MY_CS_BINSORT)))
    {
      char tmp[64];
      char *tmp_pos= tmp;
      strncpy(tmp_pos, save_cs->csname, sizeof(tmp)-4);
      tmp_pos+= strlen(tmp);
      strncpy(tmp_pos, STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      return true;
    }

    /*
      Convert the default value from client character
      set into the column character set if necessary.
    */
    if (sql_field->def &&
        save_cs != sql_field->def->collation.collation &&
        (sql_field->sql_type == DRIZZLE_TYPE_ENUM))
    {
      /*
        Starting from 5.1 we work here with a copy of CreateField
        created by the caller, not with the instance that was
        originally created during parsing. It's OK to create
        a temporary item and initialize with it a member of the
        copy -- this item will be thrown away along with the copy
        at the end of execution, and thus not introduce a dangling
        pointer in the parsed tree of a prepared statement or a
        stored procedure statement.
      */
      sql_field->def= sql_field->def->safe_charset_converter(save_cs);

      if (sql_field->def == NULL)
      {
        /* Could not convert */
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        return true;
      }
    }

    if (sql_field->sql_type == DRIZZLE_TYPE_ENUM)
    {
      size_t dummy;
      const charset_info_st * const cs= sql_field->charset;
      TYPELIB *interval= sql_field->interval;

      /*
        Create typelib from interval_list, and if necessary
        convert strings from client character set to the
        column character set.
      */
      if (!interval)
      {
        /*
          Create the typelib in runtime memory - we will free the
          occupied memory at the same time when we free this
          sql_field -- at the end of execution.
        */
        interval= sql_field->interval= typelib(*session->mem_root, sql_field->interval_list);

        List<String>::iterator int_it(sql_field->interval_list.begin());
        String conv, *tmp;
        char comma_buf[4];
        int comma_length= cs->cset->wc_mb(cs, ',', (unsigned char*) comma_buf, (unsigned char*) comma_buf + sizeof(comma_buf));
        assert(comma_length > 0);

        for (uint32_t i= 0; (tmp= int_it++); i++)
        {
          uint32_t lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(), cs, &dummy))
          {
            conv.copy(tmp->ptr(), tmp->length(), cs);
            interval->type_names[i]= session->mem.strmake(conv);
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i], interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((unsigned char *)interval->type_names[i])[lengthsp]= '\0';
        }
        sql_field->interval_list.clear(); // Don't need interval_list anymore
      }

      /* DRIZZLE_TYPE_ENUM */
      {
        uint32_t field_length;
        assert(sql_field->sql_type == DRIZZLE_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if (sql_field->flags & NOT_NULL_FLAG)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              return true;
            }

            /* else, the defaults yield the correct length for NULLs. */
          }
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (interval->find_type2(def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              return true;
            }
          }
        }
        uint32_t new_dummy;
        calculate_interval_lengths(cs, interval, &field_length, &new_dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, (uint32_t)MAX_FIELD_WIDTH-1);
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(session, sql_field))
      return true;

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      return true;
    }

    /* Check if we have used the same field name before */
    for (dup_no=0; (dup_field=it2++) != sql_field; dup_no++)
    {
      if (my_strcasecmp(system_charset_info,
                        sql_field->field_name,
                        dup_field->field_name) == 0)
      {
	/*
	  If this was a CREATE ... SELECT statement, accept a field
	  redefinition if we are changing a field in the SELECT part
	*/
	if (field_no < select_field_pos || dup_no >= select_field_pos)
	{
	  my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
	  return true;
	}
	else
	{
	  /* Field redefined */
	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->create_length_to_internal_length();
	  sql_field->unireg_check=	dup_field->unireg_check;
          /*
            We're making one field from two, the result field will have
            dup_field->flags as flags. If we've incremented null_fields
            because of sql_field->flags, decrement it back.
          */
          if (!(sql_field->flags & NOT_NULL_FLAG))
            null_fields--;
	  sql_field->flags=		dup_field->flags;
          sql_field->interval=          dup_field->interval;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }

    /** @todo Get rid of this MyISAM-specific crap. */
    if (not create_proto.engine().name().compare("MyISAM") &&
        ((sql_field->flags & BLOB_FLAG) ||
         (sql_field->sql_type == DRIZZLE_TYPE_VARCHAR)))
    {
      (*db_options)|= HA_OPTION_PACK_RECORD;
    }

    it2= alter_info->create_list.begin();
  }

  /* record_offset will be increased with 'length-of-null-bits' later */
  record_offset= 0;
  null_fields+= total_uneven_bit_length;

  it= alter_info->create_list.begin();
  while ((sql_field=it++))
  {
    assert(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns,
			     &timestamps, &timestamps_with_niladic))
      return true;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    return true;
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    return true;
  }
  if (auto_increment &&
      (engine->check_flag(HTON_BIT_NO_AUTO_INCREMENT)))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    return true;
  }

  if (blob_columns && (engine->check_flag(HTON_BIT_NO_BLOBS)))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    return true;
  }

  /* Create keys */

  List<Key>::iterator key_iterator(alter_info->key_list.begin());
  List<Key>::iterator key_iterator2(alter_info->key_list.begin());
  uint32_t key_parts=0, fk_key_count=0;
  bool primary_key=0,unique_key=0;
  Key *key, *key2;
  uint32_t tmp, key_number;
  /* special marker for keys to be ignored */
  static char ignore_key[1];

  /* Calculate number of key segements */
  *key_count= 0;

  while ((key=key_iterator++))
  {
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      if (((Foreign_key *)key)->validate(alter_info->create_list))
        return true;

      Foreign_key *fk_key= (Foreign_key*) key;

      add_foreign_key_to_table_message(&create_proto,
                                       fk_key->name.str,
                                       fk_key->columns,
                                       fk_key->ref_table,
                                       fk_key->ref_columns,
                                       fk_key->delete_opt,
                                       fk_key->update_opt,
                                       fk_key->match_opt);

      if (fk_key->ref_columns.size() &&
	  fk_key->ref_columns.size() != fk_key->columns.size())
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name.str ? fk_key->name.str :
                                     "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	return true;
      }
      continue;
    }
    (*key_count)++;
    tmp= engine->max_key_parts();
    if (key->columns.size() > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      return true;
    }
    if (check_identifier_name(&key->name, ER_TOO_LONG_IDENT))
      return true;
    key_iterator2= alter_info->key_list.begin();
    if (key->type != Key::FOREIGN_KEY)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.
        */
        if ((key2->type != Key::FOREIGN_KEY &&
             key2->name.str != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* @todo issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.size() <
               key2->columns.size()))
            key->name.str= ignore_key;
          else
          {
            key2->name.str= ignore_key;
            key_parts-= key2->columns.size();
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name.str != ignore_key)
      key_parts+=key->columns.size();
    else
      (*key_count)--;
    if (key->name.str && !tmp_table && (key->type != Key::PRIMARY) &&
        is_primary_key_name(key->name.str))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
      return true;
    }
  }
  tmp= engine->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    return true;
  }

  (*key_info_buffer)= key_info= (KeyInfo*) memory::sql_calloc(sizeof(KeyInfo) * (*key_count));
  key_part_info=(KeyPartInfo*) memory::sql_calloc(sizeof(KeyPartInfo)*key_parts);

  key_iterator= alter_info->key_list.begin();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint32_t key_length=0;
    Key_part_spec *column;

    if (key->name.str == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name.str == ignore_key);
      if (!key)
	break;
    }

    switch (key->type) {
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FOREIGN_KEY:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->key_parts=(uint8_t) key->columns.size();
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;
    key_info->algorithm= key->key_create_info.algorithm;

    uint32_t tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                           key->key_create_info.comment.str,
                                           key->key_create_info.comment.str +
                                           key->key_create_info.comment.length,
                                           INDEX_COMMENT_MAXLEN);

    if (tmp_len < key->key_create_info.comment.length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
               key->key_create_info.comment.str,"INDEX COMMENT",
               (uint32_t) INDEX_COMMENT_MAXLEN);
      return -1;
    }

    key_info->comment.length= key->key_create_info.comment.length;
    if (key_info->comment.length > 0)
    {
      key_info->flags|= HA_USES_COMMENT;
      key_info->comment.str= key->key_create_info.comment.str;
    }

    message::Table::Field *protofield= NULL;

    List<Key_part_spec>::iterator cols(key->columns.begin());
    List<Key_part_spec>::iterator cols2(key->columns.begin());
    for (uint32_t column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint32_t length;
      Key_part_spec *dup_column;
      int proto_field_nr= 0;

      it= alter_info->create_list.begin();
      field=0;
      while ((sql_field=it++) && ++proto_field_nr &&
	     my_strcasecmp(system_charset_info,
			   column->field_name.str,
			   sql_field->field_name))
      {
	field++;
      }

      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
	return true;
      }

      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
                           column->field_name.str, dup_column->field_name.str))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name.str);
	  return true;
	}
      }
      cols2= key->columns.begin();

      if (create_proto.field_size() > 0)
        protofield= create_proto.mutable_field(proto_field_nr - 1);

      {
        column->length*= sql_field->charset->mbmaxlen;

        if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
        {
          if (! (engine->check_flag(HTON_BIT_CAN_INDEX_BLOBS)))
          {
            my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name.str);
            return true;
          }
          if (! column->length)
          {
            my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name.str);
            return true;
          }
        }

        if (! (sql_field->flags & NOT_NULL_FLAG))
        {
          if (key->type == Key::PRIMARY)
          {
            /* Implicitly set primary key fields to NOT NULL for ISO conf. */
            sql_field->flags|= NOT_NULL_FLAG;
            null_fields--;

            if (protofield)
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= protofield->mutable_constraints();
              constraints->set_is_notnull(true);
            }

          }
          else
          {
            key_info->flags|= HA_NULL_PART_KEY;
            if (! (engine->check_flag(HTON_BIT_NULL_IN_KEY)))
            {
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name.str);
              return true;
            }
          }
        }

        if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
        {
          if (column_nr == 0 || (engine->check_flag(HTON_BIT_AUTO_PART_KEY)))
            auto_increment--;			// Field is used
        }
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16_t) sql_field->offset;
      key_part_info->key_type= 0;
      length= sql_field->key_length;

      if (column->length)
      {
	if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
	{
	  if ((length=column->length) > max_key_length ||
	      length > engine->max_key_part_length())
	  {
	    length= min(max_key_length, engine->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[DRIZZLE_ERRMSG_SIZE];
	      snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
                       length);
	      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
              /* Align key length to multibyte char boundary */
              length-= length % sql_field->charset->mbmaxlen;
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      return true;
	    }
	  }
	}
	else if ((column->length > length ||
            ! Field::type_can_have_key_part(sql_field->sql_type)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  return true;
	}
	else if (! (engine->check_flag(HTON_BIT_NO_PREFIX_CHAR_KEYS)))
        {
	  length=column->length;
        }
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name.str);
	  return true;
      }
      if (length > engine->max_key_part_length())
      {
        length= engine->max_key_part_length();
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[DRIZZLE_ERRMSG_SIZE];
	  snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
                   length);
	  push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
          /* Align key length to multibyte char boundary */
          length-= length % sql_field->charset->mbmaxlen;
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  return true;
	}
      }
      key_part_info->length=(uint16_t) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
          (length >= KEY_DEFAULT_PACK_LENGTH &&
           (sql_field->sql_type == DRIZZLE_TYPE_VARCHAR ||
            sql_field->sql_type == DRIZZLE_TYPE_BLOB)))
      {
        if ((column_nr == 0 && sql_field->sql_type == DRIZZLE_TYPE_BLOB) ||
            sql_field->sql_type == DRIZZLE_TYPE_VARCHAR)
        {
          key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
        }
        else
        {
          key_info->flags|= HA_PACK_KEY;
        }
      }
      /* Check if the key segment is partial, set the key flag accordingly */
      if (length != sql_field->key_length)
        key_info->flags|= HA_KEY_HAS_PART_KEY_SEG;

      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_message(ER_MULTIPLE_PRI_KEY, ER(ER_MULTIPLE_PRI_KEY),
                       MYF(0));
	    return true;
	  }
          static const char pkey_name[]= "PRIMARY";
	  key_name=pkey_name;
	  primary_key=1;
	}
	else if (!(key_name= key->name.str))
	  key_name=make_unique_key_name(sql_field->field_name,
					*key_info_buffer, key_info);
	if (check_if_keyname_exists(key_name, *key_info_buffer, key_info))
	{
	  my_error(ER_DUP_KEYNAME, MYF(0), key_name);
	  return true;
	}
	key_info->name=(char*) key_name;
      }
    }

    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      return true;
    }

    if (!(key_info->flags & HA_NULL_PART_KEY))
    {
      unique_key=1;
    }

    key_info->key_length=(uint16_t) key_length;

    if (key_length > max_key_length)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      return true;
    }

    key_info++;
  }

  if (!unique_key && !primary_key &&
      (engine->check_flag(HTON_BIT_REQUIRE_PRIMARY_KEY)))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    return true;
  }

  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    return true;
  }
  /* Sort keys in optimized order */
  internal::my_qsort((unsigned char*) *key_info_buffer, *key_count, sizeof(KeyInfo),
	             (qsort_cmp) sort_keys);

  /* Check fields. */
  it= alter_info->create_list.begin();
  while ((sql_field=it++))
  {
    Field::utype type= (Field::utype) MTYP_TYPENR(sql_field->unireg_check);

    if (session->variables.sql_mode & MODE_NO_ZERO_DATE &&
        !sql_field->def &&
        (sql_field->sql_type == DRIZZLE_TYPE_TIMESTAMP  or sql_field->sql_type == DRIZZLE_TYPE_MICROTIME) &&
        (sql_field->flags & NOT_NULL_FLAG) &&
        (type == Field::NONE || type == Field::TIMESTAMP_UN_FIELD))
    {
      /*
        An error should be reported if:
          - NO_ZERO_DATE SQL mode is active;
          - there is no explicit DEFAULT clause (default column value);
          - this is a TIMESTAMP column;
          - the column is not NULL;
          - this is not the DEFAULT CURRENT_TIMESTAMP column.

        In other words, an error should be reported if
          - NO_ZERO_DATE SQL mode is active;
          - the column definition is equivalent to
            'column_name TIMESTAMP DEFAULT 0'.
      */

      my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
      return true;
    }
  }

  return false;
}

/*
  Extend long VARCHAR fields to blob & prepare field if it's a blob

  SYNOPSIS
    prepare_blob_field()
    sql_field		Field to check

  RETURN
    0	ok
    1	Error (sql_field can't be converted to blob)
        In this case the error is given
*/

static bool prepare_blob_field(Session *,
                               CreateField *sql_field)
{

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
             MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
    return 1;
  }

  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  return 0;
}

static bool locked_create_event(Session *session,
                                const identifier::Table &identifier,
                                HA_CREATE_INFO *create_info,
                                message::Table &table_proto,
                                AlterInfo *alter_info,
                                bool is_if_not_exists,
                                bool internal_tmp_table,
                                uint db_options,
                                uint key_count,
                                KeyInfo *key_info_buffer)
{
  bool error= true;

  {

    /*
      @note if we are building a temp table we need to check to see if a temp table
      already exists, otherwise we just need to find out if a normal table exists (aka it is fine
      to create a table under a temporary table.
    */
    bool exists= 
      plugin::StorageEngine::doesTableExist(*session, identifier, 
                                            identifier.getType() != message::Table::STANDARD );

    if (exists)
    {
      if (is_if_not_exists)
      {
        error= false;
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                            identifier.getTableName().c_str());
        create_info->table_existed= 1;		// Mark that table existed
        return error;
      }

      my_error(ER_TABLE_EXISTS_ERROR, identifier);

      return error;
    }

    if (identifier.getType() == message::Table::STANDARD) // We have a real table
    {
      /*
        We don't assert here, but check the result, because the table could be
        in the table definition cache and in the same time the .frm could be
        missing from the disk, in case of manual intervention which deletes
        the .frm cursor. The user has to use FLUSH TABLES; to clear the cache.
        Then she could create the table. This case is pretty obscure and
        therefore we don't introduce a new error message only for it.
      */
      /*
        @todo improve this error condition.
      */
      if (definition::Cache::find(identifier.getKey()))
      {
        my_error(ER_TABLE_EXISTS_ERROR, identifier);

        return error;
      }
    }
  }

  session->set_proc_info("creating table");
  create_info->table_existed= 0;		// Mark that table is created

  create_info->table_options= db_options;

  if (not rea_create_table(session, identifier,
                           table_proto,
                           create_info, alter_info->create_list,
                           key_count, key_info_buffer))
  {
    return error;
  }

  if (identifier.getType() == message::Table::TEMPORARY)
  {
    /* Open table and put in temporary table list */
    if (not (session->open_temporary_table(identifier)))
    {
      (void) session->open_tables.rm_temporary_table(identifier);
      return error;
    }
  }

  /* 
    We keep this behind the lock to make sure ordering is correct for a table.
    This is a very unlikely problem where before we would write out to the
    trans log, someone would do a delete/create operation.
  */

  if (table_proto.type() == message::Table::STANDARD && not internal_tmp_table)
  {
    TransactionServices::createTable(*session, table_proto);
  }

  return false;
}


/*
  Ignore the name of this function... it locks :(

  Create a table

  SYNOPSIS
    create_table_no_lock()
    session			Thread object
    db			Database
    table_name		Table name
    create_info	        Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create
    internal_tmp_table  Set to 1 if this is an internal temporary table
			(From ALTER Table)
    select_field_count

  DESCRIPTION
    If one creates a temporary table, this is automatically opened

    Note that this function assumes that caller already have taken
    name-lock on table being created or used some other way to ensure
    that concurrent operations won't intervene. create_table()
    is a wrapper that can be used for this.

  RETURN VALUES
    false OK
    true  error
*/

bool create_table_no_lock(Session *session,
                                const identifier::Table &identifier,
                                HA_CREATE_INFO *create_info,
				message::Table &table_proto,
                                AlterInfo *alter_info,
                                bool internal_tmp_table,
                                uint32_t select_field_count,
                                bool is_if_not_exists)
{
  uint		db_options, key_count;
  KeyInfo		*key_info_buffer;
  bool		error= true;

  /* Check for duplicate fields and check type of table to create */
  if (not alter_info->create_list.size())
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    return true;
  }
  assert(identifier.getTableName() == table_proto.name());
  db_options= create_info->table_options;

  set_table_default_charset(create_info, identifier.getSchemaName().c_str());

  /* Build a Table object to pass down to the engine, and the do the actual create. */
  if (not prepare_create_table(session, create_info, table_proto, alter_info,
                               internal_tmp_table,
                               &db_options,
                               &key_info_buffer, &key_count,
                               select_field_count))
  {
    boost::mutex::scoped_lock lock(table::Cache::mutex()); /* CREATE TABLE (some confussion on naming, double check) */
    error= locked_create_event(session,
                               identifier,
                               create_info,
                               table_proto,
                               alter_info,
                               is_if_not_exists,
                               internal_tmp_table,
                               db_options, key_count,
                               key_info_buffer);
  }

  session->set_proc_info("After create");

  return(error);
}

/**
  @note the following two methods implement create [temporary] table.
*/
static bool drizzle_create_table(Session *session,
                                 const identifier::Table &identifier,
                                 HA_CREATE_INFO *create_info,
                                 message::Table &table_proto,
                                 AlterInfo *alter_info,
                                 bool internal_tmp_table,
                                 uint32_t select_field_count,
                                 bool is_if_not_exists)
{
  Table *name_lock= session->lock_table_name_if_not_cached(identifier);
  bool result;
  if (name_lock == NULL)
  {
    if (is_if_not_exists)
    {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          identifier.getTableName().c_str());
      create_info->table_existed= 1;
      result= false;
    }
    else
    {
      my_error(ER_TABLE_EXISTS_ERROR, identifier);
      result= true;
    }
  }
  else
  {
    result= create_table_no_lock(session,
                                       identifier,
                                       create_info,
                                       table_proto,
                                       alter_info,
                                       internal_tmp_table,
                                       select_field_count,
                                       is_if_not_exists);
  }

  if (name_lock)
  {
    boost::mutex::scoped_lock lock(table::Cache::mutex()); /* Lock for removing name_lock during table create */
    session->unlink_open_table(name_lock);
  }

  return(result);
}


/*
  Database locking aware wrapper for create_table_no_lock(),
*/
bool create_table(Session *session,
                        const identifier::Table &identifier,
                        HA_CREATE_INFO *create_info,
			message::Table &table_proto,
                        AlterInfo *alter_info,
                        bool internal_tmp_table,
                        uint32_t select_field_count,
                        bool is_if_not_exists)
{
  if (identifier.isTmp())
  {
    return create_table_no_lock(session,
                                      identifier,
                                      create_info,
                                      table_proto,
                                      alter_info,
                                      internal_tmp_table,
                                      select_field_count,
                                      is_if_not_exists);
  }

  return drizzle_create_table(session,
                              identifier,
                              create_info,
                              table_proto,
                              alter_info,
                              internal_tmp_table,
                              select_field_count,
                              is_if_not_exists);
}


/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KeyInfo *start, KeyInfo *end)
{
  for (KeyInfo *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(const char *field_name,KeyInfo *start,KeyInfo *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      !is_primary_key_name(field_name))
    return (char*) field_name;			// Use fieldname

  buff_end= strncpy(buff, field_name, sizeof(buff)-4);
  buff_end+= strlen(buff);

  /*
    Only 3 chars + '\0' left, so need to limit to 2 digit
    This is ok as we can't have more than 100 keys anyway
  */
  for (uint32_t i=2 ; i< 100; i++)
  {
    *buff_end= '_';
    internal::int10_to_str(i, buff_end+1, 10);
    if (!check_if_keyname_exists(buff,start,end))
      return memory::sql_strdup(buff);
  }
  return (char*) "not_specified";		// Should never happen
}


/****************************************************************************
** Alter a table definition
****************************************************************************/

/*
  Rename a table.

  SYNOPSIS
    rename_table()
      session
      base                      The plugin::StorageEngine handle.
      old_db                    The old database name.
      old_name                  The old table name.
      new_db                    The new database name.
      new_name                  The new table name.

  RETURN
    false   OK
    true    Error
*/

bool
rename_table(Session &session,
                   plugin::StorageEngine *base,
                   const identifier::Table &from,
                   const identifier::Table &to)
{
  if (not plugin::StorageEngine::doesSchemaExist(to))
  {
    my_error(ER_NO_DB_ERROR, MYF(0), to.getSchemaName().c_str());
    return true;
  }

  int error= base->renameTable(session, from, to);
  if (error == HA_ERR_WRONG_COMMAND)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER Table");
  else if (error)
  {
    my_error(ER_ERROR_ON_RENAME, MYF(0), 
			from.isTmp() ? "#sql-temporary" : from.getSQLPath().c_str(), 
			to.isTmp() ? "#sql-temporary" : to.getSQLPath().c_str(), error);
  }
  return error; 
}


/*
  Force all other threads to stop using the table

  SYNOPSIS
    wait_while_table_is_used()
    session			Thread Cursor
    table		Table to remove from cache
    function            HA_EXTRA_PREPARE_FOR_DROP if table is to be deleted
                        HA_EXTRA_FORCE_REOPEN if table is not be used
                        HA_EXTRA_PREPARE_FOR_RENAME if table is to be renamed
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on table::Cache::mutex()
    Win32 clients must also have a WRITE LOCK on the table !
*/

void wait_while_table_is_used(Session *session, Table *table,
                              enum ha_extra_function function)
{

  safe_mutex_assert_owner(table::Cache::mutex().native_handle());

  table->cursor->extra(function);
  /* Mark all tables that are in use as 'old' */
  session->abortLock(table);	/* end threads waiting on lock */

  /* Wait until all there are no other threads that has this table open */
  identifier::Table identifier(table->getShare()->getSchemaName(), table->getShare()->getTableName());
  table::Cache::removeTable(*session, identifier, RTFC_WAIT_OTHER_THREAD_FLAG);
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    session			Thread Cursor
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on table::Cache::mutex()
    Win32 clients must also have a WRITE LOCK on the table !
*/

void Session::close_cached_table(Table *table)
{

  wait_while_table_is_used(this, table, HA_EXTRA_FORCE_REOPEN);
  /* Close lock if this is not got with LOCK TABLES */
  if (open_tables.lock)
  {
    unlockTables(open_tables.lock);
    open_tables.lock= NULL;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  unlink_open_table(table);

  /* When lock on table::Cache::mutex() is freed other threads can continue */
  locking::broadcast_refresh();
}

/*
  RETURN VALUES
    false Message sent to net (admin operation went ok)
    true  Message should be sent by caller
          (admin operation or network communication failed)
*/
static bool admin_table(Session* session, TableList* tables,
                              const char *operator_name,
                              thr_lock_type lock_type,
                              bool open_for_modify,
                              int (Cursor::*operator_func)(Session*))
{
  TableList *table;
  Select_Lex *select= &session->lex().select_lex;
  List<Item> field_list;
  Item *item;
  int result_code= 0;
  const charset_info_st * const cs= system_charset_info;

  if (! session->endActiveTransaction())
    return 1;

  field_list.push_back(item = new Item_empty_string("Table", NAME_CHAR_LEN * 2, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Op", 10, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_type", 10, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_text", 255, cs));
  item->maybe_null = 1;
  session->getClient()->sendFields(field_list);

  for (table= tables; table; table= table->next_local)
  {
    identifier::Table table_identifier(table->getSchemaName(), table->getTableName());
    bool fatal_error=0;

    std::string table_name = table_identifier.getSQLPath();

    table->lock_type= lock_type;
    /* open only one table from local list of command */
    {
      TableList *save_next_global, *save_next_local;
      save_next_global= table->next_global;
      table->next_global= 0;
      save_next_local= table->next_local;
      table->next_local= 0;
      select->table_list.first= (unsigned char*)table;
      /*
        Time zone tables and SP tables can be add to lex->query_tables list,
        so it have to be prepared.
        @todo Investigate if we can put extra tables into argument instead of using lex->query_tables
      */
      session->lex().query_tables= table;
      session->lex().query_tables_last= &table->next_global;
      session->lex().query_tables_own_last= 0;
      session->no_warnings_for_error= 0;

      session->openTablesLock(table);
      session->no_warnings_for_error= 0;
      table->next_global= save_next_global;
      table->next_local= save_next_local;
    }

    /*
      CHECK Table command is only command where VIEW allowed here and this
      command use only temporary teble method for VIEWs resolving => there
      can't be VIEW tree substitition of join view => if opening table
      succeed then table->table will have real Table pointer as value (in
      case of join view substitution table->table can be 0, but here it is
      impossible)
    */
    if (!table->table)
    {
      if (!session->main_da().m_warn_list.size())
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                     ER_CHECK_NO_SUCH_TABLE, ER(ER_CHECK_NO_SUCH_TABLE));
      result_code= HA_ADMIN_CORRUPT;
      goto send_result;
    }

    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      char buff[FN_REFLEN + DRIZZLE_ERRMSG_SIZE];
      uint32_t length;
      session->getClient()->store(table_name.c_str());
      session->getClient()->store(operator_name);
      session->getClient()->store(STRING_WITH_LEN("error"));
      length= snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                       table_name.c_str());
      session->getClient()->store(buff, length);
      TransactionServices::autocommitOrRollback(*session, false);
      session->endTransaction(COMMIT);
      session->close_thread_tables();
      session->lex().reset_query_tables_list(false);
      table->table=0;				// For query cache
      if (session->getClient()->flush())
	goto err;
      continue;
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->getShare()->getVersion())
    {
      table::Cache::mutex().lock(); /* Lock type is TL_WRITE and we lock to repair the table */
      const char *old_message=session->enter_cond(COND_refresh, table::Cache::mutex(),
                                                  "Waiting to get writelock");
      session->abortLock(table->table);
      identifier::Table identifier(table->table->getShare()->getSchemaName(), table->table->getShare()->getTableName());
      table::Cache::removeTable(*session, identifier, RTFC_WAIT_OTHER_THREAD_FLAG | RTFC_CHECK_KILLED_FLAG);
      session->exit_cond(old_message);
      if (session->getKilled())
	goto err;
      open_for_modify= 0;
    }

    result_code = (table->table->cursor->*operator_func)(session);

send_result:

    session->lex().cleanup_after_one_table_open();
    session->clear_error();  // these errors shouldn't get client
    {
      List<DRIZZLE_ERROR>::iterator it(session->main_da().m_warn_list.begin());
      DRIZZLE_ERROR *err;
      while ((err= it++))
      {
        session->getClient()->store(table_name.c_str());
        session->getClient()->store(operator_name);
        session->getClient()->store(warning_level_names[err->level].str,
                               warning_level_names[err->level].length);
        session->getClient()->store(err->msg);
        if (session->getClient()->flush())
          goto err;
      }
      drizzle_reset_errors(session, true);
    }
    session->getClient()->store(table_name.c_str());
    session->getClient()->store(operator_name);

    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint32_t length=snprintf(buf, ERRMSGSIZE,
                             ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	session->getClient()->store(STRING_WITH_LEN("note"));
	session->getClient()->store(buf, length);
      }
      break;

    case HA_ADMIN_OK:
      session->getClient()->store(STRING_WITH_LEN("status"));
      session->getClient()->store(STRING_WITH_LEN("OK"));
      break;

    case HA_ADMIN_FAILED:
      session->getClient()->store(STRING_WITH_LEN("status"));
      session->getClient()->store(STRING_WITH_LEN("Operation failed"));
      break;

    case HA_ADMIN_REJECT:
      session->getClient()->store(STRING_WITH_LEN("status"));
      session->getClient()->store(STRING_WITH_LEN("Operation need committed state"));
      open_for_modify= false;
      break;

    case HA_ADMIN_ALREADY_DONE:
      session->getClient()->store(STRING_WITH_LEN("status"));
      session->getClient()->store(STRING_WITH_LEN("Table is already up to date"));
      break;

    case HA_ADMIN_CORRUPT:
      session->getClient()->store(STRING_WITH_LEN("error"));
      session->getClient()->store(STRING_WITH_LEN("Corrupt"));
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      session->getClient()->store(STRING_WITH_LEN("error"));
      session->getClient()->store(STRING_WITH_LEN("Invalid argument"));
      break;

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[ERRMSGSIZE+20];
        uint32_t length=snprintf(buf, ERRMSGSIZE,
                             _("Unknown - internal error %d during operation"),
                             result_code);
        session->getClient()->store(STRING_WITH_LEN("error"));
        session->getClient()->store(buf, length);
        fatal_error=1;
        break;
      }
    }
    if (table->table)
    {
      if (fatal_error)
      {
        table->table->getMutableShare()->resetVersion();               // Force close of table
      }
      else if (open_for_modify)
      {
        if (table->table->getShare()->getType())
        {
          table->table->cursor->info(HA_STATUS_CONST);
        }
        else
        {
          boost::unique_lock<boost::mutex> lock(table::Cache::mutex());
          identifier::Table identifier(table->table->getShare()->getSchemaName(), table->table->getShare()->getTableName());
          table::Cache::removeTable(*session, identifier, RTFC_NO_FLAG);
        }
      }
    }
    TransactionServices::autocommitOrRollback(*session, false);
    session->endTransaction(COMMIT);
    session->close_thread_tables();
    table->table=0;				// For query cache
    if (session->getClient()->flush())
      goto err;
  }

  session->my_eof();
  return false;

err:
  TransactionServices::autocommitOrRollback(*session, true);
  session->endTransaction(ROLLBACK);
  session->close_thread_tables();			// Shouldn't be needed
  if (table)
    table->table=0;
  return true;
}

  /*
    Create a new table by copying from source table

    Altough exclusive name-lock on target table protects us from concurrent
    DML and DDL operations on it we still want to wrap .FRM creation and call
    to plugin::StorageEngine::createTable() in critical section protected by
    table::Cache::mutex() in order to provide minimal atomicity against operations which
    disregard name-locks, like I_S implementation, for example. This is a
    temporary and should not be copied. Instead we should fix our code to
    always honor name-locks.

    Also some engines (e.g. NDB cluster) require that table::Cache::mutex() should be held
    during the call to plugin::StorageEngine::createTable().
    See bug #28614 for more info.
  */
static bool create_table_wrapper(Session &session,
                                 const message::Table& create_table_proto,
                                 const identifier::Table& destination_identifier,
                                 const identifier::Table& source_identifier,
                                 bool is_engine_set)
{
  // We require an additional table message because during parsing we used
  // a "new" message and it will not have all of the information that the
  // source table message would have.
  message::Table new_table_message;

  message::table::shared_ptr source_table_message= plugin::StorageEngine::getTableMessage(session, source_identifier);

  if (not source_table_message)
  {
    my_error(ER_TABLE_UNKNOWN, source_identifier);
    return false;
  }

  new_table_message.CopyFrom(*source_table_message);

  if (destination_identifier.isTmp())
  {
    new_table_message.set_type(message::Table::TEMPORARY);
  }
  else
  {
    new_table_message.set_type(message::Table::STANDARD);
  }

  if (is_engine_set)
  {
    new_table_message.mutable_engine()->set_name(create_table_proto.engine().name());
  }

  { // We now do a selective copy of elements on to the new table.
    new_table_message.set_name(create_table_proto.name());
    new_table_message.set_schema(create_table_proto.schema());
    new_table_message.set_catalog(create_table_proto.catalog());
  }

  /* Fix names of foreign keys being added */
  for (int32_t j= 0; j < new_table_message.fk_constraint_size(); j++)
  {
    if (new_table_message.fk_constraint(j).has_name())
    {
      std::string name(new_table_message.name());
      char number[20];

      name.append("_ibfk_");
      snprintf(number, sizeof(number), "%d", j+1);
      name.append(number);

      message::Table::ForeignKeyConstraint *pfkey= new_table_message.mutable_fk_constraint(j);
      pfkey->set_name(name);
    }
  }

  /*
    As mysql_truncate don't work on a new table at this stage of
    creation, instead create the table directly (for both normal and temporary tables).
  */
  bool success= plugin::StorageEngine::createTable(session,
                                                   destination_identifier,
                                                   new_table_message);

  if (success && not destination_identifier.isTmp())
  {
    TransactionServices::createTable(session, new_table_message);
  }

  return success;
}

/*
  Create a table identical to the specified table

  SYNOPSIS
    create_like_table()
    session		Thread object
    table       Table list element for target table
    src_table   Table list element for source table
    create_info Create info

  RETURN VALUES
    false OK
    true  error
*/

bool create_like_table(Session* session,
                       const identifier::Table& destination_identifier,
                       const identifier::Table& source_identifier,
                       message::Table &create_table_proto,
                       bool is_if_not_exists,
                       bool is_engine_set)
{
  bool res= true;
  bool table_exists= false;

  /*
    Check that destination tables does not exist. Note that its name
    was already checked when it was added to the table list.

    For temporary tables we don't aim to grab locks.
  */
  if (destination_identifier.isTmp())
  {
    if (session->open_tables.find_temporary_table(destination_identifier))
    {
      table_exists= true;
    }
    else
    {
      bool was_created= create_table_wrapper(*session,
                                             create_table_proto,
                                             destination_identifier,
                                             source_identifier,
                                             is_engine_set);
      if (not was_created) // This is pretty paranoid, but we assume something might not clean up after itself
      {
        (void) session->open_tables.rm_temporary_table(destination_identifier, true);
      }
      else if (not session->open_temporary_table(destination_identifier))
      {
        // We created, but we can't open... also, a hack.
        (void) session->open_tables.rm_temporary_table(destination_identifier, true);
      }
      else
      {
        res= false;
      }
    }
  }
  else // Standard table which will require locks.
  {
    Table *name_lock= session->lock_table_name_if_not_cached(destination_identifier);
    if (not name_lock)
    {
      table_exists= true;
    }
    else if (plugin::StorageEngine::doesTableExist(*session, destination_identifier))
    {
      table_exists= true;
    }
    else // Otherwise we create the table
    {
      bool was_created;
      {
        boost::mutex::scoped_lock lock(table::Cache::mutex()); /* We lock for CREATE TABLE LIKE to copy table definition */
        was_created= create_table_wrapper(*session, create_table_proto, destination_identifier,
                                          source_identifier, is_engine_set);
      }

      // So we blew the creation of the table, and we scramble to clean up
      // anything that might have been created (read... it is a hack)
      if (not was_created)
      {
        plugin::StorageEngine::dropTable(*session, destination_identifier);
      } 
      else
      {
        res= false;
      }
    }

    if (name_lock)
    {
      boost::mutex::scoped_lock lock(table::Cache::mutex()); /* unlink open tables for create table like*/
      session->unlink_open_table(name_lock);
    }
  }

  if (table_exists)
  {
    if (is_if_not_exists)
    {
      char warn_buff[DRIZZLE_ERRMSG_SIZE];
      snprintf(warn_buff, sizeof(warn_buff),
               ER(ER_TABLE_EXISTS_ERROR), destination_identifier.getTableName().c_str());
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                   ER_TABLE_EXISTS_ERROR, warn_buff);
      return false;
    }

    my_error(ER_TABLE_EXISTS_ERROR, destination_identifier);

    return true;
  }

  return res;
}


bool analyze_table(Session* session, TableList* tables)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  return(admin_table(session, tables, "analyze", lock_type, true, &Cursor::ha_analyze));
}


bool check_table(Session* session, TableList* tables)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;
  return admin_table(session, tables, "check", lock_type, false, &Cursor::ha_check);
}

} /* namespace drizzled */
