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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Function with list databases, tables or fields */

#include "mysql_priv.h"
#include "sql_select.h"                         // For select_describe
#include "sql_show.h"
#include "repl_failsafe.h"
#include <my_dir.h>

#define STR_OR_NIL(S) ((S) ? (S) : "<nil>")

/* Match the values of enum ha_choice */
static const char *ha_choice_values[] = {"", "0", "1"};

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info);



int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr)
{
  register int flag;
  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
        wildstr++;
      if (my_toupper(cs, *wildstr++) != my_toupper(cs, *str++))
        return(1);
    }
    if (! *wildstr )
      return (*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str++)
        return (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (!*wildstr)
        return(0);		/* '*' as last char: OK */
      flag=(*wildstr != wild_many && *wildstr != wild_one);
      do
      {
	if (flag)
	{
	  char cmp;
	  if ((cmp= *wildstr) == wild_prefix && wildstr[1])
	    cmp=wildstr[1];
	  cmp=my_toupper(cs, cmp);
	  while (*str && my_toupper(cs, *str) != cmp)
	    str++;
    if (!*str)
      return (1);
	}
  if (wild_case_compare(cs, str,wildstr) == 0)
      return (0);
      } while (*str++);
      return(1);
    }
  }
  return (*str != '\0');
}

/***************************************************************************
** List all table types supported
***************************************************************************/

static int make_version_string(char *buf, int buf_length, uint version)
{
  return snprintf(buf, buf_length, "%d.%d", version>>8,version&0xff);
}

static my_bool show_plugins(THD *thd, plugin_ref plugin,
                            void *arg)
{
  TABLE *table= (TABLE*) arg;
  struct st_mysql_plugin *plug= plugin_decl(plugin);
  struct st_plugin_dl *plugin_dl= plugin_dlib(plugin);
  CHARSET_INFO *cs= system_charset_info;
  char version_buf[20];

  restore_record(table, s->default_values);

  table->field[0]->store(plugin_name(plugin)->str,
                         plugin_name(plugin)->length, cs);

  table->field[1]->store(version_buf,
        make_version_string(version_buf, sizeof(version_buf), plug->version),
        cs);


  switch (plugin_state(plugin)) {
  /* case PLUGIN_IS_FREED: does not happen */
  case PLUGIN_IS_DELETED:
    table->field[2]->store(STRING_WITH_LEN("DELETED"), cs);
    break;
  case PLUGIN_IS_UNINITIALIZED:
    table->field[2]->store(STRING_WITH_LEN("INACTIVE"), cs);
    break;
  case PLUGIN_IS_READY:
    table->field[2]->store(STRING_WITH_LEN("ACTIVE"), cs);
    break;
  default:
    DBUG_ASSERT(0);
  }

  table->field[3]->store(plugin_type_names[plug->type].str,
                         plugin_type_names[plug->type].length,
                         cs);
  table->field[4]->store(version_buf,
        make_version_string(version_buf, sizeof(version_buf),
                            *(uint *)plug->info), cs);

  if (plugin_dl)
  {
    table->field[5]->store(plugin_dl->dl.str, plugin_dl->dl.length, cs);
    table->field[5]->set_notnull();
    table->field[6]->store(version_buf,
          make_version_string(version_buf, sizeof(version_buf),
                              plugin_dl->version),
          cs);
    table->field[6]->set_notnull();
  }
  else
  {
    table->field[5]->set_null();
    table->field[6]->set_null();
  }


  if (plug->author)
  {
    table->field[7]->store(plug->author, strlen(plug->author), cs);
    table->field[7]->set_notnull();
  }
  else
    table->field[7]->set_null();

  if (plug->descr)
  {
    table->field[8]->store(plug->descr, strlen(plug->descr), cs);
    table->field[8]->set_notnull();
  }
  else
    table->field[8]->set_null();

  switch (plug->license) {
  case PLUGIN_LICENSE_GPL:
    table->field[9]->store(PLUGIN_LICENSE_GPL_STRING, 
                           strlen(PLUGIN_LICENSE_GPL_STRING), cs);
    break;
  case PLUGIN_LICENSE_BSD:
    table->field[9]->store(PLUGIN_LICENSE_BSD_STRING, 
                           strlen(PLUGIN_LICENSE_BSD_STRING), cs);
    break;
  default:
    table->field[9]->store(PLUGIN_LICENSE_PROPRIETARY_STRING, 
                           strlen(PLUGIN_LICENSE_PROPRIETARY_STRING), cs);
    break;
  }
  table->field[9]->set_notnull();

  return schema_table_store_record(thd, table);
}


int fill_plugins(THD *thd, TABLE_LIST *tables, COND *cond)
{
  TABLE *table= tables->table;

  if (plugin_foreach_with_mask(thd, show_plugins, MYSQL_ANY_PLUGIN,
                               ~PLUGIN_IS_FREED, table))
    return(1);

  return(0);
}


/*
  find_files() - find files in a given directory.

  SYNOPSIS
    find_files()
    thd                 thread handler
    files               put found files in this list
    db                  database name to set in TABLE_LIST structure
    path                path to database
    wild                filter for found files
    dir                 read databases in path if TRUE, read .frm files in
                        database otherwise

  RETURN
    FIND_FILES_OK       success
    FIND_FILES_OOM      out of memory error
    FIND_FILES_DIR      no such directory, or directory can't be read
*/


find_files_result
find_files(THD *thd, List<LEX_STRING> *files, const char *db,
           const char *path, const char *wild, bool dir)
{
  uint i;
  char *ext;
  MY_DIR *dirp;
  FILEINFO *file;
  LEX_STRING *file_name= 0;
  uint file_name_len;
  TABLE_LIST table_list;

  if (wild && !wild[0])
    wild=0;

  bzero((char*) &table_list,sizeof(table_list));

  if (!(dirp = my_dir(path,MYF(dir ? MY_WANT_STAT : 0))))
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR, MYF(ME_BELL+ME_WAITTANG), db);
    else
      my_error(ER_CANT_READ_DIR, MYF(ME_BELL+ME_WAITTANG), path, my_errno);
    return(FIND_FILES_DIR);
  }

  for (i=0 ; i < (uint) dirp->number_off_files  ; i++)
  {
    char uname[NAME_LEN + 1];                   /* Unencoded name */
    file=dirp->dir_entry+i;
    if (dir)
    {                                           /* Return databases */
      if ((file->name[0] == '.' && 
          ((file->name[1] == '.' && file->name[2] == '\0') ||
            file->name[1] == '\0')))
        continue;                               /* . or .. */
#ifdef USE_SYMDIR
      char *ext;
      char buff[FN_REFLEN];
      if (my_use_symdir && !strcmp(ext=fn_ext(file->name), ".sym"))
      {
	/* Only show the sym file if it points to a directory */
	char *end;
        *ext=0;                                 /* Remove extension */
	unpack_dirname(buff, file->name);
	end= strend(buff);
	if (end != buff && end[-1] == FN_LIBCHAR)
	  end[-1]= 0;				// Remove end FN_LIBCHAR
        if (stat(buff, file->mystat))
               continue;
       }
#endif
      if (!MY_S_ISDIR(file->mystat->st_mode))
        continue;

      file_name_len= filename_to_tablename(file->name, uname, sizeof(uname));
      if (wild && wild_compare(uname, wild, 0))
        continue;
      if (!(file_name= 
            thd->make_lex_string(file_name, uname, file_name_len, TRUE)))
      {
        my_dirend(dirp);
        return(FIND_FILES_OOM);
      }
    }
    else
    {
        // Return only .frm files which aren't temp files.
      if (my_strcasecmp(system_charset_info, ext=fn_rext(file->name),reg_ext) ||
          is_prefix(file->name, tmp_file_prefix))
        continue;
      *ext=0;
      file_name_len= filename_to_tablename(file->name, uname, sizeof(uname));
      if (wild)
      {
	if (lower_case_table_names)
	{
	  if (wild_case_compare(files_charset_info, uname, wild))
	    continue;
	}
	else if (wild_compare(uname, wild, 0))
	  continue;
      }
    }
    if (!(file_name= 
          thd->make_lex_string(file_name, uname, file_name_len, TRUE)) ||
        files->push_back(file_name))
    {
      my_dirend(dirp);
      return(FIND_FILES_OOM);
    }
  }
  my_dirend(dirp);

  return(FIND_FILES_OK);
}


bool
mysqld_show_create(THD *thd, TABLE_LIST *table_list)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);

  /* Only one table for now, but VIEW can involve several tables */
  if (open_normal_and_derived_tables(thd, table_list, 0))
  {
    if (thd->is_error() && thd->main_da.sql_errno() != ER_VIEW_INVALID)
      return(TRUE);

    /*
      Clear all messages with 'error' level status and
      issue a warning with 'warning' level status in 
      case of invalid view and last error is ER_VIEW_INVALID
    */
    mysql_reset_errors(thd, true);
    thd->clear_error();
  }

  buffer.length(0);

  if (store_create_info(thd, table_list, &buffer, NULL))
    return(TRUE);

  List<Item> field_list;
  {
    field_list.push_back(new Item_empty_string("Table",NAME_CHAR_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string("Create Table",
                                               max(buffer.length(),1024)));
  }

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(TRUE);
  protocol->prepare_for_resend();
  {
    if (table_list->schema_table)
      protocol->store(table_list->schema_table->table_name,
                      system_charset_info);
    else
      protocol->store(table_list->table->alias, system_charset_info);
  }

  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->write())
    return(TRUE);

  my_eof(thd);
  return(FALSE);
}

bool mysqld_show_create_db(THD *thd, char *dbname,
                           HA_CREATE_INFO *create_info)
{
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  Protocol *protocol=thd->protocol;

  if (store_db_create_info(thd, dbname, &buffer, create_info))
  {
    /* 
      This assumes that the only reason for which store_db_create_info()
      can fail is incorrect database name (which is the case now).
    */
    my_error(ER_BAD_DB_ERROR, MYF(0), dbname);
    return(TRUE);    
  }

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database",NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database",1024));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(TRUE);

  protocol->prepare_for_resend();
  protocol->store(dbname, strlen(dbname), system_charset_info);
  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->write())
    return(TRUE);
  my_eof(thd);
  return(FALSE);
}



/****************************************************************************
  Return only fields for API mysql_list_fields
  Use "show table wildcard" in mysql instead of this
****************************************************************************/

void
mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild)
{
  TABLE *table;

  if (open_normal_and_derived_tables(thd, table_list, 0))
    return;
  table= table_list->table;

  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
    {
      field_list.push_back(new Item_field(field));
    }
  }
  restore_record(table, s->default_values);              // Get empty record
  table->use_all_columns();
  if (thd->protocol->send_fields(&field_list, Protocol::SEND_DEFAULTS))
    return;
  my_eof(thd);
  return;
}


/*
  Go through all character combinations and ensure that sql_lex.cc can
  parse it as an identifier.

  SYNOPSIS
  require_quotes()
  name			attribute name
  name_length		length of name

  RETURN
    #	Pointer to conflicting character
    0	No conflicting character
*/

static const char *require_quotes(const char *name, uint name_length)
{
  uint length;
  bool pure_digit= TRUE;
  const char *end= name + name_length;

  for (; name < end ; name++)
  {
    uchar chr= (uchar) *name;
    length= my_mbcharlen(system_charset_info, chr);
    if (length == 1 && !system_charset_info->ident_map[chr])
      return name;
    if (length == 1 && (chr < '0' || chr > '9'))
      pure_digit= FALSE;
  }
  if (pure_digit)
    return name;
  return 0;
}


/*
  Quote the given identifier if needed and append it to the target string.
  If the given identifier is empty, it will be quoted.

  SYNOPSIS
  append_identifier()
  thd                   thread handler
  packet                target string
  name                  the identifier to be appended
  name_length           length of the appending identifier
*/

void
append_identifier(THD *thd, String *packet, const char *name, uint length)
{
  const char *name_end;
  char quote_char;
  int q= get_quote_char_for_identifier(thd, name, length);

  if (q == EOF)
  {
    packet->append(name, length, packet->charset());
    return;
  }

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  VOID(packet->reserve(length*2 + 2));
  quote_char= (char) q;
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end= name+length ; name < name_end ; name+= length)
  {
    uchar chr= (uchar) *name;
    length= my_mbcharlen(system_charset_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!length)
      length= 1;
    if (length == 1 && chr == (uchar) quote_char)
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(name, length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}


/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()
    thd		Thread handler
    name	name to quote
    length	length of name

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier(THD *thd, const char *name, uint length)
{
  if (length &&
      !is_keyword(name,length) &&
      !require_quotes(name, length) &&
      !(thd->options & OPTION_QUOTE_SHOW_CREATE))
    return EOF;
  return '"';
}


/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
			     const char *filename)
{
  if (filename)
  {
    uint length= dirname_length(filename);
    packet->append(' ');
    packet->append(dir_type);
    packet->append(STRING_WITH_LEN(" DIRECTORY='"));
    packet->append(filename, length);
    packet->append('\'');
  }
}


#define LIST_PROCESS_HOST_LEN 64

static bool get_field_default_value(THD *thd, Field *timestamp_field,
                                    Field *field, String *def_value,
                                    bool quoted)
{
  bool has_default;
  bool has_now_default;

  /* 
     We are using CURRENT_TIMESTAMP instead of NOW because it is
     more standard
  */
  has_now_default= (timestamp_field == field &&
                    field->unireg_check != Field::TIMESTAMP_UN_FIELD);
    
  has_default= (field->type() != FIELD_TYPE_BLOB &&
                !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
                field->unireg_check != Field::NEXT_NUMBER
                  && has_now_default);

  def_value->length(0);
  if (has_default)
  {
    if (has_now_default)
      def_value->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
    else if (!field->is_null())
    {                                             // Not null by default
      char tmp[MAX_FIELD_WIDTH];
      String type(tmp, sizeof(tmp), field->charset());
      field->val_str(&type);
      if (type.length())
      {
        String def_val;
        uint dummy_errors;
        /* convert to system_charset_info == utf8 */
        def_val.copy(type.ptr(), type.length(), field->charset(),
                     system_charset_info, &dummy_errors);
        if (quoted)
          append_unescaped(def_value, def_val.ptr(), def_val.length());
        else
          def_value->append(def_val.ptr(), def_val.length());
      }
      else if (quoted)
        def_value->append(STRING_WITH_LEN("''"));
    }
    else if (field->maybe_null() && quoted)
      def_value->append(STRING_WITH_LEN("NULL"));    // Null as default
    else
      return 0;

  }
  return has_default;
}

/*
  Build a CREATE TABLE statement for a table.

  SYNOPSIS
    store_create_info()
    thd               The thread
    table_list        A list containing one table to write statement
                      for.
    packet            Pointer to a string where statement will be
                      written.
    create_info_arg   Pointer to create information that can be used
                      to tailor the format of the statement.  Can be
                      NULL, in which case only SQL_MODE is considered
                      when building the statement.
  
  NOTE
    Currently always return 0, but might return error code in the
    future.
    
  RETURN
    0       OK
 */

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO *create_info_arg)
{
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, buff[128], def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr,*field;
  uint primary_key;
  KEY *key_info;
  TABLE *table= table_list->table;
  handler *file= table->file;
  TABLE_SHARE *share= table->s;
  HA_CREATE_INFO create_info;
  bool show_table_options= FALSE;
  my_bitmap_map *old_map;

  restore_record(table, s->default_values); // Get empty record

  if (share->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (create_info_arg &&
      (create_info_arg->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (table_list->schema_table)
    alias= table_list->schema_table->table_name;
  else
  {
    if (lower_case_table_names == 2)
      alias= table->alias;
    else
    {
      alias= share->table_name.str;
    }
  }
  append_identifier(thd, packet, alias, strlen(alias));
  packet->append(STRING_WITH_LEN(" (\n"));
  /*
    We need this to get default values from the table
    We have to restore the read_set if we are called from insert in case
    of row based replication.
  */
  old_map= tmp_use_all_columns(table, table->read_set);

  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    uint flags = field->flags;

    if (ptr != table->field)
      packet->append(STRING_WITH_LEN(",\n"));

    packet->append(STRING_WITH_LEN("  "));
    append_identifier(thd,packet,field->field_name, strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    packet->append(type.ptr(), type.length(), system_charset_info);

    if (field->has_charset())
    {
      if (field->charset() != share->table_charset)
      {
	packet->append(STRING_WITH_LEN(" CHARACTER SET "));
	packet->append(field->charset()->csname);
      }
      /* 
	For string types dump collation name only if 
	collation is not primary for the given charset
      */
      if (!(field->charset()->state & MY_CS_PRIMARY))
      {
	packet->append(STRING_WITH_LEN(" COLLATE "));
	packet->append(field->charset()->name);
      }
    }

    if (flags & NOT_NULL_FLAG)
      packet->append(STRING_WITH_LEN(" NOT NULL"));
    else if (field->type() == MYSQL_TYPE_TIMESTAMP)
    {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(STRING_WITH_LEN(" NULL"));
    }
    {
      /*
        Add field flags about FIELD FORMAT (FIXED or DYNAMIC)
        and about STORAGE (DISK or MEMORY).
      */
      enum ha_storage_media storage_type= (enum ha_storage_media)
        ((flags >> FIELD_STORAGE_FLAGS) & STORAGE_TYPE_MASK);
      enum column_format_type column_format= (enum column_format_type)
        ((flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
      if (storage_type)
      {
        packet->append(STRING_WITH_LEN(" /*!"));
        packet->append(STRING_WITH_LEN(MYSQL_VERSION_TABLESPACE_IN_FRM_STR));
        packet->append(STRING_WITH_LEN(" STORAGE"));
        if (storage_type == HA_SM_DISK)
          packet->append(STRING_WITH_LEN(" DISK */"));
        else
          packet->append(STRING_WITH_LEN(" MEMORY */"));
      }
      if (column_format)
      {
        packet->append(STRING_WITH_LEN(" /*!"));
        packet->append(STRING_WITH_LEN(MYSQL_VERSION_TABLESPACE_IN_FRM_STR));
        packet->append(STRING_WITH_LEN(" COLUMN_FORMAT"));
        if (column_format == COLUMN_FORMAT_TYPE_FIXED)
          packet->append(STRING_WITH_LEN(" FIXED */"));
        else
          packet->append(STRING_WITH_LEN(" DYNAMIC */"));
      }
    }
    if (get_field_default_value(thd, table->timestamp_field,
                                field, &def_value, 1))
    {
      packet->append(STRING_WITH_LEN(" DEFAULT "));
      packet->append(def_value.ptr(), def_value.length(), system_charset_info);
    }

    if (table->timestamp_field == field && field->unireg_check != Field::TIMESTAMP_DN_FIELD)
      packet->append(STRING_WITH_LEN(" ON UPDATE CURRENT_TIMESTAMP"));

    if (field->unireg_check == Field::NEXT_NUMBER)
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT"));

    if (field->comment.length)
    {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, field->comment.str, field->comment.length);
    }
  }

  key_info= table->key_info;
  bzero((char*) &create_info, sizeof(create_info));
  /* Allow update_create_info to update row type */
  create_info.row_type= share->row_type;
  file->update_create_info(&create_info);
  primary_key= share->primary_key;

  for (uint i=0 ; i < share->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    bool found_primary=0;
    packet->append(STRING_WITH_LEN(",\n  "));

    if (i == primary_key && !strcmp(key_info->name, primary_key_name))
    {
      found_primary=1;
      /*
        No space at end, because a space will be added after where the
        identifier would go, but that is not added for primary key.
      */
      packet->append(STRING_WITH_LEN("PRIMARY KEY"));
    }
    else if (key_info->flags & HA_NOSAME)
      packet->append(STRING_WITH_LEN("UNIQUE KEY "));
    else
      packet->append(STRING_WITH_LEN("KEY "));

    if (!found_primary)
     append_identifier(thd, packet, key_info->name, strlen(key_info->name));

    packet->append(STRING_WITH_LEN(" ("));

    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (j)
        packet->append(',');

      if (key_part->field)
        append_identifier(thd,packet,key_part->field->field_name,
			  strlen(key_part->field->field_name));
      if (key_part->field &&
          (key_part->length !=
           table->field[key_part->fieldnr-1]->key_length()))
      {
        char *end;
        buff[0] = '(';
        end= int10_to_str((long) key_part->length /
                          key_part->field->charset()->mbmaxlen,
                          buff + 1,10);
        *end++ = ')';
        packet->append(buff,(uint) (end-buff));
      }
    }
    packet->append(')');
    store_key_options(thd, packet, table, key_info);
  }

  /*
    Get possible foreign key definitions stored in InnoDB and append them
    to the CREATE TABLE statement
  */

  if ((for_str= file->get_foreign_key_create_info()))
  {
    packet->append(for_str, strlen(for_str));
    file->free_foreign_key_create_info(for_str);
  }

  packet->append(STRING_WITH_LEN("\n)"));
  {
    show_table_options= TRUE;
    /*
      Get possible table space definitions and append them
      to the CREATE TABLE statement
    */

    switch (table->s->default_storage_media) {
    case(HA_SM_DEFAULT):
      if ((for_str= (char *)file->get_tablespace_name()))
      {
        packet->append(STRING_WITH_LEN(" /*!50100 TABLESPACE "));
        append_identifier(thd, packet, for_str, strlen(for_str));
        packet->append(STRING_WITH_LEN(" */"));
      }
      break;
    case(HA_SM_DISK):
      packet->append(STRING_WITH_LEN(" /*!50100"));
      if ((for_str= (char *)file->get_tablespace_name()))
      {
        packet->append(STRING_WITH_LEN(" TABLESPACE "));
        append_identifier(thd, packet, for_str, strlen(for_str));
      }
      packet->append(STRING_WITH_LEN(" STORAGE DISK */"));
      break;
    case(HA_SM_MEMORY):
      packet->append(STRING_WITH_LEN(" /*!50100"));
      if ((for_str= (char *)file->get_tablespace_name()))
      {
        packet->append(STRING_WITH_LEN(" TABLESPACE "));
        append_identifier(thd, packet, for_str, strlen(for_str));
      }
      packet->append(STRING_WITH_LEN(" STORAGE MEMORY */"));
      break;
    };

    /*
      IF   check_create_info
      THEN add ENGINE only if it was used when creating the table
    */
    if (!create_info_arg ||
        (create_info_arg->used_fields & HA_CREATE_USED_ENGINE))
    {
      packet->append(STRING_WITH_LEN(" ENGINE="));
      packet->append(file->table_type());
    }

    /*
      Add AUTO_INCREMENT=... if there is an AUTO_INCREMENT column,
      and NEXT_ID > 1 (the default).  We must not print the clause
      for engines that do not support this as it would break the
      import of dumps, but as of this writing, the test for whether
      AUTO_INCREMENT columns are allowed and wether AUTO_INCREMENT=...
      is supported is identical, !(file->table_flags() & HA_NO_AUTO_INCREMENT))
      Because of that, we do not explicitly test for the feature,
      but may extrapolate its existence from that of an AUTO_INCREMENT column.
    */

    if (create_info.auto_increment_value > 1)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT="));
      end= longlong10_to_str(create_info.auto_increment_value, buff,10);
      packet->append(buff, (uint) (end - buff));
    }

    
    if (share->table_charset)
    {
      /*
        IF   check_create_info
        THEN add DEFAULT CHARSET only if it was used when creating the table
      */
      if (!create_info_arg ||
          (create_info_arg->used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
      {
        packet->append(STRING_WITH_LEN(" DEFAULT CHARSET="));
        packet->append(share->table_charset->csname);
        if (!(share->table_charset->state & MY_CS_PRIMARY))
        {
          packet->append(STRING_WITH_LEN(" COLLATE="));
          packet->append(table->s->table_charset->name);
        }
      }
    }

    if (share->min_rows)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" MIN_ROWS="));
      end= longlong10_to_str(share->min_rows, buff, 10);
      packet->append(buff, (uint) (end- buff));
    }

    if (share->max_rows && !table_list->schema_table)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" MAX_ROWS="));
      end= longlong10_to_str(share->max_rows, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }

    if (share->avg_row_length)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" AVG_ROW_LENGTH="));
      end= longlong10_to_str(share->avg_row_length, buff,10);
      packet->append(buff, (uint) (end - buff));
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=1"));
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=0"));
    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compability */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      packet->append(STRING_WITH_LEN(" CHECKSUM=1"));
    if (share->page_checksum != HA_CHOICE_UNDEF)
    {
      packet->append(STRING_WITH_LEN(" PAGE_CHECKSUM="));
      packet->append(ha_choice_values[(uint) share->page_checksum], 1);
    }
    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      packet->append(STRING_WITH_LEN(" DELAY_KEY_WRITE=1"));
    if (create_info.row_type != ROW_TYPE_DEFAULT)
    {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint) create_info.row_type]);
    }
    if (share->transactional != HA_CHOICE_UNDEF)
    {
      packet->append(STRING_WITH_LEN(" TRANSACTIONAL="));
      packet->append(ha_choice_values[(uint) share->transactional], 1);
    }
    if (table->s->key_block_size)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end= longlong10_to_str(table->s->key_block_size, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }
    table->file->append_create_info(packet);
    if (share->comment.length)
    {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, share->comment.str, share->comment.length);
    }
    if (share->connect_string.length)
    {
      packet->append(STRING_WITH_LEN(" CONNECTION="));
      append_unescaped(packet, share->connect_string.str, share->connect_string.length);
    }
    append_directory(thd, packet, "DATA",  create_info.data_file_name);
    append_directory(thd, packet, "INDEX", create_info.index_file_name);
  }
  tmp_restore_column_map(table->read_set, old_map);
  return(0);
}

/**
  Get a CREATE statement for a given database.

  The database is identified by its name, passed as @c dbname parameter.
  The name should be encoded using the system character set (UTF8 currently).

  Resulting statement is stored in the string pointed by @c buffer. The string
  is emptied first and its character set is set to the system character set.

  If HA_LEX_CREATE_IF_NOT_EXISTS flag is set in @c create_info->options, then
  the resulting CREATE statement contains "IF NOT EXISTS" clause. Other flags
  in @c create_options are ignored.

  @param  thd           The current thread instance.
  @param  dbname        The name of the database.
  @param  buffer        A String instance where the statement is stored.
  @param  create_info   If not NULL, the options member influences the resulting 
                        CRATE statement.

  @returns TRUE if errors are detected, FALSE otherwise.
*/

bool store_db_create_info(THD *thd, const char *dbname, String *buffer,
                          HA_CREATE_INFO *create_info)
{
  HA_CREATE_INFO create;
  uint create_options = create_info ? create_info->options : 0;

  if (!my_strcasecmp(system_charset_info, dbname,
                     INFORMATION_SCHEMA_NAME.str))
  {
    dbname= INFORMATION_SCHEMA_NAME.str;
    create.default_table_charset= system_charset_info;
  }
  else
  {
    if (check_db_dir_existence(dbname))
      return(TRUE);

    load_db_opt_by_name(thd, dbname, &create);
  }

  buffer->length(0);
  buffer->free();
  buffer->set_charset(system_charset_info);
  buffer->append(STRING_WITH_LEN("CREATE DATABASE "));

  if (create_options & HA_LEX_CREATE_IF_NOT_EXISTS)
    buffer->append(STRING_WITH_LEN("/*!32312 IF NOT EXISTS*/ "));

  append_identifier(thd, buffer, dbname, strlen(dbname));

  if (create.default_table_charset)
  {
    buffer->append(STRING_WITH_LEN(" /*!40100"));
    buffer->append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    buffer->append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY))
    {
      buffer->append(STRING_WITH_LEN(" COLLATE "));
      buffer->append(create.default_table_charset->name);
    }
    buffer->append(STRING_WITH_LEN(" */"));
  }

  return(FALSE);
}

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info)
{
  char *end, buff[32];

  if (key_info->algorithm == HA_KEY_ALG_BTREE)
    packet->append(STRING_WITH_LEN(" USING BTREE"));

  if (key_info->algorithm == HA_KEY_ALG_HASH)
    packet->append(STRING_WITH_LEN(" USING HASH"));

  if ((key_info->flags & HA_USES_BLOCK_SIZE) &&
      table->s->key_block_size != key_info->block_size)
  {
    packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
    end= longlong10_to_str(key_info->block_size, buff, 10);
    packet->append(buff, (uint) (end - buff));
  }

  DBUG_ASSERT(test(key_info->flags & HA_USES_COMMENT) == 
              (key_info->comment.length > 0));
  if (key_info->flags & HA_USES_COMMENT)
  {
    packet->append(STRING_WITH_LEN(" COMMENT "));
    append_unescaped(packet, key_info->comment.str, 
                     key_info->comment.length);
  }
}


/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/

class thread_info :public ilink {
public:
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused)))
  { TRASH(ptr, size); }

  ulong thread_id;
  time_t start_time;
  uint   command;
  const char *user,*host,*db,*proc_info,*state_info;
  char *query;
};

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List<thread_info>;
#endif

void mysqld_list_processes(THD *thd,const char *user, bool verbose)
{
  Item *field;
  List<Item> field_list;
  I_List<thread_info> thread_infos;
  ulong max_query_length= (verbose ? thd->variables.max_allowed_packet :
			   PROCESS_LIST_WIDTH);
  Protocol *protocol= thd->protocol;

  field_list.push_back(new Item_int("Id", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("User",16));
  field_list.push_back(new Item_empty_string("Host",LIST_PROCESS_HOST_LEN));
  field_list.push_back(field=new Item_empty_string("db",NAME_CHAR_LEN));
  field->maybe_null=1;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(new Item_return_int("Time",7, MYSQL_TYPE_LONG));
  field_list.push_back(field=new Item_empty_string("State",30));
  field->maybe_null=1;
  field_list.push_back(field=new Item_empty_string("Info",max_query_length));
  field->maybe_null=1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return;

  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  if (!thd->killed)
  {
    I_List_iterator<THD> it(threads);
    THD *tmp;
    while ((tmp=it++))
    {
      Security_context *tmp_sctx= tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      if ((tmp->vio_ok() || tmp->system_thread) && (!user || (tmp_sctx->user && !strcmp(tmp_sctx->user, user))))
      {
        thread_info *thd_info= new thread_info;

        thd_info->thread_id=tmp->thread_id;
        thd_info->user= thd->strdup(tmp_sctx->user ? tmp_sctx->user :
                                    (tmp->system_thread ?
                                     "system user" : "unauthenticated user"));
        thd_info->host= thd->strdup(tmp_sctx->host_or_ip[0] ? 
                                    tmp_sctx->host_or_ip : 
                                    tmp_sctx->host ? tmp_sctx->host : "");
        if ((thd_info->db=tmp->db))             // Safe test
          thd_info->db=thd->strdup(thd_info->db);
        thd_info->command=(int) tmp->command;
        if ((mysys_var= tmp->mysys_var))
          pthread_mutex_lock(&mysys_var->mutex);
        thd_info->proc_info= (char*) (tmp->killed == THD::KILL_CONNECTION? "Killed" : 0);
        thd_info->state_info= (char*) (tmp->net.reading_or_writing ?
                                       (tmp->net.reading_or_writing == 2 ?
                                        "Writing to net" :
                                        thd_info->command == COM_SLEEP ? NullS :
                                        "Reading from net") :
                                       tmp->proc_info ? tmp->proc_info :
                                       tmp->mysys_var &&
                                       tmp->mysys_var->current_cond ?
                                       "Waiting on cond" : NullS);
        if (mysys_var)
          pthread_mutex_unlock(&mysys_var->mutex);

        thd_info->start_time= tmp->start_time;
        thd_info->query=0;
        if (tmp->query)
        {
	  /* 
            query_length is always set to 0 when we set query = NULL; see
	    the comment in sql_class.h why this prevents crashes in possible
            races with query_length
          */
          uint length= min(max_query_length, tmp->query_length);
          thd_info->query=(char*) thd->strmake(tmp->query,length);
        }
        thread_infos.append(thd_info);
      }
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thread_info *thd_info;
  time_t now= my_time(0);
  while ((thd_info=thread_infos.get()))
  {
    protocol->prepare_for_resend();
    protocol->store((ulonglong) thd_info->thread_id);
    protocol->store(thd_info->user, system_charset_info);
    protocol->store(thd_info->host, system_charset_info);
    protocol->store(thd_info->db, system_charset_info);
    if (thd_info->proc_info)
      protocol->store(thd_info->proc_info, system_charset_info);
    else
      protocol->store(command_name[thd_info->command].str, system_charset_info);
    if (thd_info->start_time)
      protocol->store((uint32) (now - thd_info->start_time));
    else
      protocol->store_null();
    protocol->store(thd_info->state_info, system_charset_info);
    protocol->store(thd_info->query, system_charset_info);
    if (protocol->write())
      break; /* purecov: inspected */
  }
  my_eof(thd);
  return;
}

int fill_schema_processlist(THD* thd, TABLE_LIST* tables, COND* cond)
{
  TABLE *table= tables->table;
  CHARSET_INFO *cs= system_charset_info;
  char *user;
  time_t now= my_time(0);

  user= NullS;

  VOID(pthread_mutex_lock(&LOCK_thread_count));

  if (!thd->killed)
  {
    I_List_iterator<THD> it(threads);
    THD* tmp;

    while ((tmp= it++))
    {
      Security_context *tmp_sctx= tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      const char *val;

      if ((!tmp->vio_ok() && !tmp->system_thread))
        continue;

      restore_record(table, s->default_values);
      /* ID */
      table->field[0]->store((longlong) tmp->thread_id, TRUE);
      /* USER */
      val= tmp_sctx->user ? tmp_sctx->user :
            (tmp->system_thread ? "system user" : "unauthenticated user");
      table->field[1]->store(val, strlen(val), cs);
      /* HOST */
      table->field[2]->store(tmp_sctx->host_or_ip,
                             strlen(tmp_sctx->host_or_ip), cs);
      /* DB */
      if (tmp->db)
      {
        table->field[3]->store(tmp->db, strlen(tmp->db), cs);
        table->field[3]->set_notnull();
      }

      if ((mysys_var= tmp->mysys_var))
        pthread_mutex_lock(&mysys_var->mutex);
      /* COMMAND */
      if ((val= (char *) (tmp->killed == THD::KILL_CONNECTION? "Killed" : 0)))
        table->field[4]->store(val, strlen(val), cs);
      else
        table->field[4]->store(command_name[tmp->command].str,
                               command_name[tmp->command].length, cs);
      /* MYSQL_TIME */
      table->field[5]->store((uint32)(tmp->start_time ?
                                      now - tmp->start_time : 0), TRUE);
      /* STATE */
      val= (char*) (tmp->net.reading_or_writing ?
                    (tmp->net.reading_or_writing == 2 ?
                     "Writing to net" :
                     tmp->command == COM_SLEEP ? NullS :
                     "Reading from net") :
                    tmp->proc_info ? tmp->proc_info :
                    tmp->mysys_var &&
                    tmp->mysys_var->current_cond ?
                    "Waiting on cond" : NullS);
      if (val)
      {
        table->field[6]->store(val, strlen(val), cs);
        table->field[6]->set_notnull();
      }

      if (mysys_var)
        pthread_mutex_unlock(&mysys_var->mutex);

      /* INFO */
      if (tmp->query)
      {
        table->field[7]->store(tmp->query,
                               min(PROCESS_LIST_INFO_WIDTH,
                                   tmp->query_length), cs);
        table->field[7]->set_notnull();
      }

      if (schema_table_store_record(thd, table))
      {
        VOID(pthread_mutex_unlock(&LOCK_thread_count));
        return(1);
      }
    }
  }

  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  return(0);
}

/*****************************************************************************
  Status functions
*****************************************************************************/

static DYNAMIC_ARRAY all_status_vars;
static bool status_vars_inited= 0;
static int show_var_cmp(const void *var1, const void *var2)
{
  return strcmp(((SHOW_VAR*)var1)->name, ((SHOW_VAR*)var2)->name);
}

/*
  deletes all the SHOW_UNDEF elements from the array and calls
  delete_dynamic() if it's completely empty.
*/
static void shrink_var_array(DYNAMIC_ARRAY *array)
{
  uint a,b;
  SHOW_VAR *all= dynamic_element(array, 0, SHOW_VAR *);

  for (a= b= 0; b < array->elements; b++)
    if (all[b].type != SHOW_UNDEF)
      all[a++]= all[b];
  if (a)
  {
    bzero(all+a, sizeof(SHOW_VAR)); // writing NULL-element to the end
    array->elements= a;
  }
  else // array is completely empty - delete it
    delete_dynamic(array);
}

/*
  Adds an array of SHOW_VAR entries to the output of SHOW STATUS

  SYNOPSIS
    add_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to add to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    The handling of all_status_vars[] is completely internal, it's allocated
    automatically when something is added to it, and deleted completely when
    the last entry is removed.

    As a special optimization, if add_status_vars() is called before
    init_status_vars(), it assumes "startup mode" - neither concurrent access
    to the array nor SHOW STATUS are possible (thus it skips locks and qsort)

    The last entry of the all_status_vars[] should always be {0,0,SHOW_UNDEF}
*/
int add_status_vars(SHOW_VAR *list)
{
  int res= 0;
  if (status_vars_inited)
    pthread_mutex_lock(&LOCK_status);
  if (!all_status_vars.buffer && // array is not allocated yet - do it now
      my_init_dynamic_array(&all_status_vars, sizeof(SHOW_VAR), 200, 20))
  {
    res= 1;
    goto err;
  }
  while (list->name)
    res|= insert_dynamic(&all_status_vars, (uchar*)list++);
  res|= insert_dynamic(&all_status_vars, (uchar*)list); // appending NULL-element
  all_status_vars.elements--; // but next insert_dynamic should overwite it
  if (status_vars_inited)
    sort_dynamic(&all_status_vars, show_var_cmp);
err:
  if (status_vars_inited)
    pthread_mutex_unlock(&LOCK_status);
  return res;
}

/*
  Make all_status_vars[] usable for SHOW STATUS

  NOTE
    See add_status_vars(). Before init_status_vars() call, add_status_vars()
    works in a special fast "startup" mode. Thus init_status_vars()
    should be called as late as possible but before enabling multi-threading.
*/
void init_status_vars()
{
  status_vars_inited=1;
  sort_dynamic(&all_status_vars, show_var_cmp);
}

void reset_status_vars()
{
  SHOW_VAR *ptr= (SHOW_VAR*) all_status_vars.buffer;
  SHOW_VAR *last= ptr + all_status_vars.elements;
  for (; ptr < last; ptr++)
  {
    /* Note that SHOW_LONG_NOFLUSH variables are not reset */
    if (ptr->type == SHOW_LONG)
      *(ulong*) ptr->value= 0;
  }  
}

/*
  catch-all cleanup function, cleans up everything no matter what

  DESCRIPTION
    This function is not strictly required if all add_to_status/
    remove_status_vars are properly paired, but it's a safety measure that
    deletes everything from the all_status_vars[] even if some
    remove_status_vars were forgotten
*/
void free_status_vars()
{
  delete_dynamic(&all_status_vars);
}

/*
  Removes an array of SHOW_VAR entries from the output of SHOW STATUS

  SYNOPSIS
    remove_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to remove to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    there's lots of room for optimizing this, especially in non-sorted mode,
    but nobody cares - it may be called only in case of failed plugin
    initialization in the mysqld startup.
*/

void remove_status_vars(SHOW_VAR *list)
{
  if (status_vars_inited)
  {
    pthread_mutex_lock(&LOCK_status);
    SHOW_VAR *all= dynamic_element(&all_status_vars, 0, SHOW_VAR *);
    int a= 0, b= all_status_vars.elements, c= (a+b)/2;

    for (; list->name; list++)
    {
      int res= 0;
      for (a= 0, b= all_status_vars.elements; b-a > 1; c= (a+b)/2)
      {
        res= show_var_cmp(list, all+c);
        if (res < 0)
          b= c;
        else if (res > 0)
          a= c;
        else
          break;
      }
      if (res == 0)
        all[c].type= SHOW_UNDEF;
    }
    shrink_var_array(&all_status_vars);
    pthread_mutex_unlock(&LOCK_status);
  }
  else
  {
    SHOW_VAR *all= dynamic_element(&all_status_vars, 0, SHOW_VAR *);
    uint i;
    for (; list->name; list++)
    {
      for (i= 0; i < all_status_vars.elements; i++)
      {
        if (show_var_cmp(list, all+i))
          continue;
        all[i].type= SHOW_UNDEF;
        break;
      }
    }
    shrink_var_array(&all_status_vars);
  }
}

inline void make_upper(char *buf)
{
  for (; *buf; buf++)
    *buf= my_toupper(system_charset_info, *buf);
}

static bool show_status_array(THD *thd, const char *wild,
                              SHOW_VAR *variables,
                              enum enum_var_type value_type,
                              struct system_status_var *status_var,
                              const char *prefix, TABLE *table,
                              bool ucase_names)
{
  MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, long);
  char * const buff= (char *) &buff_data;
  char *prefix_end;
  /* the variable name should not be longer than 64 characters */
  char name_buffer[64];
  int len;
  LEX_STRING null_lex_str;
  SHOW_VAR tmp, *var;

  null_lex_str.str= 0;				// For sys_var->value_ptr()
  null_lex_str.length= 0;

  prefix_end=strnmov(name_buffer, prefix, sizeof(name_buffer)-1);
  if (*prefix)
    *prefix_end++= '_';
  len=name_buffer + sizeof(name_buffer) - prefix_end;

  for (; variables->name; variables++)
  {
    strnmov(prefix_end, variables->name, len);
    name_buffer[sizeof(name_buffer)-1]=0;       /* Safety */
    if (ucase_names)
      make_upper(name_buffer);

    /*
      if var->type is SHOW_FUNC, call the function.
      Repeat as necessary, if new var is again SHOW_FUNC
    */
    for (var=variables; var->type == SHOW_FUNC; var= &tmp)
      ((mysql_show_var_func)(var->value))(thd, &tmp, buff);

    SHOW_TYPE show_type=var->type;
    if (show_type == SHOW_ARRAY)
    {
      show_status_array(thd, wild, (SHOW_VAR *) var->value, value_type,
                        status_var, name_buffer, table, ucase_names);
    }
    else
    {
      if (!(wild && wild[0] && wild_case_compare(system_charset_info,
                                                 name_buffer, wild)))
      {
        char *value=var->value;
        const char *pos, *end;                  // We assign a lot of const's

        pthread_mutex_lock(&LOCK_global_system_variables);

        if (show_type == SHOW_SYS)
        {
          show_type= ((sys_var*) value)->show_type();
          value=     (char*) ((sys_var*) value)->value_ptr(thd, value_type,
                                                           &null_lex_str);
        }

        pos= end= buff;
        /*
          note that value may be == buff. All SHOW_xxx code below
          should still work in this case
        */
        switch (show_type) {
        case SHOW_DOUBLE_STATUS:
          value= ((char *) status_var + (ulong) value);
          /* fall through */
        case SHOW_DOUBLE:
          /* 6 is the default precision for '%f' in sprintf() */
          end= buff + my_fcvt(*(double *) value, 6, buff, NULL);
          break;
        case SHOW_LONG_STATUS:
          value= ((char *) status_var + (ulong) value);
          /* fall through */
        case SHOW_LONG:
        case SHOW_LONG_NOFLUSH: // the difference lies in refresh_status()
          end= int10_to_str(*(long*) value, buff, 10);
          break;
        case SHOW_LONGLONG_STATUS:
          value= ((char *) status_var + (ulonglong) value);
          /* fall through */
        case SHOW_LONGLONG:
          end= longlong10_to_str(*(longlong*) value, buff, 10);
          break;
        case SHOW_HA_ROWS:
          end= longlong10_to_str((longlong) *(ha_rows*) value, buff, 10);
          break;
        case SHOW_BOOL:
          end= strmov(buff, *(bool*) value ? "ON" : "OFF");
          break;
        case SHOW_MY_BOOL:
          end= strmov(buff, *(my_bool*) value ? "ON" : "OFF");
          break;
        case SHOW_INT:
          end= int10_to_str((long) *(uint32*) value, buff, 10);
          break;
        case SHOW_HAVE:
        {
          SHOW_COMP_OPTION tmp= *(SHOW_COMP_OPTION*) value;
          pos= show_comp_option_name[(int) tmp];
          end= strend(pos);
          break;
        }
        case SHOW_CHAR:
        {
          if (!(pos= value))
            pos= "";
          end= strend(pos);
          break;
        }
       case SHOW_CHAR_PTR:
        {
          if (!(pos= *(char**) value))
            pos= "";
          end= strend(pos);
          break;
        }
        case SHOW_KEY_CACHE_LONG:
          value= (char*) dflt_key_cache + (ulong)value;
          end= int10_to_str(*(long*) value, buff, 10);
          break;
        case SHOW_KEY_CACHE_LONGLONG:
          value= (char*) dflt_key_cache + (ulong)value;
	  end= longlong10_to_str(*(longlong*) value, buff, 10);
	  break;
        case SHOW_UNDEF:
          break;                                        // Return empty string
        case SHOW_SYS:                                  // Cannot happen
        default:
          DBUG_ASSERT(0);
          break;
        }
        restore_record(table, s->default_values);
        table->field[0]->store(name_buffer, strlen(name_buffer),
                               system_charset_info);
        table->field[1]->store(pos, (uint32) (end - pos), system_charset_info);
        table->field[1]->set_notnull();

        pthread_mutex_unlock(&LOCK_global_system_variables);

        if (schema_table_store_record(thd, table))
          return(TRUE);
      }
    }
  }

  return(FALSE);
}


/* collect status for all running threads */

void calc_sum_of_all_status(STATUS_VAR *to)
{

  /* Ensure that thread id not killed during loop */
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list

  I_List_iterator<THD> it(threads);
  THD *tmp;
  
  /* Get global values as base */
  *to= global_status_var;
  
  /* Add to this status from existing threads */
  while ((tmp= it++))
    add_to_status(to, &tmp->status_var);
  
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  return;
}


/* This is only used internally, but we need it here as a forward reference */
extern ST_SCHEMA_TABLE schema_tables[];

typedef struct st_lookup_field_values
{
  LEX_STRING db_value, table_value;
  bool wild_db_value, wild_table_value;
} LOOKUP_FIELD_VALUES;


/*
  Store record to I_S table, convert HEAP table
  to MyISAM if necessary

  SYNOPSIS
    schema_table_store_record()
    thd                   thread handler
    table                 Information schema table to be updated

  RETURN
    0	                  success
    1	                  error
*/

bool schema_table_store_record(THD *thd, TABLE *table)
{
  int error;
  if ((error= table->file->ha_write_row(table->record[0])))
  {
    TMP_TABLE_PARAM *param= table->pos_in_table_list->schema_table_param;

    if (create_myisam_from_heap(thd, table, param->start_recinfo, 
                                &param->recinfo, error, 0))
      return 1;
  }
  return 0;
}


int make_table_list(THD *thd, SELECT_LEX *sel,
                    LEX_STRING *db_name, LEX_STRING *table_name)
{
  Table_ident *table_ident;
  table_ident= new Table_ident(thd, *db_name, *table_name, 1);
  sel->init_query();
  if (!sel->add_table_to_list(thd, table_ident, 0, 0, TL_READ))
    return 1;
  return 0;
}


/**
  @brief    Get lookup value from the part of 'WHERE' condition 

  @details This function gets lookup value from 
           the part of 'WHERE' condition if it's possible and 
           fill appropriate lookup_field_vals struct field
           with this value.

  @param[in]      thd                   thread handler
  @param[in]      item_func             part of WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool get_lookup_value(THD *thd, Item_func *item_func,
                      TABLE_LIST *table, 
                      LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  ST_SCHEMA_TABLE *schema_table= table->schema_table;
  ST_FIELD_INFO *field_info= schema_table->fields_info;
  const char *field_name1= schema_table->idx_field1 >= 0 ?
    field_info[schema_table->idx_field1].field_name : "";
  const char *field_name2= schema_table->idx_field2 >= 0 ?
    field_info[schema_table->idx_field2].field_name : "";

  if (item_func->functype() == Item_func::EQ_FUNC ||
      item_func->functype() == Item_func::EQUAL_FUNC)
  {
    int idx_field, idx_val;
    char tmp[MAX_FIELD_WIDTH];
    String *tmp_str, str_buff(tmp, sizeof(tmp), system_charset_info);
    Item_field *item_field;
    CHARSET_INFO *cs= system_charset_info;

    if (item_func->arguments()[0]->type() == Item::FIELD_ITEM &&
        item_func->arguments()[1]->const_item())
    {
      idx_field= 0;
      idx_val= 1;
    }
    else if (item_func->arguments()[1]->type() == Item::FIELD_ITEM &&
             item_func->arguments()[0]->const_item())
    {
      idx_field= 1;
      idx_val= 0;
    }
    else
      return 0;

    item_field= (Item_field*) item_func->arguments()[idx_field];
    if (table->table != item_field->field->table)
      return 0;
    tmp_str= item_func->arguments()[idx_val]->val_str(&str_buff);

    /* impossible value */
    if (!tmp_str)
      return 1;

    /* Lookup value is database name */
    if (!cs->coll->strnncollsp(cs, (uchar *) field_name1, strlen(field_name1),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0))
    {
      thd->make_lex_string(&lookup_field_vals->db_value, tmp_str->ptr(),
                           tmp_str->length(), FALSE);
    }
    /* Lookup value is table name */
    else if (!cs->coll->strnncollsp(cs, (uchar *) field_name2,
                                    strlen(field_name2),
                                    (uchar *) item_field->field_name,
                                    strlen(item_field->field_name), 0))
    {
      thd->make_lex_string(&lookup_field_vals->table_value, tmp_str->ptr(),
                           tmp_str->length(), FALSE);
    }
  }
  return 0;
}


/**
  @brief    Calculates lookup values from 'WHERE' condition 

  @details This function calculates lookup value(database name, table name)
           from 'WHERE' condition if it's possible and 
           fill lookup_field_vals struct fields with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool calc_lookup_values_from_cond(THD *thd, COND *cond, TABLE_LIST *table,
                                  LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  if (!cond)
    return 0;

  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        if (item->type() == Item::FUNC_ITEM)
        {
          if (get_lookup_value(thd, (Item_func*)item, table, lookup_field_vals))
            return 1;
        }
        else
        {
          if (calc_lookup_values_from_cond(thd, item, table, lookup_field_vals))
            return 1;
        }
      }
    }
    return 0;
  }
  else if (cond->type() == Item::FUNC_ITEM &&
           get_lookup_value(thd, (Item_func*) cond, table, lookup_field_vals))
    return 1;
  return 0;
}


bool uses_only_table_name_fields(Item *item, TABLE_LIST *table)
{
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *item_func= (Item_func*)item;
    for (uint i=0; i<item_func->argument_count(); i++)
    {
      if (!uses_only_table_name_fields(item_func->arguments()[i], table))
        return 0;
    }
  }
  else if (item->type() == Item::FIELD_ITEM)
  {
    Item_field *item_field= (Item_field*)item;
    CHARSET_INFO *cs= system_charset_info;
    ST_SCHEMA_TABLE *schema_table= table->schema_table;
    ST_FIELD_INFO *field_info= schema_table->fields_info;
    const char *field_name1= schema_table->idx_field1 >= 0 ?
      field_info[schema_table->idx_field1].field_name : "";
    const char *field_name2= schema_table->idx_field2 >= 0 ?
      field_info[schema_table->idx_field2].field_name : "";
    if (table->table != item_field->field->table ||
        (cs->coll->strnncollsp(cs, (uchar *) field_name1, strlen(field_name1),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0) &&
         cs->coll->strnncollsp(cs, (uchar *) field_name2, strlen(field_name2),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0)))
      return 0;
  }
  else if (item->type() == Item::REF_ITEM)
    return uses_only_table_name_fields(item->real_item(), table);

  if (item->type() == Item::SUBSELECT_ITEM && !item->const_item())
    return 0;

  return 1;
}


static COND * make_cond_for_info_schema(COND *cond, TABLE_LIST *table)
{
  if (!cond)
    return (COND*) 0;
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_info_schema(item, table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (COND*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_info_schema(item, table);
	if (!fix)
	  return (COND*) 0;
	new_cond->argument_list()->push_back(fix);
      }
      new_cond->quick_fix_field();
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_only_table_name_fields(cond, table))
    return (COND*) 0;
  return cond;
}


/**
  @brief   Calculate lookup values(database name, table name)

  @details This function calculates lookup values(database name, table name)
           from 'WHERE' condition or wild values (for 'SHOW' commands only)
           from LEX struct and fill lookup_field_vals struct field
           with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      tables                I_S table
  @param[in, out] lookup_field_values   Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool get_lookup_field_values(THD *thd, COND *cond, TABLE_LIST *tables,
                             LOOKUP_FIELD_VALUES *lookup_field_values)
{
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  bzero((char*) lookup_field_values, sizeof(LOOKUP_FIELD_VALUES));
  switch (lex->sql_command) {
  case SQLCOM_SHOW_DATABASES:
    if (wild)
    {
      lookup_field_values->db_value.str= (char*) wild;
      lookup_field_values->db_value.length= strlen(wild);
      lookup_field_values->wild_db_value= 1;
    }
    return 0;
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
    lookup_field_values->db_value.str= lex->select_lex.db;
    lookup_field_values->db_value.length=strlen(lex->select_lex.db);
    if (wild)
    {
      lookup_field_values->table_value.str= (char*)wild;
      lookup_field_values->table_value.length= strlen(wild);
      lookup_field_values->wild_table_value= 1;
    }
    return 0;
  default:
    /*
      The "default" is for queries over I_S.
      All previous cases handle SHOW commands.
    */
    return calc_lookup_values_from_cond(thd, cond, tables, lookup_field_values);
  }
}


enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table)
{
  return (enum enum_schema_tables) (schema_table - &schema_tables[0]);
}


/*
  Create db names list. Information schema name always is first in list

  SYNOPSIS
    make_db_list()
    thd                   thread handler
    files                 list of db names
    wild                  wild string
    idx_field_vals        idx_field_vals->db_name contains db name or
                          wild string
    with_i_schema         returns 1 if we added 'IS' name to list
                          otherwise returns 0 

  RETURN
    zero                  success
    non-zero              error
*/

int make_db_list(THD *thd, List<LEX_STRING> *files,
                 LOOKUP_FIELD_VALUES *lookup_field_vals,
                 bool *with_i_schema)
{
  LEX_STRING *i_s_name_copy= 0;
  i_s_name_copy= thd->make_lex_string(i_s_name_copy,
                                      INFORMATION_SCHEMA_NAME.str,
                                      INFORMATION_SCHEMA_NAME.length, TRUE);
  *with_i_schema= 0;
  if (lookup_field_vals->wild_db_value)
  {
    /*
      This part of code is only for SHOW DATABASES command.
      idx_field_vals->db_value can be 0 when we don't use
      LIKE clause (see also get_index_field_values() function)
    */
    if (!lookup_field_vals->db_value.str ||
        !wild_case_compare(system_charset_info, 
                           INFORMATION_SCHEMA_NAME.str,
                           lookup_field_vals->db_value.str))
    {
      *with_i_schema= 1;
      if (files->push_back(i_s_name_copy))
        return 1;
    }
    return (find_files(thd, files, NullS, mysql_data_home,
                       lookup_field_vals->db_value.str, 1) != FIND_FILES_OK);
  }


  /*
    If we have db lookup vaule we just add it to list and
    exit from the function
  */
  if (lookup_field_vals->db_value.str)
  {
    if (!my_strcasecmp(system_charset_info, INFORMATION_SCHEMA_NAME.str,
                       lookup_field_vals->db_value.str))
    {
      *with_i_schema= 1;
      if (files->push_back(i_s_name_copy))
        return 1;
      return 0;
    }
    if (files->push_back(&lookup_field_vals->db_value))
      return 1;
    return 0;
  }

  /*
    Create list of existing databases. It is used in case
    of select from information schema table
  */
  if (files->push_back(i_s_name_copy))
    return 1;
  *with_i_schema= 1;
  return (find_files(thd, files, NullS,
                     mysql_data_home, NullS, 1) != FIND_FILES_OK);
}


struct st_add_schema_table 
{
  List<LEX_STRING> *files;
  const char *wild;
};


static my_bool add_schema_table(THD *thd, plugin_ref plugin,
                                void* p_data)
{
  LEX_STRING *file_name= 0;
  st_add_schema_table *data= (st_add_schema_table *)p_data;
  List<LEX_STRING> *file_list= data->files;
  const char *wild= data->wild;
  ST_SCHEMA_TABLE *schema_table= plugin_data(plugin, ST_SCHEMA_TABLE *);

  if (schema_table->hidden)
      return(0);
  if (wild)
  {
    if (lower_case_table_names)
    {
      if (wild_case_compare(files_charset_info,
                            schema_table->table_name,
                            wild))
        return(0);
    }
    else if (wild_compare(schema_table->table_name, wild, 0))
      return(0);
  }

  if ((file_name= thd->make_lex_string(file_name, schema_table->table_name,
                                       strlen(schema_table->table_name),
                                       TRUE)) &&
      !file_list->push_back(file_name))
    return(0);
  return(1);
}


int schema_tables_add(THD *thd, List<LEX_STRING> *files, const char *wild)
{
  LEX_STRING *file_name= 0;
  ST_SCHEMA_TABLE *tmp_schema_table= schema_tables;
  st_add_schema_table add_data;

  for (; tmp_schema_table->table_name; tmp_schema_table++)
  {
    if (tmp_schema_table->hidden)
      continue;
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info,
                              tmp_schema_table->table_name,
                              wild))
          continue;
      }
      else if (wild_compare(tmp_schema_table->table_name, wild, 0))
        continue;
    }
    if ((file_name= 
         thd->make_lex_string(file_name, tmp_schema_table->table_name,
                              strlen(tmp_schema_table->table_name), TRUE)) &&
        !files->push_back(file_name))
      continue;
    return(1);
  }

  add_data.files= files;
  add_data.wild= wild;
  if (plugin_foreach(thd, add_schema_table,
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &add_data))
      return(1);

  return(0);
}


/**
  @brief          Create table names list

  @details        The function creates the list of table names in
                  database

  @param[in]      thd                   thread handler
  @param[in]      table_names           List of table names in database
  @param[in]      lex                   pointer to LEX struct
  @param[in]      lookup_field_vals     pointer to LOOKUP_FIELD_VALUE struct
  @param[in]      with_i_schema         TRUE means that we add I_S tables to list
  @param[in]      db_name               database name

  @return         Operation status
    @retval       0           ok
    @retval       1           fatal error
    @retval       2           Not fatal error; Safe to ignore this file list
*/

static int
make_table_name_list(THD *thd, List<LEX_STRING> *table_names, LEX *lex,
                     LOOKUP_FIELD_VALUES *lookup_field_vals,
                     bool with_i_schema, LEX_STRING *db_name)
{
  char path[FN_REFLEN];
  build_table_filename(path, sizeof(path), db_name->str, "", "", 0);
  if (!lookup_field_vals->wild_table_value &&
      lookup_field_vals->table_value.str)
  {
    if (with_i_schema)
    {
      if (find_schema_table(thd, lookup_field_vals->table_value.str))
      {
        if (table_names->push_back(&lookup_field_vals->table_value))
          return 1;
      }
    }
    else
    {    
      if (table_names->push_back(&lookup_field_vals->table_value))
        return 1;
    }
    return 0;
  }

  /*
    This call will add all matching the wildcards (if specified) IS tables
    to the list
  */
  if (with_i_schema)
    return (schema_tables_add(thd, table_names,
                              lookup_field_vals->table_value.str));

  find_files_result res= find_files(thd, table_names, db_name->str, path,
                                    lookup_field_vals->table_value.str, 0);
  if (res != FIND_FILES_OK)
  {
    /*
      Downgrade errors about problems with database directory to
      warnings if this is not a 'SHOW' command.  Another thread
      may have dropped database, and we may still have a name
      for that directory.
    */
    if (res == FIND_FILES_DIR)
    {
      if (lex->sql_command != SQLCOM_SELECT)
        return 1;
      thd->clear_error();
      return 2;
    }
    return 1;
  }
  return 0;
}


/**
  @brief          Fill I_S table for SHOW COLUMNS|INDEX commands

  @param[in]      thd                      thread handler
  @param[in]      tables                   TABLE_LIST for I_S table
  @param[in]      schema_table             pointer to I_S structure
  @param[in]      open_tables_state_backup pointer to Open_tables_state object
                                           which is used to save|restore original
                                           status of variables related to
                                           open tables state

  @return         Operation status
    @retval       0           success
    @retval       1           error
*/

static int 
fill_schema_show_cols_or_idxs(THD *thd, TABLE_LIST *tables,
                              ST_SCHEMA_TABLE *schema_table,
                              Open_tables_state *open_tables_state_backup)
{
  LEX *lex= thd->lex;
  bool res;
  LEX_STRING tmp_lex_string, tmp_lex_string1, *db_name, *table_name;
  enum_sql_command save_sql_command= lex->sql_command;
  TABLE_LIST *show_table_list= (TABLE_LIST*) tables->schema_select_lex->
    table_list.first;
  TABLE *table= tables->table;
  int error= 1;

  lex->all_selects_list= tables->schema_select_lex;
  /*
    Restore thd->temporary_tables to be able to process
    temporary tables(only for 'show index' & 'show columns').
    This should be changed when processing of temporary tables for
    I_S tables will be done.
  */
  thd->temporary_tables= open_tables_state_backup->temporary_tables;
  /*
    Let us set fake sql_command so views won't try to merge
    themselves into main statement. If we don't do this,
    SELECT * from information_schema.xxxx will cause problems.
    SQLCOM_SHOW_FIELDS is used because it satisfies 'only_view_structure()' 
  */
  lex->sql_command= SQLCOM_SHOW_FIELDS;
  res= open_normal_and_derived_tables(thd, show_table_list,
                                      MYSQL_LOCK_IGNORE_FLUSH);
  lex->sql_command= save_sql_command;
  /*
    get_all_tables() returns 1 on failure and 0 on success thus
    return only these and not the result code of ::process_table()

    We should use show_table_list->alias instead of 
    show_table_list->table_name because table_name
    could be changed during opening of I_S tables. It's safe
    to use alias because alias contains original table name 
    in this case(this part of code is used only for 
    'show columns' & 'show statistics' commands).
  */
   table_name= thd->make_lex_string(&tmp_lex_string1, show_table_list->alias,
                                    strlen(show_table_list->alias), FALSE);
   db_name= thd->make_lex_string(&tmp_lex_string, show_table_list->db,
                                 show_table_list->db_length, FALSE);
      

   error= test(schema_table->process_table(thd, show_table_list,
                                           table, res, db_name,
                                           table_name));
   thd->temporary_tables= 0;
   close_tables_for_reopen(thd, &show_table_list);
   return(error);
}


/**
  @brief          Fill I_S table for SHOW TABLE NAMES commands

  @param[in]      thd                      thread handler
  @param[in]      table                    TABLE struct for I_S table
  @param[in]      db_name                  database name
  @param[in]      table_name               table name
  @param[in]      with_i_schema            I_S table if TRUE

  @return         Operation status
    @retval       0           success
    @retval       1           error
*/

static int fill_schema_table_names(THD *thd, TABLE *table,
                                   LEX_STRING *db_name, LEX_STRING *table_name,
                                   bool with_i_schema)
{
  if (with_i_schema)
  {
    table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"),
                           system_charset_info);
  }
  else
  {
    enum legacy_db_type not_used;
    char path[FN_REFLEN];
    (void) build_table_filename(path, sizeof(path), db_name->str, 
                                table_name->str, reg_ext, 0);
    switch (mysql_frm_type(thd, path, &not_used)) {
    case FRMTYPE_ERROR:
      table->field[3]->store(STRING_WITH_LEN("ERROR"),
                             system_charset_info);
      break;
    case FRMTYPE_TABLE:
      table->field[3]->store(STRING_WITH_LEN("BASE TABLE"),
                             system_charset_info);
      break;
    default:
      DBUG_ASSERT(0);
    }
    if (thd->is_error() && thd->main_da.sql_errno() == ER_NO_SUCH_TABLE)
    {
      thd->clear_error();
      return 0;
    }
  }
  if (schema_table_store_record(thd, table))
    return 1;
  return 0;
}


/**
  @brief          Get open table method

  @details        The function calculates the method which will be used
                  for table opening:
                  SKIP_OPEN_TABLE - do not open table
                  OPEN_FRM_ONLY   - open FRM file only
                  OPEN_FULL_TABLE - open FRM, data, index files
  @param[in]      tables               I_S table table_list
  @param[in]      schema_table         I_S table struct
  @param[in]      schema_table_idx     I_S table index

  @return         return a set of flags
    @retval       SKIP_OPEN_TABLE | OPEN_FRM_ONLY | OPEN_FULL_TABLE
*/

static uint get_table_open_method(TABLE_LIST *tables,
                                  ST_SCHEMA_TABLE *schema_table,
                                  enum enum_schema_tables schema_table_idx)
{
  /*
    determine which method will be used for table opening
  */
  if (schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
  {
    Field **ptr, *field;
    int table_open_method= 0, field_indx= 0;
    for (ptr=tables->table->field; (field= *ptr) ; ptr++)
    {
      if (bitmap_is_set(tables->table->read_set, field->field_index))
        table_open_method|= schema_table->fields_info[field_indx].open_method;
      field_indx++;
    }
    return table_open_method;
  }
  /* I_S tables which use get_all_tables but can not be optimized */
  return (uint) OPEN_FULL_TABLE;
}


/**
  @brief          Fill I_S table with data from FRM file only

  @param[in]      thd                      thread handler
  @param[in]      table                    TABLE struct for I_S table
  @param[in]      schema_table             I_S table struct
  @param[in]      db_name                  database name
  @param[in]      table_name               table name
  @param[in]      schema_table_idx         I_S table index

  @return         Operation status
    @retval       0           Table is processed and we can continue
                              with new table
    @retval       1           It's view and we have to use
                              open_tables function for this table
*/

static int fill_schema_table_from_frm(THD *thd,TABLE_LIST *tables,
                                      ST_SCHEMA_TABLE *schema_table, 
                                      LEX_STRING *db_name,
                                      LEX_STRING *table_name,
                                      enum enum_schema_tables schema_table_idx)
{
  TABLE *table= tables->table;
  TABLE_SHARE *share;
  TABLE tbl;
  TABLE_LIST table_list;
  uint res= 0;
  int error;
  char key[MAX_DBKEY_LENGTH];
  uint key_length;

  bzero((char*) &table_list, sizeof(TABLE_LIST));
  bzero((char*) &tbl, sizeof(TABLE));

  table_list.table_name= table_name->str;
  table_list.db= db_name->str;

  key_length= create_table_def_key(thd, key, &table_list, 0);
  pthread_mutex_lock(&LOCK_open);
  share= get_table_share(thd, &table_list, key,
                         key_length, OPEN_VIEW, &error);
  if (!share)
  {
    res= 0;
    goto err;
  }

  {
    tbl.s= share;
    table_list.table= &tbl;
    res= schema_table->process_table(thd, &table_list, table,
                                     res, db_name, table_name);
  }

  release_table_share(share, RELEASE_NORMAL);

err:
  pthread_mutex_unlock(&LOCK_open);
  thd->clear_error();
  return res;
}



/**
  @brief          Fill I_S tables whose data are retrieved
                  from frm files and storage engine

  @details        The information schema tables are internally represented as
                  temporary tables that are filled at query execution time.
                  Those I_S tables whose data are retrieved
                  from frm files and storage engine are filled by the function
                  get_all_tables().

  @param[in]      thd                      thread handler
  @param[in]      tables                   I_S table
  @param[in]      cond                     'WHERE' condition

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/

int get_all_tables(THD *thd, TABLE_LIST *tables, COND *cond)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  SELECT_LEX *old_all_select_lex= lex->all_selects_list;
  enum_sql_command save_sql_command= lex->sql_command;
  SELECT_LEX *lsel= tables->schema_select_lex;
  ST_SCHEMA_TABLE *schema_table= tables->schema_table;
  SELECT_LEX sel;
  LOOKUP_FIELD_VALUES lookup_field_vals;
  LEX_STRING *db_name, *table_name;
  bool with_i_schema;
  enum enum_schema_tables schema_table_idx;
  List<LEX_STRING> db_names;
  List_iterator_fast<LEX_STRING> it(db_names);
  COND *partial_cond= 0;
  uint derived_tables= lex->derived_tables; 
  int error= 1;
  Open_tables_state open_tables_state_backup;
  Query_tables_list query_tables_list_backup;
  uint table_open_method;
  bool old_value= thd->no_warnings_for_error;

  lex->reset_n_backup_query_tables_list(&query_tables_list_backup);

  /*
    We should not introduce deadlocks even if we already have some
    tables open and locked, since we won't lock tables which we will
    open and will ignore possible name-locks for these tables.
  */
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup);

  schema_table_idx= get_schema_table_idx(schema_table);
  tables->table_open_method= table_open_method=
    get_table_open_method(tables, schema_table, schema_table_idx);
  /* 
    this branch processes SHOW FIELDS, SHOW INDEXES commands.
    see sql_parse.cc, prepare_schema_table() function where
    this values are initialized
  */
  if (lsel && lsel->table_list.first)
  {
    error= fill_schema_show_cols_or_idxs(thd, tables, schema_table,
                                         &open_tables_state_backup);
    goto err;
  }

  if (get_lookup_field_values(thd, cond, tables, &lookup_field_vals))
  {
    error= 0;
    goto err;
  }

  if (!lookup_field_vals.wild_db_value && !lookup_field_vals.wild_table_value)
  {
    /* 
      if lookup value is empty string then
      it's impossible table name or db name
    */
    if (lookup_field_vals.db_value.str &&
        !lookup_field_vals.db_value.str[0] ||
        lookup_field_vals.table_value.str &&
        !lookup_field_vals.table_value.str[0])
    {
      error= 0;
      goto err;
    }
  }

  if (lookup_field_vals.db_value.length &&
      !lookup_field_vals.wild_db_value)
    tables->has_db_lookup_value= TRUE;
  if (lookup_field_vals.table_value.length &&
      !lookup_field_vals.wild_table_value) 
    tables->has_table_lookup_value= TRUE;

  if (tables->has_db_lookup_value && tables->has_table_lookup_value)
    partial_cond= 0;
  else
    partial_cond= make_cond_for_info_schema(cond, tables);

  if (lex->describe)
  {
    /* EXPLAIN SELECT */
    error= 0;
    goto err;
  }

  if (make_db_list(thd, &db_names, &lookup_field_vals, &with_i_schema))
    goto err;
  it.rewind(); /* To get access to new elements in basis list */
  while ((db_name= it++))
  {
    {
      thd->no_warnings_for_error= 1;
      List<LEX_STRING> table_names;
      int res= make_table_name_list(thd, &table_names, lex,
                                    &lookup_field_vals,
                                    with_i_schema, db_name);
      if (res == 2)   /* Not fatal error, continue */
        continue;
      if (res)
        goto err;

      List_iterator_fast<LEX_STRING> it_files(table_names);
      while ((table_name= it_files++))
      {
	restore_record(table, s->default_values);
        table->field[schema_table->idx_field1]->
          store(db_name->str, db_name->length, system_charset_info);
        table->field[schema_table->idx_field2]->
          store(table_name->str, table_name->length, system_charset_info);

        if (!partial_cond || partial_cond->val_int())
        {
          /*
            If table is I_S.tables and open_table_method is 0 (eg SKIP_OPEN)
            we can skip table opening and we don't have lookup value for 
            table name or lookup value is wild string(table name list is
            already created by make_table_name_list() function).
          */
          if (!table_open_method && schema_table_idx == SCH_TABLES &&
              (!lookup_field_vals.table_value.length ||
               lookup_field_vals.wild_table_value))
          {
            if (schema_table_store_record(thd, table))
              goto err;      /* Out of space in temporary table */
            continue;
          }

          /* SHOW TABLE NAMES command */
          if (schema_table_idx == SCH_TABLE_NAMES)
          {
            if (fill_schema_table_names(thd, tables->table, db_name,
                                        table_name, with_i_schema))
              continue;
          }
          else
          {
            if (!(table_open_method & ~OPEN_FRM_ONLY) &&
                !with_i_schema)
            {
              if (!fill_schema_table_from_frm(thd, tables, schema_table, db_name,
                                              table_name, schema_table_idx))
                continue;
            }

            int res;
            LEX_STRING tmp_lex_string, orig_db_name;
            /*
              Set the parent lex of 'sel' because it is needed by
              sel.init_query() which is called inside make_table_list.
            */
            thd->no_warnings_for_error= 1;
            sel.parent_lex= lex;
            /* db_name can be changed in make_table_list() func */
            if (!thd->make_lex_string(&orig_db_name, db_name->str,
                                      db_name->length, FALSE))
              goto err;
            if (make_table_list(thd, &sel, db_name, table_name))
              goto err;
            TABLE_LIST *show_table_list= (TABLE_LIST*) sel.table_list.first;
            lex->all_selects_list= &sel;
            lex->derived_tables= 0;
            lex->sql_command= SQLCOM_SHOW_FIELDS;
            show_table_list->i_s_requested_object=
              schema_table->i_s_requested_object;
            res= open_normal_and_derived_tables(thd, show_table_list,
                                                MYSQL_LOCK_IGNORE_FLUSH);
            lex->sql_command= save_sql_command;
            /*
              XXX:  show_table_list has a flag i_is_requested,
              and when it's set, open_normal_and_derived_tables()
              can return an error without setting an error message
              in THD, which is a hack. This is why we have to
              check for res, then for thd->is_error() only then
              for thd->main_da.sql_errno().
            */
            if (res && thd->is_error() &&
                thd->main_da.sql_errno() == ER_NO_SUCH_TABLE)
            {
              /*
                Hide error for not existing table.
                This error can occur for example when we use
                where condition with db name and table name and this
                table does not exist.
              */
              res= 0;
              thd->clear_error();
            }
            else
            {
              /*
                We should use show_table_list->alias instead of 
                show_table_list->table_name because table_name
                could be changed during opening of I_S tables. It's safe
                to use alias because alias contains original table name 
                in this case.
              */
              thd->make_lex_string(&tmp_lex_string, show_table_list->alias,
                                   strlen(show_table_list->alias), FALSE);
              res= schema_table->process_table(thd, show_table_list, table,
                                               res, &orig_db_name,
                                               &tmp_lex_string);
              close_tables_for_reopen(thd, &show_table_list);
            }
            DBUG_ASSERT(!lex->query_tables_own_last);
            if (res)
              goto err;
          }
        }
      }
      /*
        If we have information schema its always the first table and only
        the first table. Reset for other tables.
      */
      with_i_schema= 0;
    }
  }

  error= 0;
err:
  thd->restore_backup_open_tables_state(&open_tables_state_backup);
  lex->restore_backup_query_tables_list(&query_tables_list_backup);
  lex->derived_tables= derived_tables;
  lex->all_selects_list= old_all_select_lex;
  lex->sql_command= save_sql_command;
  thd->no_warnings_for_error= old_value;
  return(error);
}


bool store_schema_shemata(THD* thd, TABLE *table, LEX_STRING *db_name,
                          CHARSET_INFO *cs)
{
  restore_record(table, s->default_values);
  table->field[1]->store(db_name->str, db_name->length, system_charset_info);
  table->field[2]->store(cs->csname, strlen(cs->csname), system_charset_info);
  table->field[3]->store(cs->name, strlen(cs->name), system_charset_info);
  return schema_table_store_record(thd, table);
}


int fill_schema_schemata(THD *thd, TABLE_LIST *tables, COND *cond)
{
  /*
    TODO: fill_schema_shemata() is called when new client is connected.
    Returning error status in this case leads to client hangup.
  */

  LOOKUP_FIELD_VALUES lookup_field_vals;
  List<LEX_STRING> db_names;
  LEX_STRING *db_name;
  bool with_i_schema;
  HA_CREATE_INFO create;
  TABLE *table= tables->table;

  if (get_lookup_field_values(thd, cond, tables, &lookup_field_vals))
    return(0);
  if (make_db_list(thd, &db_names, &lookup_field_vals,
                   &with_i_schema))
    return(1);

  /*
    If we have lookup db value we should check that the database exists
  */
  if(lookup_field_vals.db_value.str && !lookup_field_vals.wild_db_value &&
     !with_i_schema)
  {
    char path[FN_REFLEN+16];
    uint path_len;
    struct stat stat_info;
    if (!lookup_field_vals.db_value.str[0])
      return(0);
    path_len= build_table_filename(path, sizeof(path),
                                   lookup_field_vals.db_value.str, "", "", 0);
    path[path_len-1]= 0;
    if (stat(path,&stat_info))
      return(0);
  }

  List_iterator_fast<LEX_STRING> it(db_names);
  while ((db_name=it++))
  {
    if (with_i_schema)       // information schema name is always first in list
    {
      if (store_schema_shemata(thd, table, db_name,
                               system_charset_info))
        return(1);
      with_i_schema= 0;
      continue;
    }
    {
      load_db_opt_by_name(thd, db_name->str, &create);
      if (store_schema_shemata(thd, table, db_name,
                               create.default_table_charset))
        return(1);
    }
  }
  return(0);
}


static int get_schema_tables_record(THD *thd, TABLE_LIST *tables,
				    TABLE *table, bool res,
				    LEX_STRING *db_name,
				    LEX_STRING *table_name)
{
  const char *tmp_buff;
  MYSQL_TIME time;
  CHARSET_INFO *cs= system_charset_info;

  restore_record(table, s->default_values);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(table_name->str, table_name->length, cs);
  if (res)
  {
    /*
      there was errors during opening tables
    */
    const char *error= thd->is_error() ? thd->main_da.message() : "";
    if (tables->schema_table)
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    else
      table->field[3]->store(STRING_WITH_LEN("BASE TABLE"), cs);
    table->field[20]->store(error, strlen(error), cs);
    thd->clear_error();
  }
  else
  {
    char option_buff[350],*ptr;
    TABLE *show_table= tables->table;
    TABLE_SHARE *share= show_table->s;
    handler *file= show_table->file;
    handlerton *tmp_db_type= share->db_type();
    if (share->tmp_table == SYSTEM_TMP_TABLE)
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    else if (share->tmp_table)
      table->field[3]->store(STRING_WITH_LEN("LOCAL TEMPORARY"), cs);
    else
      table->field[3]->store(STRING_WITH_LEN("BASE TABLE"), cs);

    for (int i= 4; i < 20; i++)
    {
      if (i == 7 || (i > 12 && i < 17) || i == 18)
        continue;
      table->field[i]->set_notnull();
    }
    tmp_buff= (char *) ha_resolve_storage_engine_name(tmp_db_type);
    table->field[4]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[5]->store((longlong) share->frm_version, TRUE);

    ptr=option_buff;
    if (share->min_rows)
    {
      ptr=strmov(ptr," min_rows=");
      ptr=longlong10_to_str(share->min_rows,ptr,10);
    }
    if (share->max_rows)
    {
      ptr=strmov(ptr," max_rows=");
      ptr=longlong10_to_str(share->max_rows,ptr,10);
    }
    if (share->avg_row_length)
    {
      ptr=strmov(ptr," avg_row_length=");
      ptr=longlong10_to_str(share->avg_row_length,ptr,10);
    }
    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      ptr=strmov(ptr," pack_keys=1");
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      ptr=strmov(ptr," pack_keys=0");
    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compability */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      ptr=strmov(ptr," checksum=1");
    if (share->page_checksum != HA_CHOICE_UNDEF)
      ptr= strxmov(ptr, " page_checksum=",
                   ha_choice_values[(uint) share->page_checksum], NullS);
    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      ptr=strmov(ptr," delay_key_write=1");
    if (share->row_type != ROW_TYPE_DEFAULT)
      ptr=strxmov(ptr, " row_format=", 
                  ha_row_type[(uint) share->row_type],
                  NullS);
    if (share->transactional != HA_CHOICE_UNDEF)
    {
      ptr= strxmov(ptr, " TRANSACTIONAL=",
                   (share->transactional == HA_CHOICE_YES ? "1" : "0"),
                   NullS);
    }
    if (share->transactional != HA_CHOICE_UNDEF)
      ptr= strxmov(ptr, " transactional=",
                   ha_choice_values[(uint) share->transactional], NullS);
    table->field[19]->store(option_buff+1,
                            (ptr == option_buff ? 0 : 
                             (uint) (ptr-option_buff)-1), cs);

    tmp_buff= (share->table_charset ?
               share->table_charset->name : "default");
    table->field[17]->store(tmp_buff, strlen(tmp_buff), cs);

    if (share->comment.str)
      table->field[20]->store(share->comment.str, share->comment.length, cs);

    if(file)
    {
      file->info(HA_STATUS_VARIABLE | HA_STATUS_TIME | HA_STATUS_AUTO |
                 HA_STATUS_NO_LOCK);
      enum row_type row_type = file->get_row_type();
      switch (row_type) {
      case ROW_TYPE_NOT_USED:
      case ROW_TYPE_DEFAULT:
        tmp_buff= ((share->db_options_in_use &
                    HA_OPTION_COMPRESS_RECORD) ? "Compressed" :
                   (share->db_options_in_use & HA_OPTION_PACK_RECORD) ?
                   "Dynamic" : "Fixed");
        break;
      case ROW_TYPE_FIXED:
        tmp_buff= "Fixed";
        break;
      case ROW_TYPE_DYNAMIC:
        tmp_buff= "Dynamic";
        break;
      case ROW_TYPE_COMPRESSED:
        tmp_buff= "Compressed";
        break;
      case ROW_TYPE_REDUNDANT:
        tmp_buff= "Redundant";
        break;
      case ROW_TYPE_COMPACT:
        tmp_buff= "Compact";
        break;
      case ROW_TYPE_PAGE:
        tmp_buff= "Paged";
        break;
      }
      table->field[6]->store(tmp_buff, strlen(tmp_buff), cs);
      if (!tables->schema_table)
      {
        table->field[7]->store((longlong) file->stats.records, TRUE);
        table->field[7]->set_notnull();
      }
      table->field[8]->store((longlong) file->stats.mean_rec_length, TRUE);
      table->field[9]->store((longlong) file->stats.data_file_length, TRUE);
      if (file->stats.max_data_file_length)
      {
        table->field[10]->store((longlong) file->stats.max_data_file_length,
                                TRUE);
      }
      table->field[11]->store((longlong) file->stats.index_file_length, TRUE);
      table->field[12]->store((longlong) file->stats.delete_length, TRUE);
      if (show_table->found_next_number_field)
      {
        table->field[13]->store((longlong) file->stats.auto_increment_value,
                                TRUE);
        table->field[13]->set_notnull();
      }
      if (file->stats.create_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.create_time);
        table->field[14]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
        table->field[14]->set_notnull();
      }
      if (file->stats.update_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.update_time);
        table->field[15]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
        table->field[15]->set_notnull();
      }
      if (file->stats.check_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.check_time);
        table->field[16]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
        table->field[16]->set_notnull();
      }
      if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
      {
        table->field[18]->store((longlong) file->checksum(), TRUE);
        table->field[18]->set_notnull();
      }
    }
  }
  return(schema_table_store_record(thd, table));
}


/**
  @brief    Store field characteristics into appropriate I_S table columns

  @param[in]      table             I_S table
  @param[in]      field             processed field
  @param[in]      cs                I_S table charset
  @param[in]      offset            offset from beginning of table
                                    to DATE_TYPE column in I_S table
                                    
  @return         void
*/

void store_column_type(TABLE *table, Field *field, CHARSET_INFO *cs,
                       uint offset)
{
  bool is_blob;
  int decimals, field_length;
  const char *tmp_buff;
  char column_type_buff[MAX_FIELD_WIDTH];
  String column_type(column_type_buff, sizeof(column_type_buff), cs);

  field->sql_type(column_type);
  /* DTD_IDENTIFIER column */
  table->field[offset + 7]->store(column_type.ptr(), column_type.length(), cs);
  table->field[offset + 7]->set_notnull();
  tmp_buff= strchr(column_type.ptr(), '(');
  /* DATA_TYPE column */
  table->field[offset]->store(column_type.ptr(),
                         (tmp_buff ? tmp_buff - column_type.ptr() :
                          column_type.length()), cs);
  is_blob= (field->type() == MYSQL_TYPE_BLOB);
  if (field->has_charset() || is_blob ||
      field->real_type() == MYSQL_TYPE_VARCHAR ||  // For varbinary type
      field->real_type() == MYSQL_TYPE_STRING)     // For binary type
  {
    uint32 octet_max_length= field->max_display_length();
    if (is_blob && octet_max_length != (uint32) 4294967295U)
      octet_max_length /= field->charset()->mbmaxlen;
    longlong char_max_len= is_blob ? 
      (longlong) octet_max_length / field->charset()->mbminlen :
      (longlong) octet_max_length / field->charset()->mbmaxlen;
    /* CHARACTER_MAXIMUM_LENGTH column*/
    table->field[offset + 1]->store(char_max_len, TRUE);
    table->field[offset + 1]->set_notnull();
    /* CHARACTER_OCTET_LENGTH column */
    table->field[offset + 2]->store((longlong) octet_max_length, TRUE);
    table->field[offset + 2]->set_notnull();
  }

  /*
    Calculate field_length and decimals.
    They are set to -1 if they should not be set (we should return NULL)
  */

  decimals= field->decimals();
  switch (field->type()) {
  case MYSQL_TYPE_NEWDECIMAL:
    field_length= ((Field_new_decimal*) field)->precision;
    break;
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
    field_length= field->max_display_length() - 1;
    break;
  case MYSQL_TYPE_FLOAT:  
  case MYSQL_TYPE_DOUBLE:
    field_length= field->field_length;
    if (decimals == NOT_FIXED_DEC)
      decimals= -1;                           // return NULL
    break;
  default:
    field_length= decimals= -1;
    break;
  }

  /* NUMERIC_PRECISION column */
  if (field_length >= 0)
  {
    table->field[offset + 3]->store((longlong) field_length, TRUE);
    table->field[offset + 3]->set_notnull();
  }
  /* NUMERIC_SCALE column */
  if (decimals >= 0)
  {
    table->field[offset + 4]->store((longlong) decimals, TRUE);
    table->field[offset + 4]->set_notnull();
  }
  if (field->has_charset())
  {
    /* CHARACTER_SET_NAME column*/
    tmp_buff= field->charset()->csname;
    table->field[offset + 5]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 5]->set_notnull();
    /* COLLATION_NAME column */
    tmp_buff= field->charset()->name;
    table->field[offset + 6]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 6]->set_notnull();
  }
}


static int get_schema_column_record(THD *thd, TABLE_LIST *tables,
				    TABLE *table, bool res,
				    LEX_STRING *db_name,
				    LEX_STRING *table_name)
{
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  CHARSET_INFO *cs= system_charset_info;
  TABLE *show_table;
  TABLE_SHARE *show_table_share;
  Field **ptr, *field, *timestamp_field;
  int count;

  if (res)
  {
    if (lex->sql_command != SQLCOM_SHOW_FIELDS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.COLUMS
        rather than in SHOW COLUMNS
      */ 
      if (thd->is_error())
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                     thd->main_da.sql_errno(), thd->main_da.message());
      thd->clear_error();
      res= 0;
    }
    return(res);
  }

  show_table= tables->table;
  show_table_share= show_table->s;
  count= 0;

  if (tables->schema_table)
  {
    ptr= show_table->field;
    timestamp_field= show_table->timestamp_field;
    show_table->use_all_columns();               // Required for default
  }
  else
  {
    ptr= show_table_share->field;
    timestamp_field= show_table_share->timestamp_field;
    /*
      read_set may be inited in case of
      temporary table
    */
    if (!show_table->read_set)
    {
      /* to satisfy 'field->val_str' ASSERTs */
      uchar *bitmaps;
      uint bitmap_size= show_table_share->column_bitmap_size;
      if (!(bitmaps= (uchar*) alloc_root(thd->mem_root, bitmap_size)))
        return(0);
      bitmap_init(&show_table->def_read_set,
                  (my_bitmap_map*) bitmaps, show_table_share->fields, FALSE);
      bitmap_set_all(&show_table->def_read_set);
      show_table->read_set= &show_table->def_read_set;
    }
    bitmap_set_all(show_table->read_set);
  }

  for (; (field= *ptr) ; ptr++)
  {
    uchar *pos;
    char tmp[MAX_FIELD_WIDTH];
    String type(tmp,sizeof(tmp), system_charset_info);
    char *end;

    /* to satisfy 'field->val_str' ASSERTs */
    field->table= show_table;
    show_table->in_use= thd;

    if (wild && wild[0] &&
        wild_case_compare(system_charset_info, field->field_name,wild))
      continue;

    count++;
    /* Get default row, with all NULL fields set to NULL */
    restore_record(table, s->default_values);

    table->field[1]->store(db_name->str, db_name->length, cs);
    table->field[2]->store(table_name->str, table_name->length, cs);
    table->field[3]->store(field->field_name, strlen(field->field_name),
                           cs);
    table->field[4]->store((longlong) count, TRUE);

    if (get_field_default_value(thd, timestamp_field, field, &type, 0))
    {
      table->field[5]->store(type.ptr(), type.length(), cs);
      table->field[5]->set_notnull();
    }
    pos=(uchar*) ((field->flags & NOT_NULL_FLAG) ?  "NO" : "YES");
    table->field[6]->store((const char*) pos,
                           strlen((const char*) pos), cs);
    store_column_type(table, field, cs, 7);

    pos=(uchar*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
                 (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
                 (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
    table->field[15]->store((const char*) pos,
                            strlen((const char*) pos), cs);

    end= tmp;
    if (field->unireg_check == Field::NEXT_NUMBER)
      table->field[16]->store(STRING_WITH_LEN("auto_increment"), cs);
    if (timestamp_field == field &&
        field->unireg_check != Field::TIMESTAMP_DN_FIELD)
      table->field[16]->store(STRING_WITH_LEN("on update CURRENT_TIMESTAMP"),
                              cs);

    table->field[18]->store(field->comment.str, field->comment.length, cs);
    {
      enum ha_storage_media storage_type= (enum ha_storage_media)
        ((field->flags >> FIELD_STORAGE_FLAGS) & STORAGE_TYPE_MASK);
      enum column_format_type column_format= (enum column_format_type)
        ((field->flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
      pos=(uchar*)(storage_type == HA_SM_DEFAULT ? "Default" :
                   storage_type == HA_SM_DISK ? "Disk" : "Memory");
      table->field[19]->store((const char*) pos,
                              strlen((const char*) pos), cs);
      pos=(uchar*)(column_format == COLUMN_FORMAT_TYPE_DEFAULT ? "Default" :
                   column_format == COLUMN_FORMAT_TYPE_FIXED ? "Fixed" :
                                                             "Dynamic");
      table->field[20]->store((const char*) pos,
                              strlen((const char*) pos), cs);
    }
    if (schema_table_store_record(thd, table))
      return(1);
  }
  return(0);
}



int fill_schema_charsets(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;

  for (cs= all_charsets ; cs < all_charsets+255 ; cs++)
  {
    CHARSET_INFO *tmp_cs= cs[0];
    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) && 
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        !(tmp_cs->state & MY_CS_HIDDEN) &&
        !(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cs->csname,wild)))
    {
      const char *comment;
      restore_record(table, s->default_values);
      table->field[0]->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      table->field[1]->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      comment= tmp_cs->comment ? tmp_cs->comment : "";
      table->field[2]->store(comment, strlen(comment), scs);
      table->field[3]->store((longlong) tmp_cs->mbmaxlen, TRUE);
      if (schema_table_store_record(thd, table))
        return 1;
    }
  }
  return 0;
}


int fill_schema_collation(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for (cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) ||
         (tmp_cs->state & MY_CS_HIDDEN) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) || 
          !my_charset_same(tmp_cs, tmp_cl))
	continue;
      if (!(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cl->name,wild)))
      {
	const char *tmp_buff;
	restore_record(table, s->default_values);
	table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
        table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
        table->field[2]->store((longlong) tmp_cl->number, TRUE);
        tmp_buff= (tmp_cl->state & MY_CS_PRIMARY) ? "Yes" : "";
	table->field[3]->store(tmp_buff, strlen(tmp_buff), scs);
        tmp_buff= (tmp_cl->state & MY_CS_COMPILED)? "Yes" : "";
	table->field[4]->store(tmp_buff, strlen(tmp_buff), scs);
        table->field[5]->store((longlong) tmp_cl->strxfrm_multiply, TRUE);
        if (schema_table_store_record(thd, table))
          return 1;
      }
    }
  }
  return 0;
}


int fill_schema_coll_charset_app(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for (cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) || 
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) || 
          !my_charset_same(tmp_cs,tmp_cl))
	continue;
      restore_record(table, s->default_values);
      table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
      if (schema_table_store_record(thd, table))
        return 1;
    }
  }
  return 0;
}


static int get_schema_stat_record(THD *thd, TABLE_LIST *tables,
				  TABLE *table, bool res,
				  LEX_STRING *db_name,
				  LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;
  if (res)
  {
    if (thd->lex->sql_command != SQLCOM_SHOW_KEYS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.STATISTICS
        rather than in SHOW KEYS
      */
      if (thd->is_error())
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                     thd->main_da.sql_errno(), thd->main_da.message());
      thd->clear_error();
      res= 0;
    }
    return(res);
  }
  else
  {
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->s->key_info;
    if (show_table->file)
      show_table->file->info(HA_STATUS_VARIABLE |
                             HA_STATUS_NO_LOCK |
                             HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->s->keys ; i++,key_info++)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      const char *str;
      for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        restore_record(table, s->default_values);
        table->field[1]->store(db_name->str, db_name->length, cs);
        table->field[2]->store(table_name->str, table_name->length, cs);
        table->field[3]->store((longlong) ((key_info->flags &
                                            HA_NOSAME) ? 0 : 1), TRUE);
        table->field[4]->store(db_name->str, db_name->length, cs);
        table->field[5]->store(key_info->name, strlen(key_info->name), cs);
        table->field[6]->store((longlong) (j+1), TRUE);
        str=(key_part->field ? key_part->field->field_name :
             "?unknown field?");
        table->field[7]->store(str, strlen(str), cs);
        if (show_table->file)
        {
          if (show_table->file->index_flags(i, j, 0) & HA_READ_ORDER)
          {
            table->field[8]->store(((key_part->key_part_flag &
                                     HA_REVERSE_SORT) ?
                                    "D" : "A"), 1, cs);
            table->field[8]->set_notnull();
          }
          KEY *key=show_table->key_info+i;
          if (key->rec_per_key[j])
          {
            ha_rows records=(show_table->file->stats.records /
                             key->rec_per_key[j]);
            table->field[9]->store((longlong) records, TRUE);
            table->field[9]->set_notnull();
          }
          str= show_table->file->index_type(i);
          table->field[13]->store(str, strlen(str), cs);
        }
        if ((key_part->field &&
             key_part->length !=
             show_table->s->field[key_part->fieldnr-1]->key_length()))
        {
          table->field[10]->store((longlong) key_part->length /
                                  key_part->field->charset()->mbmaxlen, TRUE);
          table->field[10]->set_notnull();
        }
        uint flags= key_part->field ? key_part->field->flags : 0;
        const char *pos=(char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
        table->field[12]->store(pos, strlen(pos), cs);
        if (!show_table->s->keys_in_use.is_set(i))
          table->field[14]->store(STRING_WITH_LEN("disabled"), cs);
        else
          table->field[14]->store("", 0, cs);
        table->field[14]->set_notnull();
        DBUG_ASSERT(test(key_info->flags & HA_USES_COMMENT) == 
                   (key_info->comment.length > 0));
        if (key_info->flags & HA_USES_COMMENT)
          table->field[15]->store(key_info->comment.str, 
                                  key_info->comment.length, cs);
        if (schema_table_store_record(thd, table))
          return(1);
      }
    }
  }
  return(res);
}


bool store_constraints(THD *thd, TABLE *table, LEX_STRING *db_name,
                       LEX_STRING *table_name, const char *key_name,
                       uint key_len, const char *con_type, uint con_len)
{
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, s->default_values);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[3]->store(db_name->str, db_name->length, cs);
  table->field[4]->store(table_name->str, table_name->length, cs);
  table->field[5]->store(con_type, con_len, cs);
  return schema_table_store_record(thd, table);
}


static int get_schema_constraints_record(THD *thd, TABLE_LIST *tables,
					 TABLE *table, bool res,
					 LEX_STRING *db_name,
					 LEX_STRING *table_name)
{
  if (res)
  {
    if (thd->is_error())
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                   thd->main_da.sql_errno(), thd->main_da.message());
    thd->clear_error();
    return(0);
  }
  else
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->s->primary_key;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;

      if (i == primary_key && !strcmp(key_info->name, primary_key_name))
      {
        if (store_constraints(thd, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("PRIMARY KEY")))
          return(1);
      }
      else if (key_info->flags & HA_NOSAME)
      {
        if (store_constraints(thd, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("UNIQUE")))
          return(1);
      }
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info=it++))
    {
      if (store_constraints(thd, table, db_name, table_name, 
                            f_key_info->forein_id->str,
                            strlen(f_key_info->forein_id->str),
                            "FOREIGN KEY", 11))
        return(1);
    }
  }
  return(res);
}


void store_key_column_usage(TABLE *table, LEX_STRING *db_name,
                            LEX_STRING *table_name, const char *key_name,
                            uint key_len, const char *con_type, uint con_len,
                            longlong idx)
{
  CHARSET_INFO *cs= system_charset_info;
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[4]->store(db_name->str, db_name->length, cs);
  table->field[5]->store(table_name->str, table_name->length, cs);
  table->field[6]->store(con_type, con_len, cs);
  table->field[7]->store((longlong) idx, TRUE);
}


static int get_schema_key_column_usage_record(THD *thd,
					      TABLE_LIST *tables,
					      TABLE *table, bool res,
					      LEX_STRING *db_name,
					      LEX_STRING *table_name)
{
  if (res)
  {
    if (thd->is_error())
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                   thd->main_da.sql_errno(), thd->main_da.message());
    thd->clear_error();
    return(0);
  }
  else
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->s->primary_key;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;
      uint f_idx= 0;
      KEY_PART_INFO *key_part= key_info->key_part;
      for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        if (key_part->field)
        {
          f_idx++;
          restore_record(table, s->default_values);
          store_key_column_usage(table, db_name, table_name,
                                 key_info->name,
                                 strlen(key_info->name), 
                                 key_part->field->field_name, 
                                 strlen(key_part->field->field_name),
                                 (longlong) f_idx);
          if (schema_table_store_record(thd, table))
            return(1);
        }
      }
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> fkey_it(f_key_list);
    while ((f_key_info= fkey_it++))
    {
      LEX_STRING *f_info;
      LEX_STRING *r_info;
      List_iterator_fast<LEX_STRING> it(f_key_info->foreign_fields),
        it1(f_key_info->referenced_fields);
      uint f_idx= 0;
      while ((f_info= it++))
      {
        r_info= it1++;
        f_idx++;
        restore_record(table, s->default_values);
        store_key_column_usage(table, db_name, table_name,
                               f_key_info->forein_id->str,
                               f_key_info->forein_id->length,
                               f_info->str, f_info->length,
                               (longlong) f_idx);
        table->field[8]->store((longlong) f_idx, TRUE);
        table->field[8]->set_notnull();
        table->field[9]->store(f_key_info->referenced_db->str,
                               f_key_info->referenced_db->length,
                               system_charset_info);
        table->field[9]->set_notnull();
        table->field[10]->store(f_key_info->referenced_table->str,
                                f_key_info->referenced_table->length, 
                                system_charset_info);
        table->field[10]->set_notnull();
        table->field[11]->store(r_info->str, r_info->length,
                                system_charset_info);
        table->field[11]->set_notnull();
        if (schema_table_store_record(thd, table))
          return(1);
      }
    }
  }
  return(res);
}


int fill_open_tables(THD *thd, TABLE_LIST *tables, COND *cond)
{
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *cs= system_charset_info;
  OPEN_TABLE_LIST *open_list;
  if (!(open_list=list_open_tables(thd,thd->lex->select_lex.db, wild))
            && thd->is_fatal_error)
    return(1);

  for (; open_list ; open_list=open_list->next)
  {
    restore_record(table, s->default_values);
    table->field[0]->store(open_list->db, strlen(open_list->db), cs);
    table->field[1]->store(open_list->table, strlen(open_list->table), cs);
    table->field[2]->store((longlong) open_list->in_use, TRUE);
    table->field[3]->store((longlong) open_list->locked, TRUE);
    if (schema_table_store_record(thd, table))
      return(1);
  }
  return(0);
}


int fill_variables(THD *thd, TABLE_LIST *tables, COND *cond)
{
  int res= 0;
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  enum enum_schema_tables schema_table_idx=
    get_schema_table_idx(tables->schema_table);
  enum enum_var_type option_type= OPT_SESSION;
  bool upper_case_names= (schema_table_idx != SCH_VARIABLES);
  bool sorted_vars= (schema_table_idx == SCH_VARIABLES);

  if (lex->option_type == OPT_GLOBAL ||
      schema_table_idx == SCH_GLOBAL_VARIABLES)
    option_type= OPT_GLOBAL;

  rw_rdlock(&LOCK_system_variables_hash);
  res= show_status_array(thd, wild, enumerate_sys_vars(thd, sorted_vars),
                         option_type, NULL, "", tables->table, upper_case_names);
  rw_unlock(&LOCK_system_variables_hash);
  return(res);
}


int fill_status(THD *thd, TABLE_LIST *tables, COND *cond)
{
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  int res= 0;
  STATUS_VAR *tmp1, tmp;
  enum enum_schema_tables schema_table_idx=
    get_schema_table_idx(tables->schema_table);
  enum enum_var_type option_type;
  bool upper_case_names= (schema_table_idx != SCH_STATUS);

  if (schema_table_idx == SCH_STATUS)
  {
    option_type= lex->option_type;
    if (option_type == OPT_GLOBAL)
      tmp1= &tmp;
    else
      tmp1= thd->initial_status_var;
  }
  else if (schema_table_idx == SCH_GLOBAL_STATUS)
  {
    option_type= OPT_GLOBAL;
    tmp1= &tmp;
  }
  else
  { 
    option_type= OPT_SESSION;
    tmp1= &thd->status_var;
  }

  pthread_mutex_lock(&LOCK_status);
  if (option_type == OPT_GLOBAL)
    calc_sum_of_all_status(&tmp);
  res= show_status_array(thd, wild,
                         (SHOW_VAR *)all_status_vars.buffer,
                         option_type, tmp1, "", tables->table,
                         upper_case_names);
  pthread_mutex_unlock(&LOCK_status);
  return(res);
}


/*
  Fill and store records into I_S.referential_constraints table

  SYNOPSIS
    get_referential_constraints_record()
    thd                 thread handle
    tables              table list struct(processed table)
    table               I_S table
    res                 1 means the error during opening of the processed table
                        0 means processed table is opened without error
    base_name           db name
    file_name           table name

  RETURN
    0	ok
    #   error
*/

static int
get_referential_constraints_record(THD *thd, TABLE_LIST *tables,
                                   TABLE *table, bool res,
                                   LEX_STRING *db_name, LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;

  if (res)
  {
    if (thd->is_error())
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                   thd->main_da.sql_errno(), thd->main_da.message());
    thd->clear_error();
    return(0);
  }

  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      restore_record(table, s->default_values);
      table->field[1]->store(db_name->str, db_name->length, cs);
      table->field[9]->store(table_name->str, table_name->length, cs);
      table->field[2]->store(f_key_info->forein_id->str,
                             f_key_info->forein_id->length, cs);
      table->field[4]->store(f_key_info->referenced_db->str, 
                             f_key_info->referenced_db->length, cs);
      table->field[10]->store(f_key_info->referenced_table->str, 
                             f_key_info->referenced_table->length, cs);
      if (f_key_info->referenced_key_name)
      {
        table->field[5]->store(f_key_info->referenced_key_name->str, 
                               f_key_info->referenced_key_name->length, cs);
        table->field[5]->set_notnull();
      }
      else
        table->field[5]->set_null();
      table->field[6]->store(STRING_WITH_LEN("NONE"), cs);
      table->field[7]->store(f_key_info->update_method->str, 
                             f_key_info->update_method->length, cs);
      table->field[8]->store(f_key_info->delete_method->str, 
                             f_key_info->delete_method->length, cs);
      if (schema_table_store_record(thd, table))
        return(1);
    }
  }
  return(0);
}


struct schema_table_ref 
{
  const char *table_name;
  ST_SCHEMA_TABLE *schema_table;
};


/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table_in_plugin()
    thd                 thread handler
    plugin              plugin
    table_name          table name

  RETURN
    0	table not found
    1   found the schema table
*/
static my_bool find_schema_table_in_plugin(THD *thd, plugin_ref plugin,
                                           void* p_table)
{
  schema_table_ref *p_schema_table= (schema_table_ref *)p_table;
  const char* table_name= p_schema_table->table_name;
  ST_SCHEMA_TABLE *schema_table= plugin_data(plugin, ST_SCHEMA_TABLE *);

  if (!my_strcasecmp(system_charset_info,
                     schema_table->table_name,
                     table_name)) {
    p_schema_table->schema_table= schema_table;
    return(1);
  }

  return(0);
}


/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table()
    thd                 thread handler
    table_name          table name

  RETURN
    0	table not found
    #   pointer to 'schema_tables' element
*/

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char* table_name)
{
  schema_table_ref schema_table_a;
  ST_SCHEMA_TABLE *schema_table= schema_tables;

  for (; schema_table->table_name; schema_table++)
  {
    if (!my_strcasecmp(system_charset_info,
                       schema_table->table_name,
                       table_name))
      return(schema_table);
  }

  schema_table_a.table_name= table_name;
  if (plugin_foreach(thd, find_schema_table_in_plugin, 
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &schema_table_a))
    return(schema_table_a.schema_table);

  return(NULL);
}


ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx)
{
  return &schema_tables[schema_table_idx];
}


/**
  Create information_schema table using schema_table data.

  @note

  @param
    thd	       	          thread handler

  @param table_list Used to pass I_S table information(fields info, tables
  parameters etc) and table name.

  @retval  \#             Pointer to created table
  @retval  NULL           Can't create table
*/

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list)
{
  int field_count= 0;
  Item *item;
  TABLE *table;
  List<Item> field_list;
  ST_SCHEMA_TABLE *schema_table= table_list->schema_table;
  ST_FIELD_INFO *fields_info= schema_table->fields_info;
  CHARSET_INFO *cs= system_charset_info;

  for (; fields_info->field_name; fields_info++)
  {
    switch (fields_info->field_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONGLONG:
      if (!(item= new Item_return_int(fields_info->field_name,
                                      fields_info->field_length,
                                      fields_info->field_type,
                                      fields_info->value)))
      {
        return(0);
      }
      item->unsigned_flag= (fields_info->field_flags & MY_I_S_UNSIGNED);
      break;
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
      if (!(item=new Item_return_date_time(fields_info->field_name,
                                           fields_info->field_type)))
      {
        return(0);
      }
      break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      if ((item= new Item_float(fields_info->field_name, 0.0, NOT_FIXED_DEC, 
                           fields_info->field_length)) == NULL)
        return(NULL);
      break;
    case MYSQL_TYPE_NEWDECIMAL:
      if (!(item= new Item_decimal((longlong) fields_info->value, false)))
      {
        return(0);
      }
      item->unsigned_flag= (fields_info->field_flags & MY_I_S_UNSIGNED);
      item->decimals= fields_info->field_length%10;
      item->max_length= (fields_info->field_length/100)%100;
      if (item->unsigned_flag == 0)
        item->max_length+= 1;
      if (item->decimals > 0)
        item->max_length+= 1;
      item->set_name(fields_info->field_name,
                     strlen(fields_info->field_name), cs);
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      if (!(item= new Item_blob(fields_info->field_name,
                                fields_info->field_length)))
      {
        return(0);
      }
      break;
    default:
      /* Don't let unimplemented types pass through. Could be a grave error. */
      DBUG_ASSERT(fields_info->field_type == MYSQL_TYPE_STRING);

      if (!(item= new Item_empty_string("", fields_info->field_length, cs)))
      {
        return(0);
      }
      item->set_name(fields_info->field_name,
                     strlen(fields_info->field_name), cs);
      break;
    }
    field_list.push_back(item);
    item->maybe_null= (fields_info->field_flags & MY_I_S_MAYBE_NULL);
    field_count++;
  }
  TMP_TABLE_PARAM *tmp_table_param =
    (TMP_TABLE_PARAM*) (thd->alloc(sizeof(TMP_TABLE_PARAM)));
  tmp_table_param->init();
  tmp_table_param->table_charset= cs;
  tmp_table_param->field_count= field_count;
  tmp_table_param->schema_table= 1;
  SELECT_LEX *select_lex= thd->lex->current_select;
  if (!(table= create_tmp_table(thd, tmp_table_param,
                                field_list, (ORDER*) 0, 0, 0, 
                                (select_lex->options | thd->options |
                                 TMP_TABLE_ALL_COLUMNS),
                                HA_POS_ERROR, table_list->alias)))
    return(0);
  my_bitmap_map* bitmaps=
    (my_bitmap_map*) thd->alloc(bitmap_buffer_size(field_count));
  bitmap_init(&table->def_read_set, (my_bitmap_map*) bitmaps, field_count,
              FALSE);
  table->read_set= &table->def_read_set;
  bitmap_clear_all(table->read_set);
  table_list->schema_table_param= tmp_table_param;
  return(table);
}


/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    make_old_format()
    thd			thread handler
    schema_table        pointer to 'schema_tables' element

  RETURN
   1	error
   0	success
*/

int make_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  ST_FIELD_INFO *field_info= schema_table->fields_info;
  Name_resolution_context *context= &thd->lex->select_lex.context;
  for (; field_info->field_name; field_info++)
  {
    if (field_info->old_name)
    {
      Item_field *field= new Item_field(context,
                                        NullS, NullS, field_info->field_name);
      if (field)
      {
        field->set_name(field_info->old_name,
                        strlen(field_info->old_name),
                        system_charset_info);
        if (add_item_to_list(thd, field))
          return 1;
      }
    }
  }
  return 0;
}


int make_schemata_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  LEX *lex= thd->lex;
  SELECT_LEX *sel= lex->current_select;
  Name_resolution_context *context= &sel->context;

  if (!sel->item_list.elements)
  {
    ST_FIELD_INFO *field_info= &schema_table->fields_info[1];
    String buffer(tmp,sizeof(tmp), system_charset_info);
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (!field || add_item_to_list(thd, field))
      return 1;
    buffer.length(0);
    buffer.append(field_info->old_name);
    if (lex->wild && lex->wild->ptr())
    {
      buffer.append(STRING_WITH_LEN(" ("));
      buffer.append(lex->wild->ptr());
      buffer.append(')');
    }
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  }
  return 0;
}


int make_table_names_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  String buffer(tmp,sizeof(tmp), thd->charset());
  LEX *lex= thd->lex;
  Name_resolution_context *context= &lex->select_lex.context;

  ST_FIELD_INFO *field_info= &schema_table->fields_info[2];
  buffer.length(0);
  buffer.append(field_info->old_name);
  buffer.append(lex->select_lex.db);
  if (lex->wild && lex->wild->ptr())
  {
    buffer.append(STRING_WITH_LEN(" ("));
    buffer.append(lex->wild->ptr());
    buffer.append(')');
  }
  Item_field *field= new Item_field(context,
                                    NullS, NullS, field_info->field_name);
  if (add_item_to_list(thd, field))
    return 1;
  field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  if (thd->lex->verbose)
  {
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
    field_info= &schema_table->fields_info[3];
    field= new Item_field(context, NullS, NullS, field_info->field_name);
    if (add_item_to_list(thd, field))
      return 1;
    field->set_name(field_info->old_name, strlen(field_info->old_name),
                    system_charset_info);
  }
  return 0;
}


int make_columns_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int fields_arr[]= {3, 14, 13, 6, 15, 5, 16, 17, 18, -1};
  int *field_num= fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context= &thd->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    field_info= &schema_table->fields_info[*field_num];
    if (!thd->lex->verbose && (*field_num == 13 ||
                               *field_num == 17 ||
                               *field_num == 18))
      continue;
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (field)
    {
      field->set_name(field_info->old_name,
                      strlen(field_info->old_name),
                      system_charset_info);
      if (add_item_to_list(thd, field))
        return 1;
    }
  }
  return 0;
}


int make_character_sets_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int fields_arr[]= {0, 2, 1, 3, -1};
  int *field_num= fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context= &thd->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    field_info= &schema_table->fields_info[*field_num];
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (field)
    {
      field->set_name(field_info->old_name,
                      strlen(field_info->old_name),
                      system_charset_info);
      if (add_item_to_list(thd, field))
        return 1;
    }
  }
  return 0;
}


/*
  Create information_schema table

  SYNOPSIS
  mysql_schema_table()
    thd                thread handler
    lex                pointer to LEX
    table_list         pointer to table_list

  RETURN
    0	success
    1   error
*/

int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list)
{
  TABLE *table;
  if (!(table= table_list->schema_table->create_table(thd, table_list)))
    return(1);
  table->s->tmp_table= SYSTEM_TMP_TABLE;
  /*
    This test is necessary to make
    case insensitive file systems +
    upper case table names(information schema tables) +
    views
    working correctly
  */
  if (table_list->schema_table_name)
    table->alias_name_used= my_strcasecmp(table_alias_charset,
                                          table_list->schema_table_name,
                                          table_list->alias);
  table_list->table_name= table->s->table_name.str;
  table_list->table_name_length= table->s->table_name.length;
  table_list->table= table;
  table->next= thd->derived_tables;
  thd->derived_tables= table;
  table_list->select_lex->options |= OPTION_SCHEMA_TABLE;

  if (table_list->schema_table_reformed) // show command
  {
    SELECT_LEX *sel= lex->current_select;
    Item *item;
    Field_translator *transl, *org_transl;

    if (table_list->field_translation)
    {
      Field_translator *end= table_list->field_translation_end;
      for (transl= table_list->field_translation; transl < end; transl++)
      {
        if (!transl->item->fixed &&
            transl->item->fix_fields(thd, &transl->item))
          return(1);
      }
      return(0);
    }
    List_iterator_fast<Item> it(sel->item_list);
    if (!(transl=
          (Field_translator*)(thd->stmt_arena->
                              alloc(sel->item_list.elements *
                                    sizeof(Field_translator)))))
    {
      return(1);
    }
    for (org_transl= transl; (item= it++); transl++)
    {
      transl->item= item;
      transl->name= item->name;
      if (!item->fixed && item->fix_fields(thd, &transl->item))
      {
        return(1);
      }
    }
    table_list->field_translation= org_transl;
    table_list->field_translation_end= transl;
  }

  return(0);
}


/*
  Generate select from information_schema table

  SYNOPSIS
    make_schema_select()
    thd                  thread handler
    sel                  pointer to SELECT_LEX
    schema_table_idx     index of 'schema_tables' element

  RETURN
    0	success
    1   error
*/

int make_schema_select(THD *thd, SELECT_LEX *sel,
		       enum enum_schema_tables schema_table_idx)
{
  ST_SCHEMA_TABLE *schema_table= get_schema_table(schema_table_idx);
  LEX_STRING db, table;
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  thd->make_lex_string(&db, INFORMATION_SCHEMA_NAME.str,
                       INFORMATION_SCHEMA_NAME.length, 0);
  thd->make_lex_string(&table, schema_table->table_name,
                       strlen(schema_table->table_name), 0);
  if (schema_table->old_format(thd, schema_table) ||   /* Handle old syntax */
      !sel->add_table_to_list(thd, new Table_ident(thd, db, table, 0),
                              0, 0, TL_READ))
  {
    return(1);
  }
  return(0);
}


/*
  Fill temporary schema tables before SELECT

  SYNOPSIS
    get_schema_tables_result()
    join  join which use schema tables
    executed_place place where I_S table processed

  RETURN
    FALSE success
    TRUE  error
*/

bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place)
{
  JOIN_TAB *tmp_join_tab= join->join_tab+join->tables;
  THD *thd= join->thd;
  LEX *lex= thd->lex;
  bool result= 0;

  thd->no_warnings_for_error= 1;
  for (JOIN_TAB *tab= join->join_tab; tab < tmp_join_tab; tab++)
  {
    if (!tab->table || !tab->table->pos_in_table_list)
      break;

    TABLE_LIST *table_list= tab->table->pos_in_table_list;
    if (table_list->schema_table)
    {
      bool is_subselect= (&lex->unit != lex->current_select->master_unit() &&
                          lex->current_select->master_unit()->item);


      /* skip I_S optimizations specific to get_all_tables */
      if (thd->lex->describe &&
          (table_list->schema_table->fill_table != get_all_tables))
        continue;

      /*
        If schema table is already processed and
        the statement is not a subselect then
        we don't need to fill this table again.
        If schema table is already processed and
        schema_table_state != executed_place then
        table is already processed and
        we should skip second data processing.
      */
      if (table_list->schema_table_state &&
          (!is_subselect || table_list->schema_table_state != executed_place))
        continue;

      /*
        if table is used in a subselect and
        table has been processed earlier with the same
        'executed_place' value then we should refresh the table.
      */
      if (table_list->schema_table_state && is_subselect)
      {
        table_list->table->file->extra(HA_EXTRA_NO_CACHE);
        table_list->table->file->extra(HA_EXTRA_RESET_STATE);
        table_list->table->file->ha_delete_all_rows();
        free_io_cache(table_list->table);
        filesort_free_buffers(table_list->table,1);
        table_list->table->null_row= 0;
      }
      else
        table_list->table->file->stats.records= 0;

      if (table_list->schema_table->fill_table(thd, table_list,
                                               tab->select_cond))
      {
        result= 1;
        join->error= 1;
        tab->read_record.file= table_list->table->file;
        table_list->schema_table_state= executed_place;
        break;
      }
      tab->read_record.file= table_list->table->file;
      table_list->schema_table_state= executed_place;
    }
  }
  thd->no_warnings_for_error= 0;
  return(result);
}

ST_FIELD_INFO schema_fields_info[]=
{
  {"CATALOG_NAME", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"SCHEMA_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database",
   SKIP_OPEN_TABLE},
  {"DEFAULT_CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"DEFAULT_COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"SQL_PATH", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO tables_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"TABLE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"ENGINE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Engine", OPEN_FRM_ONLY},
  {"VERSION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Version", OPEN_FRM_ONLY},
  {"ROW_FORMAT", 10, MYSQL_TYPE_STRING, 0, 1, "Row_format", OPEN_FULL_TABLE},
  {"TABLE_ROWS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Rows", OPEN_FULL_TABLE},
  {"AVG_ROW_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Avg_row_length", OPEN_FULL_TABLE},
  {"DATA_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_length", OPEN_FULL_TABLE},
  {"MAX_DATA_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Max_data_length", OPEN_FULL_TABLE},
  {"INDEX_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Index_length", OPEN_FULL_TABLE},
  {"DATA_FREE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_free", OPEN_FULL_TABLE},
  {"AUTO_INCREMENT", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Auto_increment", OPEN_FULL_TABLE},
  {"CREATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Create_time", OPEN_FULL_TABLE},
  {"UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Update_time", OPEN_FULL_TABLE},
  {"CHECK_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Check_time", OPEN_FULL_TABLE},
  {"TABLE_COLLATION", 64, MYSQL_TYPE_STRING, 0, 1, "Collation", OPEN_FRM_ONLY},
  {"CHECKSUM", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Checksum", OPEN_FULL_TABLE},
  {"CREATE_OPTIONS", 255, MYSQL_TYPE_STRING, 0, 1, "Create_options",
   OPEN_FRM_ONLY},
  {"TABLE_COMMENT", TABLE_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, "Comment", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO columns_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FRM_ONLY},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Field",
   OPEN_FRM_ONLY},
  {"ORDINAL_POSITION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_UNSIGNED, 0, OPEN_FRM_ONLY},
  {"COLUMN_DEFAULT", MAX_FIELD_VARCHARLENGTH, MYSQL_TYPE_STRING, 0,
   1, "Default", OPEN_FRM_ONLY},
  {"IS_NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
  {"DATA_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"CHARACTER_MAXIMUM_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"CHARACTER_OCTET_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"NUMERIC_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"NUMERIC_SCALE", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FRM_ONLY},
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 1, "Collation", OPEN_FRM_ONLY},
  {"COLUMN_TYPE", 65535, MYSQL_TYPE_STRING, 0, 0, "Type", OPEN_FRM_ONLY},
  {"COLUMN_KEY", 3, MYSQL_TYPE_STRING, 0, 0, "Key", OPEN_FRM_ONLY},
  {"EXTRA", 27, MYSQL_TYPE_STRING, 0, 0, "Extra", OPEN_FRM_ONLY},
  {"PRIVILEGES", 80, MYSQL_TYPE_STRING, 0, 0, "Privileges", OPEN_FRM_ONLY},
  {"COLUMN_COMMENT", COLUMN_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, "Comment", OPEN_FRM_ONLY},
  {"STORAGE", 8, MYSQL_TYPE_STRING, 0, 0, "Storage", OPEN_FRM_ONLY},
  {"FORMAT", 8, MYSQL_TYPE_STRING, 0, 0, "Format", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO charsets_fields_info[]=
{
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Charset",
   SKIP_OPEN_TABLE},
  {"DEFAULT_COLLATE_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Default collation",
   SKIP_OPEN_TABLE},
  {"DESCRIPTION", 60, MYSQL_TYPE_STRING, 0, 0, "Description",
   SKIP_OPEN_TABLE},
  {"MAXLEN", 3, MYSQL_TYPE_LONGLONG, 0, 0, "Maxlen", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO collation_fields_info[]=
{
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Collation", SKIP_OPEN_TABLE},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Charset",
   SKIP_OPEN_TABLE},
  {"ID", MY_INT32_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, "Id",
   SKIP_OPEN_TABLE},
  {"IS_DEFAULT", 3, MYSQL_TYPE_STRING, 0, 0, "Default", SKIP_OPEN_TABLE},
  {"IS_COMPILED", 3, MYSQL_TYPE_STRING, 0, 0, "Compiled", SKIP_OPEN_TABLE},
  {"SORTLEN", 3, MYSQL_TYPE_LONGLONG, 0, 0, "Sortlen", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO events_fields_info[]=
{
  {"EVENT_CATALOG", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EVENT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Db",
   SKIP_OPEN_TABLE},
  {"EVENT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"DEFINER", 77, MYSQL_TYPE_STRING, 0, 0, "Definer", SKIP_OPEN_TABLE},
  {"TIME_ZONE", 64, MYSQL_TYPE_STRING, 0, 0, "Time zone", SKIP_OPEN_TABLE},
  {"EVENT_BODY", 8, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EVENT_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EVENT_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
  {"EXECUTE_AT", 0, MYSQL_TYPE_DATETIME, 0, 1, "Execute at", SKIP_OPEN_TABLE},
  {"INTERVAL_VALUE", 256, MYSQL_TYPE_STRING, 0, 1, "Interval value",
   SKIP_OPEN_TABLE},
  {"INTERVAL_FIELD", 18, MYSQL_TYPE_STRING, 0, 1, "Interval field",
   SKIP_OPEN_TABLE},
  {"SQL_MODE", 65535, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"STARTS", 0, MYSQL_TYPE_DATETIME, 0, 1, "Starts", SKIP_OPEN_TABLE},
  {"ENDS", 0, MYSQL_TYPE_DATETIME, 0, 1, "Ends", SKIP_OPEN_TABLE},
  {"STATUS", 18, MYSQL_TYPE_STRING, 0, 0, "Status", SKIP_OPEN_TABLE},
  {"ON_COMPLETION", 12, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"CREATED", 0, MYSQL_TYPE_DATETIME, 0, 0, 0, SKIP_OPEN_TABLE},
  {"LAST_ALTERED", 0, MYSQL_TYPE_DATETIME, 0, 0, 0, SKIP_OPEN_TABLE},
  {"LAST_EXECUTED", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EVENT_COMMENT", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ORIGINATOR", 10, MYSQL_TYPE_LONGLONG, 0, 0, "Originator", SKIP_OPEN_TABLE},
  {"CHARACTER_SET_CLIENT", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "character_set_client", SKIP_OPEN_TABLE},
  {"COLLATION_CONNECTION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "collation_connection", SKIP_OPEN_TABLE},
  {"DATABASE_COLLATION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "Database Collation", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};



ST_FIELD_INFO coll_charset_app_fields_info[]=
{
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO stat_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FRM_ONLY},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", OPEN_FRM_ONLY},
  {"NON_UNIQUE", 1, MYSQL_TYPE_LONGLONG, 0, 0, "Non_unique", OPEN_FRM_ONLY},
  {"INDEX_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"INDEX_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Key_name",
   OPEN_FRM_ONLY},
  {"SEQ_IN_INDEX", 2, MYSQL_TYPE_LONGLONG, 0, 0, "Seq_in_index", OPEN_FRM_ONLY},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Column_name",
   OPEN_FRM_ONLY},
  {"COLLATION", 1, MYSQL_TYPE_STRING, 0, 1, "Collation", OPEN_FRM_ONLY},
  {"CARDINALITY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 1,
   "Cardinality", OPEN_FULL_TABLE},
  {"SUB_PART", 3, MYSQL_TYPE_LONGLONG, 0, 1, "Sub_part", OPEN_FRM_ONLY},
  {"PACKED", 10, MYSQL_TYPE_STRING, 0, 1, "Packed", OPEN_FRM_ONLY},
  {"NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
  {"INDEX_TYPE", 16, MYSQL_TYPE_STRING, 0, 0, "Index_type", OPEN_FULL_TABLE},
  {"COMMENT", 16, MYSQL_TYPE_STRING, 0, 1, "Comment", OPEN_FRM_ONLY},
  {"INDEX_COMMENT", INDEX_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, "Index_Comment", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO user_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO schema_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO column_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_constraints_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO key_column_usage_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ORDINAL_POSITION", 10 ,MYSQL_TYPE_LONGLONG, 0, 0, 0, OPEN_FULL_TABLE},
  {"POSITION_IN_UNIQUE_CONSTRAINT", 10 ,MYSQL_TYPE_LONGLONG, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_names_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Tables_in_",
   SKIP_OPEN_TABLE},
  {"TABLE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table_type",
   OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO open_tables_fields_info[]=
{
  {"Database", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database",
   SKIP_OPEN_TABLE},
  {"Table",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", SKIP_OPEN_TABLE},
  {"In_use", 1, MYSQL_TYPE_LONGLONG, 0, 0, "In_use", SKIP_OPEN_TABLE},
  {"Name_locked", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Name_locked", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO variables_fields_info[]=
{
  {"VARIABLE_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Variable_name",
   SKIP_OPEN_TABLE},
  {"VARIABLE_VALUE", 16300, MYSQL_TYPE_STRING, 0, 1, "Value", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO processlist_fields_info[]=
{
  {"ID", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Id", SKIP_OPEN_TABLE},
  {"USER", 16, MYSQL_TYPE_STRING, 0, 0, "User", SKIP_OPEN_TABLE},
  {"HOST", LIST_PROCESS_HOST_LEN,  MYSQL_TYPE_STRING, 0, 0, "Host",
   SKIP_OPEN_TABLE},
  {"DB", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Db", SKIP_OPEN_TABLE},
  {"COMMAND", 16, MYSQL_TYPE_STRING, 0, 0, "Command", SKIP_OPEN_TABLE},
  {"TIME", 7, MYSQL_TYPE_LONGLONG, 0, 0, "Time", SKIP_OPEN_TABLE},
  {"STATE", 64, MYSQL_TYPE_STRING, 0, 1, "State", SKIP_OPEN_TABLE},
  {"INFO", PROCESS_LIST_INFO_WIDTH, MYSQL_TYPE_STRING, 0, 1, "Info",
   SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO plugin_fields_info[]=
{
  {"PLUGIN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"PLUGIN_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_STATUS", 10, MYSQL_TYPE_STRING, 0, 0, "Status", SKIP_OPEN_TABLE},
  {"PLUGIN_TYPE", 80, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
  {"PLUGIN_TYPE_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_LIBRARY", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Library",
   SKIP_OPEN_TABLE},
  {"PLUGIN_LIBRARY_VERSION", 20, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_AUTHOR", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_LICENSE", 80, MYSQL_TYPE_STRING, 0, 1, "License", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};

ST_FIELD_INFO referential_constraints_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0,
   MY_I_S_MAYBE_NULL, 0, OPEN_FULL_TABLE},
  {"MATCH_OPTION", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"UPDATE_RULE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"DELETE_RULE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO parameters_fields_info[]=
{
  {"SPECIFIC_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"SPECIFIC_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"SPECIFIC_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ORDINAL_POSITION", 21 , MYSQL_TYPE_LONG, 0, 0, 0, OPEN_FULL_TABLE},
  {"PARAMETER_MODE", 5, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"PARAMETER_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"DATA_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CHARACTER_MAXIMUM_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"CHARACTER_OCTET_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"NUMERIC_PRECISION", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"NUMERIC_SCALE", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"DTD_IDENTIFIER", 65535, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ROUTINE_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE}
};


/*
  Description of ST_FIELD_INFO in table.h

  Make sure that the order of schema_tables and enum_schema_tables are the same.

*/

ST_SCHEMA_TABLE schema_tables[]=
{
  {"CHARACTER_SETS", charsets_fields_info, create_schema_table, 
   fill_schema_charsets, make_character_sets_old_format, 0, -1, -1, 0, 0},
  {"COLLATIONS", collation_fields_info, create_schema_table, 
   fill_schema_collation, make_old_format, 0, -1, -1, 0, 0},
  {"COLLATION_CHARACTER_SET_APPLICABILITY", coll_charset_app_fields_info,
   create_schema_table, fill_schema_coll_charset_app, 0, 0, -1, -1, 0, 0},
  {"COLUMNS", columns_fields_info, create_schema_table, 
   get_all_tables, make_columns_old_format, get_schema_column_record, 1, 2, 0,
   OPTIMIZE_I_S_TABLE|OPEN_VIEW_FULL},
  {"GLOBAL_STATUS", variables_fields_info, create_schema_table,
   fill_status, make_old_format, 0, -1, -1, 0, 0},
  {"GLOBAL_VARIABLES", variables_fields_info, create_schema_table,
   fill_variables, make_old_format, 0, -1, -1, 0, 0},
  {"KEY_COLUMN_USAGE", key_column_usage_fields_info, create_schema_table,
   get_all_tables, 0, get_schema_key_column_usage_record, 4, 5, 0,
   OPEN_TABLE_ONLY},
  {"OPEN_TABLES", open_tables_fields_info, create_schema_table,
   fill_open_tables, make_old_format, 0, -1, -1, 1, 0},
  {"PLUGINS", plugin_fields_info, create_schema_table,
   fill_plugins, make_old_format, 0, -1, -1, 0, 0},
  {"PROCESSLIST", processlist_fields_info, create_schema_table,
   fill_schema_processlist, make_old_format, 0, -1, -1, 0, 0},
  {"REFERENTIAL_CONSTRAINTS", referential_constraints_fields_info,
   create_schema_table, get_all_tables, 0, get_referential_constraints_record,
   1, 9, 0, OPEN_TABLE_ONLY},
  {"SCHEMATA", schema_fields_info, create_schema_table,
   fill_schema_schemata, make_schemata_old_format, 0, 1, -1, 0, 0},
  {"SESSION_STATUS", variables_fields_info, create_schema_table,
   fill_status, make_old_format, 0, -1, -1, 0, 0},
  {"SESSION_VARIABLES", variables_fields_info, create_schema_table,
   fill_variables, make_old_format, 0, -1, -1, 0, 0},
  {"STATISTICS", stat_fields_info, create_schema_table, 
   get_all_tables, make_old_format, get_schema_stat_record, 1, 2, 0,
   OPEN_TABLE_ONLY|OPTIMIZE_I_S_TABLE},
  {"STATUS", variables_fields_info, create_schema_table, fill_status, 
   make_old_format, 0, -1, -1, 1, 0},
  {"TABLES", tables_fields_info, create_schema_table, 
   get_all_tables, make_old_format, get_schema_tables_record, 1, 2, 0,
   OPTIMIZE_I_S_TABLE},
  {"TABLE_CONSTRAINTS", table_constraints_fields_info, create_schema_table,
   get_all_tables, 0, get_schema_constraints_record, 3, 4, 0, OPEN_TABLE_ONLY},
  {"TABLE_NAMES", table_names_fields_info, create_schema_table,
   get_all_tables, make_table_names_old_format, 0, 1, 2, 1, 0},
  {"VARIABLES", variables_fields_info, create_schema_table, fill_variables,
   make_old_format, 0, -1, -1, 1, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator_fast<char>;
template class List<char>;
#endif

int initialize_schema_table(st_plugin_int *plugin)
{
  ST_SCHEMA_TABLE *schema_table;

  if (!(schema_table= (ST_SCHEMA_TABLE *)my_malloc(sizeof(ST_SCHEMA_TABLE),
                                MYF(MY_WME | MY_ZEROFILL))))
      return(1);
  /* Historical Requirement */
  plugin->data= schema_table; // shortcut for the future
  if (plugin->plugin->init)
  {
    schema_table->create_table= create_schema_table;
    schema_table->old_format= make_old_format;
    schema_table->idx_field1= -1, 
    schema_table->idx_field2= -1; 

    /* Make the name available to the init() function. */
    schema_table->table_name= plugin->name.str;

    if (plugin->plugin->init(schema_table))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
    
    /* Make sure the plugin name is not set inside the init() function. */
    schema_table->table_name= plugin->name.str;
  }

  return(0);
err:
  my_free(schema_table, MYF(0));
  return(1);
}

int finalize_schema_table(st_plugin_int *plugin)
{
  ST_SCHEMA_TABLE *schema_table= (ST_SCHEMA_TABLE *)plugin->data;

  if (schema_table && plugin->plugin->deinit)
    my_free(schema_table, MYF(0));

  return(0);
}
