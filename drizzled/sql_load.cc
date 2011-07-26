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


/* Copy data from a textfile to table */

#include <config.h>

#include <drizzled/sql_load.h>
#include <drizzled/error.h>
#include <drizzled/data_home.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/field/epoch.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/sql_lex.h>
#include <drizzled/copy_info.h>
#include <drizzled/file_exchange.h>
#include <drizzled/util/test.h>
#include <drizzled/session/transactions.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <climits>
#include <boost/filesystem.hpp>

namespace fs=boost::filesystem;
using namespace std;
namespace drizzled
{

class READ_INFO {
  int	cursor;
  unsigned char	*buffer;                /* Buffer for read text */
  unsigned char *end_of_buff;           /* Data in bufferts ends here */
  size_t buff_length;                   /* Length of buffert */
  size_t max_length;                    /* Max length of row */
  char	*field_term_ptr,*line_term_ptr,*line_start_ptr,*line_start_end;
  uint	field_term_length,line_term_length,enclosed_length;
  int	field_term_char,line_term_char,enclosed_char,escape_char;
  int	*stack,*stack_pos;
  bool	found_end_of_line,start_of_line,eof;
  bool  need_end_io_cache;
  internal::io_cache_st cache;

public:
  bool error,line_cuted,found_null,enclosed;
  unsigned char	*row_start,			/* Found row starts here */
	*row_end;			/* Found row ends here */
  const charset_info_st *read_charset;

  READ_INFO(int cursor, size_t tot_length, const charset_info_st * const cs,
	    String &field_term,String &line_start,String &line_term,
	    String &enclosed,int escape, bool is_fifo);
  ~READ_INFO();
  int read_field();
  int read_fixed_length(void);
  int next_line(void);
  char unescape(char chr);
  int terminator(char *ptr,uint32_t length);
  bool find_start_of_fields();

  /*
    We need to force cache close before destructor is invoked to log
    the last read block
  */
  void end_io_cache()
  {
    cache.end_io_cache();
    need_end_io_cache = 0;
  }

  /*
    Either this method, or we need to make cache public
    Arg must be set from load() since constructor does not see
    either the table or Session value
  */
  void set_io_cache_arg(void* arg) { cache.arg = arg; }
};

static int read_fixed_length(Session *session, CopyInfo &info, TableList *table_list,
                             List<Item> &fields_vars, List<Item> &set_fields,
                             List<Item> &set_values, READ_INFO &read_info,
			     uint32_t skip_lines,
			     bool ignore_check_option_errors);
static int read_sep_field(Session *session, CopyInfo &info, TableList *table_list,
                          List<Item> &fields_vars, List<Item> &set_fields,
                          List<Item> &set_values, READ_INFO &read_info,
			  String &enclosed, uint32_t skip_lines,
			  bool ignore_check_option_errors);


/*
  Execute LOAD DATA query

  SYNOPSYS
    load()
      session - current thread
      ex  - file_exchange object representing source cursor and its parsing rules
      table_list  - list of tables to which we are loading data
      fields_vars - list of fields and variables to which we read
                    data from cursor
      set_fields  - list of fields mentioned in set clause
      set_values  - expressions to assign to fields in previous list
      handle_duplicates - indicates whenever we should emit error or
                          replace row if we will meet duplicates.
      ignore -          - indicates whenever we should ignore duplicates

  RETURN VALUES
    true - error / false - success
*/

int load(Session *session,file_exchange *ex,TableList *table_list,
	        List<Item> &fields_vars, List<Item> &set_fields,
                List<Item> &set_values,
                enum enum_duplicates handle_duplicates, bool ignore)
{
  int file;
  Table *table= NULL;
  int error;
  String *field_term=ex->field_term,*escaped=ex->escaped;
  String *enclosed=ex->enclosed;
  bool is_fifo=0;

  assert(table_list->getSchemaName()); // This should never be null

  /*
    If path for cursor is not defined, we will use the current database.
    If this is not set, we will use the directory where the table to be
    loaded is located
  */
  util::string::ptr schema(session->schema());
  const char *tdb= (schema and not schema->empty()) ? schema->c_str() : table_list->getSchemaName(); // Result should never be null
  assert(tdb);
  uint32_t skip_lines= ex->skip_lines;
  bool transactional_table;

  /* Escape and enclosed character may be a utf8 4-byte character */
  if (escaped->length() > 4 || enclosed->length() > 4)
  {
    my_error(ER_WRONG_FIELD_TERMINATORS,MYF(0),enclosed->c_ptr(), enclosed->length());
    return true;
  }

  if (session->openTablesLock(table_list))
    return true;

  if (setup_tables_and_check_access(session, &session->lex().select_lex.context,
                                    &session->lex().select_lex.top_join_list,
                                    table_list,
                                    &session->lex().select_lex.leaf_tables, true))
     return(-1);

  /*
    Let us emit an error if we are loading data to table which is used
    in subselect in SET clause like we do it for INSERT.

    The main thing to fix to remove this restriction is to ensure that the
    table is marked to be 'used for insert' in which case we should never
    mark this table as 'const table' (ie, one that has only one row).
  */
  if (unique_table(table_list, table_list->next_global))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->getTableName());
    return true;
  }

  table= table_list->table;
  transactional_table= table->cursor->has_transactions();

  if (!fields_vars.size())
  {
    Field **field;
    for (field= table->getFields(); *field ; field++)
      fields_vars.push_back(new Item_field(*field));
    table->setWriteSet();
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    /*
      Let us also prepare SET clause, altough it is probably empty
      in this case.
    */
    if (setup_fields(session, 0, set_fields, MARK_COLUMNS_WRITE, 0, 0) ||
        setup_fields(session, 0, set_values, MARK_COLUMNS_READ, 0, 0))
      return true;
  }
  else
  {						// Part field list
    /* TODO: use this conds for 'WITH CHECK OPTIONS' */
    if (setup_fields(session, 0, fields_vars, MARK_COLUMNS_WRITE, 0, 0) ||
        setup_fields(session, 0, set_fields, MARK_COLUMNS_WRITE, 0, 0) ||
        check_that_all_fields_are_given_values(session, table, table_list))
      return true;
    /*
      Check whenever TIMESTAMP field with auto-set feature specified
      explicitly.
    */
    if (table->timestamp_field)
    {
      if (table->isWriteSet(table->timestamp_field->position()))
      {
        table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
      }
      else
      {
        table->setWriteSet(table->timestamp_field->position());
      }
    }
    /* Fix the expressions in SET clause */
    if (setup_fields(session, 0, set_values, MARK_COLUMNS_READ, 0, 0))
      return true;
  }

  table->mark_columns_needed_for_insert();

  size_t tot_length=0;
  bool use_blobs= 0, use_vars= 0;
  List<Item>::iterator it(fields_vars.begin());
  Item *item;

  while ((item= it++))
  {
    Item *real_item= item->real_item();

    if (real_item->type() == Item::FIELD_ITEM)
    {
      Field *field= ((Item_field*)real_item)->field;
      if (field->flags & BLOB_FLAG)
      {
        use_blobs= 1;
        tot_length+= 256;			// Will be extended if needed
      }
      else
        tot_length+= field->field_length;
    }
    else if (item->type() == Item::STRING_ITEM)
      use_vars= 1;
  }
  if (use_blobs && !ex->line_term->length() && !field_term->length())
  {
    my_message(ER_BLOBS_AND_NO_TERMINATED,ER(ER_BLOBS_AND_NO_TERMINATED),
	       MYF(0));
    return true;
  }
  if (use_vars && !field_term->length() && !enclosed->length())
  {
    my_error(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, MYF(0));
    return true;
  }

  fs::path to_file(ex->file_name);
  fs::path target_path(fs::system_complete(getDataHomeCatalog()));
  if (not to_file.has_root_directory())
  {
    int count_elements= 0;
    for (fs::path::iterator iter= to_file.begin();
         iter != to_file.end();
         ++iter, ++count_elements)
    { }

    if (count_elements == 1)
    {
      target_path /= tdb;
    }
    target_path /= to_file;
  }
  else
  {
    target_path= to_file;
  }

  if (not secure_file_priv.string().empty())
  {
    if (target_path.file_string().substr(0, secure_file_priv.file_string().size()) != secure_file_priv.file_string())
    {
      /* Read only allowed from within dir specified by secure_file_priv */
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
      return true;
    }
  }

  struct stat stat_info;
  if (stat(target_path.file_string().c_str(), &stat_info))
  {
    my_error(ER_FILE_NOT_FOUND, MYF(0), target_path.file_string().c_str(), errno);
    return true;
  }

  // if we are not in slave thread, the cursor must be:
  if (!((stat_info.st_mode & S_IROTH) == S_IROTH &&  // readable by others
        (stat_info.st_mode & S_IFLNK) != S_IFLNK && // and not a symlink
        ((stat_info.st_mode & S_IFREG) == S_IFREG ||
         (stat_info.st_mode & S_IFIFO) == S_IFIFO)))
  {
    my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), target_path.file_string().c_str());
    return true;
  }
  if ((stat_info.st_mode & S_IFIFO) == S_IFIFO)
    is_fifo = 1;


  if ((file=internal::my_open(target_path.file_string().c_str(), O_RDONLY,MYF(MY_WME))) < 0)
  {
    my_error(ER_CANT_OPEN_FILE, MYF(0), target_path.file_string().c_str(), errno);
    return true;
  }
  CopyInfo info;
  memset(&info, 0, sizeof(info));
  info.ignore= ignore;
  info.handle_duplicates=handle_duplicates;
  info.escape_char=escaped->length() ? (*escaped)[0] : INT_MAX;

  identifier::Schema identifier(*schema);
  READ_INFO read_info(file, tot_length,
                      ex->cs ? ex->cs : plugin::StorageEngine::getSchemaCollation(identifier),
		      *field_term, *ex->line_start, *ex->line_term, *enclosed,
		      info.escape_char, is_fifo);
  if (read_info.error)
  {
    if	(file >= 0)
      internal::my_close(file,MYF(0));			// no files in net reading
    return true;				// Can't allocate buffers
  }

  /*
   * Per the SQL standard, inserting NULL into a NOT NULL
   * field requires an error to be thrown.
   *
   * @NOTE
   *
   * NULL check and handling occurs in field_conv.cc
   */
  session->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  session->cuted_fields=0L;
  /* Skip lines if there is a line terminator */
  if (ex->line_term->length())
  {
    /* ex->skip_lines needs to be preserved for logging */
    while (skip_lines > 0)
    {
      skip_lines--;
      if (read_info.next_line())
	break;
    }
  }

  if (!(error=test(read_info.error)))
  {

    table->next_number_field=table->found_next_number_field;
    if (ignore ||
	handle_duplicates == DUP_REPLACE)
      table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (handle_duplicates == DUP_REPLACE)
        table->cursor->extra(HA_EXTRA_WRITE_CAN_REPLACE);
    table->cursor->ha_start_bulk_insert((ha_rows) 0);
    table->copy_blobs=1;

    session->setAbortOnWarning(true);

    if (!field_term->length() && !enclosed->length())
      error= read_fixed_length(session, info, table_list, fields_vars,
                               set_fields, set_values, read_info,
			       skip_lines, ignore);
    else
      error= read_sep_field(session, info, table_list, fields_vars,
                            set_fields, set_values, read_info,
			    *enclosed, skip_lines, ignore);
    if (table->cursor->ha_end_bulk_insert() && !error)
    {
      table->print_error(errno, MYF(0));
      error= 1;
    }
    table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->cursor->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    table->next_number_field=0;
  }
  if (file >= 0)
    internal::my_close(file,MYF(0));
  free_blobs(table);				/* if pack_blob was used */
  table->copy_blobs=0;
  session->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  /*
     simulated killing in the middle of per-row loop
     must be effective for binlogging
  */
  if (error) 
  {
    error= -1;				// Error on read
    goto err;
  }

  char msg[FN_REFLEN];
  snprintf(msg, sizeof(msg), ER(ER_LOAD_INFO), info.records, info.deleted,
	   (info.records - info.copied), session->cuted_fields);

  if (session->transaction.stmt.hasModifiedNonTransData())
    session->transaction.all.markModifiedNonTransData();

  /* ok to client sent only after binlog write and engine commit */
  session->my_ok(info.copied + info.deleted, 0, 0L, msg);
err:
  assert(transactional_table || !(info.copied || info.deleted) ||
              session->transaction.stmt.hasModifiedNonTransData());
  table->cursor->ha_release_auto_increment();
  table->auto_increment_field_not_null= false;
  session->setAbortOnWarning(false);

  return(error);
}


/****************************************************************************
** Read of rows of fixed size + optional garage + optonal newline
****************************************************************************/

static int
read_fixed_length(Session *session, CopyInfo &info, TableList *table_list,
                  List<Item> &fields_vars, List<Item> &set_fields,
                  List<Item> &set_values, READ_INFO &read_info,
                  uint32_t skip_lines, bool ignore_check_option_errors)
{
  List<Item>::iterator it(fields_vars.begin());
  Item_field *sql_field;
  Table *table= table_list->table;
  bool err;

  while (!read_info.read_fixed_length())
  {
    if (session->getKilled())
    {
      session->send_kill_message();
      return 1;
    }
    if (skip_lines)
    {
      /*
	We could implement this with a simple seek if:
	- We are not using DATA INFILE LOCAL
	- escape character is  ""
	- line starting prefix is ""
      */
      skip_lines--;
      continue;
    }
    it= fields_vars.begin();
    unsigned char *pos=read_info.row_start;
#ifdef HAVE_VALGRIND
    read_info.row_end[0]=0;
#endif

    table->restoreRecordAsDefault();
    /*
      There is no variables in fields_vars list in this format so
      this conversion is safe.
    */
    while ((sql_field= (Item_field*) it++))
    {
      Field *field= sql_field->field;
      if (field == table->next_number_field)
        table->auto_increment_field_not_null= true;
      /*
        No fields specified in fields_vars list can be null in this format.
        Mark field as not null, we should do this for each row because of
        restore_record...
      */
      field->set_notnull();

      if (pos == read_info.row_end)
      {
        session->cuted_fields++;			/* Not enough fields */
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_WARN_TOO_FEW_RECORDS,
                            ER(ER_WARN_TOO_FEW_RECORDS), session->row_count);

        if (not field->maybe_null() and field->is_timestamp())
            ((field::Epoch::pointer) field)->set_time();
      }
      else
      {
	uint32_t length;
	unsigned char save_chr;
	if ((length=(uint32_t) (read_info.row_end-pos)) >
	    field->field_length)
        {
	  length=field->field_length;
        }
	save_chr=pos[length];
        pos[length]='\0'; // Add temp null terminator for store()
        field->store((char*) pos,length,read_info.read_charset);
	pos[length]=save_chr;
	if ((pos+=length) > read_info.row_end)
	  pos= read_info.row_end;	/* Fills rest with space */
      }
    }
    if (pos != read_info.row_end)
    {
      session->cuted_fields++;			/* To long row */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER(ER_WARN_TOO_MANY_RECORDS), session->row_count);
    }

    if (session->getKilled() ||
        fill_record(session, set_fields, set_values,
                    ignore_check_option_errors))
      return 1;

    err= write_record(session, table, &info);
    table->auto_increment_field_not_null= false;
    if (err)
      return 1;

    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      session->cuted_fields++;			/* To long row */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER(ER_WARN_TOO_MANY_RECORDS), session->row_count);
    }
    session->row_count++;
  }
  return(test(read_info.error));
}



static int
read_sep_field(Session *session, CopyInfo &info, TableList *table_list,
               List<Item> &fields_vars, List<Item> &set_fields,
               List<Item> &set_values, READ_INFO &read_info,
	       String &enclosed, uint32_t skip_lines,
	       bool ignore_check_option_errors)
{
  List<Item>::iterator it(fields_vars.begin());
  Item *item;
  Table *table= table_list->table;
  uint32_t enclosed_length;
  bool err;

  enclosed_length=enclosed.length();

  for (;;it= fields_vars.begin())
  {
    if (session->getKilled())
    {
      session->send_kill_message();
      return 1;
    }

    table->restoreRecordAsDefault();

    while ((item= it++))
    {
      uint32_t length;
      unsigned char *pos;
      Item *real_item;

      if (read_info.read_field())
	break;

      /* If this line is to be skipped we don't want to fill field or var */
      if (skip_lines)
        continue;

      pos=read_info.row_start;
      length=(uint32_t) (read_info.row_end-pos);

      real_item= item->real_item();

      if ((!read_info.enclosed && (enclosed_length && length == 4 && !memcmp(pos, STRING_WITH_LEN("NULL")))) ||
	  (length == 1 && read_info.found_null))
      {

        if (real_item->type() == Item::FIELD_ITEM)
        {
          Field *field= ((Item_field *)real_item)->field;
          if (field->reset())
          {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0), field->field_name,
                     session->row_count);
            return 1;
          }
          field->set_null();
          if (not field->maybe_null())
          {
            if (field->is_timestamp())
            {
              ((field::Epoch::pointer) field)->set_time();
            }
            else if (field != table->next_number_field)
            {
              field->set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_NULL_TO_NOTNULL, 1);
            }
          }
	}
        else if (item->type() == Item::STRING_ITEM)
        {
          ((Item_user_var_as_out_param *)item)->set_null_value(
                                                  read_info.read_charset);
        }
        else
        {
          my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
          return 1;
        }

	continue;
      }

      if (real_item->type() == Item::FIELD_ITEM)
      {
        Field *field= ((Item_field *)real_item)->field;
        field->set_notnull();
        read_info.row_end[0]=0;			// Safe to change end marker
        if (field == table->next_number_field)
          table->auto_increment_field_not_null= true;
        field->store((char*) pos, length, read_info.read_charset);
      }
      else if (item->type() == Item::STRING_ITEM)
      {
        ((Item_user_var_as_out_param *)item)->set_value((char*) pos, length,
                                                        read_info.read_charset);
      }
      else
      {
        my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
        return 1;
      }
    }
    if (read_info.error)
      break;
    if (skip_lines)
    {
      skip_lines--;
      continue;
    }
    if (item)
    {
      /* Have not read any field, thus input cursor is simply ended */
      if (item == &fields_vars.front())
	break;
      for (; item ; item= it++)
      {
        Item *real_item= item->real_item();
        if (real_item->type() == Item::FIELD_ITEM)
        {
          Field *field= ((Item_field *)real_item)->field;
          if (field->reset())
          {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0),field->field_name,
                     session->row_count);
            return 1;
          }
          if (not field->maybe_null() and field->is_timestamp())
              ((field::Epoch::pointer) field)->set_time();
          /*
            QQ: We probably should not throw warning for each field.
            But how about intention to always have the same number
            of warnings in Session::cuted_fields (and get rid of cuted_fields
            in the end ?)
          */
          session->cuted_fields++;
          push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                              ER_WARN_TOO_FEW_RECORDS,
                              ER(ER_WARN_TOO_FEW_RECORDS), session->row_count);
        }
        else if (item->type() == Item::STRING_ITEM)
        {
          ((Item_user_var_as_out_param *)item)->set_null_value(
                                                  read_info.read_charset);
        }
        else
        {
          my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
          return 1;
        }
      }
    }

    if (session->getKilled() ||
        fill_record(session, set_fields, set_values,
                    ignore_check_option_errors))
      return 1;

    err= write_record(session, table, &info);
    table->auto_increment_field_not_null= false;
    if (err)
      return 1;
    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      session->cuted_fields++;			/* To long row */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS, ER(ER_WARN_TOO_MANY_RECORDS),
                          session->row_count);
      if (session->getKilled())
        return 1;
    }
    session->row_count++;
  }
  return(test(read_info.error));
}


/* Unescape all escape characters, mark \N as null */

char
READ_INFO::unescape(char chr)
{
  /* keep this switch synchornous with the ESCAPE_CHARS macro */
  switch(chr) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'r': return '\r';
  case 'b': return '\b';
  case '0': return 0;				// Ascii null
  case 'Z': return '\032';			// Win32 end of cursor
  case 'N': found_null=1;

    /* fall through */
  default:  return chr;
  }
}


/*
  Read a line using buffering
  If last line is empty (in line mode) then it isn't outputed
*/


READ_INFO::READ_INFO(int file_par, size_t tot_length,
                     const charset_info_st * const cs,
		     String &field_term, String &line_start, String &line_term,
		     String &enclosed_par, int escape, bool is_fifo)
  :cursor(file_par),escape_char(escape)
{
  read_charset= cs;
  field_term_ptr=(char*) field_term.ptr();
  field_term_length= field_term.length();
  line_term_ptr=(char*) line_term.ptr();
  line_term_length= line_term.length();
  if (line_start.length() == 0)
  {
    line_start_ptr=0;
    start_of_line= 0;
  }
  else
  {
    line_start_ptr=(char*) line_start.ptr();
    line_start_end=line_start_ptr+line_start.length();
    start_of_line= 1;
  }
  /* If field_terminator == line_terminator, don't use line_terminator */
  if (field_term_length == line_term_length &&
      !memcmp(field_term_ptr,line_term_ptr,field_term_length))
  {
    line_term_length=0;
    line_term_ptr=(char*) "";
  }
  enclosed_char= (enclosed_length=enclosed_par.length()) ?
    (unsigned char) enclosed_par[0] : INT_MAX;
  field_term_char= field_term_length ? (unsigned char) field_term_ptr[0] : INT_MAX;
  line_term_char= line_term_length ? (unsigned char) line_term_ptr[0] : INT_MAX;
  error=eof=found_end_of_line=found_null=line_cuted=0;
  buff_length=tot_length;


  /* Set of a stack for unget if long terminators */
  size_t length= max(field_term_length,line_term_length)+1;
  set_if_bigger(length, line_start.length());
  stack= stack_pos= (int*) memory::sql_alloc(sizeof(int)*length);

  if (!(buffer=(unsigned char*) calloc(1, buff_length+1)))
    error=1;
  else
  {
    end_of_buff=buffer+buff_length;
    if (cache.init_io_cache((false) ? -1 : cursor, 0,
                            (false) ? internal::READ_NET :
                            (is_fifo ? internal::READ_FIFO : internal::READ_CACHE),0L,1,
                            MYF(MY_WME)))
    {
      free((unsigned char*) buffer);
      error=1;
    }
    else
    {
      /*
	init_io_cache() will not initialize read_function member
	if the cache is READ_NET. So we work around the problem with a
	manual assignment
      */
      need_end_io_cache = 1;
    }
  }
}


READ_INFO::~READ_INFO()
{
  if (!error)
  {
    if (need_end_io_cache)
      cache.end_io_cache();
    free(buffer);
    error=1;
  }
}


#define GET (stack_pos != stack ? *--stack_pos : my_b_get(&cache))
#define PUSH(A) *(stack_pos++)=(A)


inline int READ_INFO::terminator(char *ptr,uint32_t length)
{
  int chr=0;					// Keep gcc happy
  uint32_t i;
  for (i=1 ; i < length ; i++)
  {
    if ((chr=GET) != *++ptr)
    {
      break;
    }
  }
  if (i == length)
    return 1;
  PUSH(chr);
  while (i-- > 1)
    PUSH((unsigned char) *--ptr);
  return 0;
}


int READ_INFO::read_field()
{
  int chr,found_enclosed_char;
  unsigned char *to,*new_buffer;

  found_null=0;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  /* Skip until we find 'line_start' */

  if (start_of_line)
  {						// Skip until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }
  if ((chr=GET) == my_b_EOF)
  {
    found_end_of_line=eof=1;
    return 1;
  }
  to=buffer;
  if (chr == enclosed_char)
  {
    found_enclosed_char=enclosed_char;
    *to++=(unsigned char) chr;				// If error
  }
  else
  {
    found_enclosed_char= INT_MAX;
    PUSH(chr);
  }

  for (;;)
  {
    while ( to < end_of_buff)
    {
      chr = GET;
      if ((my_mbcharlen(read_charset, chr) > 1) &&
          to+my_mbcharlen(read_charset, chr) <= end_of_buff)
      {
        unsigned char* p = (unsigned char*)to;
        *to++ = chr;
        int ml = my_mbcharlen(read_charset, chr);
        int i;
        for (i=1; i<ml; i++) {
          chr = GET;
          if (chr == my_b_EOF)
            goto found_eof;
          *to++ = chr;
        }
        if (my_ismbchar(read_charset,
              (const char *)p,
              (const char *)to))
          continue;
        for (i=0; i<ml; i++)
          PUSH((unsigned char) *--to);
        chr = GET;
      }
      if (chr == my_b_EOF)
        goto found_eof;
      if (chr == escape_char)
      {
        if ((chr=GET) == my_b_EOF)
        {
          *to++= (unsigned char) escape_char;
          goto found_eof;
        }
        /*
          When escape_char == enclosed_char, we treat it like we do for
          handling quotes in SQL parsing -- you can double-up the
          escape_char to include it literally, but it doesn't do escapes
          like \n. This allows: LOAD DATA ... ENCLOSED BY '"' ESCAPED BY '"'
          with data like: "fie""ld1", "field2"
         */
        if (escape_char != enclosed_char || chr == escape_char)
        {
          *to++ = (unsigned char) unescape((char) chr);
          continue;
        }
        PUSH(chr);
        chr= escape_char;
      }
#ifdef ALLOW_LINESEPARATOR_IN_STRINGS
      if (chr == line_term_char)
#else
        if (chr == line_term_char && found_enclosed_char == INT_MAX)
#endif
        {
          if (terminator(line_term_ptr,line_term_length))
          {					// Maybe unexpected linefeed
            enclosed=0;
            found_end_of_line=1;
            row_start=buffer;
            row_end=  to;
            return 0;
          }
        }
      if (chr == found_enclosed_char)
      {
        if ((chr=GET) == found_enclosed_char)
        {					// Remove dupplicated
          *to++ = (unsigned char) chr;
          continue;
        }
        // End of enclosed field if followed by field_term or line_term
        if (chr == my_b_EOF ||
            (chr == line_term_char && terminator(line_term_ptr, line_term_length)))
        {					// Maybe unexpected linefeed
          enclosed=1;
          found_end_of_line=1;
          row_start=buffer+1;
          row_end=  to;
          return 0;
        }
        if (chr == field_term_char &&
            terminator(field_term_ptr,field_term_length))
        {
          enclosed=1;
          row_start=buffer+1;
          row_end=  to;
          return 0;
        }
        /*
           The string didn't terminate yet.
           Store back next character for the loop
         */
        PUSH(chr);
        /* copy the found term character to 'to' */
        chr= found_enclosed_char;
      }
      else if (chr == field_term_char && found_enclosed_char == INT_MAX)
      {
        if (terminator(field_term_ptr,field_term_length))
        {
          enclosed=0;
          row_start=buffer;
          row_end=  to;
          return 0;
        }
      }
      *to++ = (unsigned char) chr;
    }
    /*
     ** We come here if buffer is too small. Enlarge it and continue
     */
    new_buffer=(unsigned char*) realloc(buffer, buff_length+1+IO_SIZE);
    to=new_buffer + (to-buffer);
    buffer=new_buffer;
    buff_length+=IO_SIZE;
    end_of_buff=buffer+buff_length;
  }

found_eof:
  enclosed=0;
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return 0;
}

/*
  Read a row with fixed length.

  NOTES
    The row may not be fixed size on disk if there are escape
    characters in the cursor.

  IMPLEMENTATION NOTE
    One can't use fixed length with multi-byte charset **

  RETURN
    0  ok
    1  error
*/

int READ_INFO::read_fixed_length()
{
  int chr;
  unsigned char *to;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  if (start_of_line)
  {						// Skip until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }

  to=row_start=buffer;
  while (to < end_of_buff)
  {
    if ((chr=GET) == my_b_EOF)
      goto found_eof;
    if (chr == escape_char)
    {
      if ((chr=GET) == my_b_EOF)
      {
	*to++= (unsigned char) escape_char;
	goto found_eof;
      }
      *to++ =(unsigned char) unescape((char) chr);
      continue;
    }
    if (chr == line_term_char)
    {
      if (terminator(line_term_ptr,line_term_length))
      {						// Maybe unexpected linefeed
	found_end_of_line=1;
	row_end=  to;
	return 0;
      }
    }
    *to++ = (unsigned char) chr;
  }
  row_end=to;					// Found full line
  return 0;

found_eof:
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return to == buffer ? 1 : 0;
}


int READ_INFO::next_line()
{
  line_cuted=0;
  start_of_line= line_start_ptr != 0;
  if (found_end_of_line || eof)
  {
    found_end_of_line=0;
    return eof;
  }
  found_end_of_line=0;
  if (!line_term_length)
    return 0;					// No lines
  for (;;)
  {
    int chr = GET;
    if (my_mbcharlen(read_charset, chr) > 1)
    {
      for (uint32_t i=1;
          chr != my_b_EOF && i<my_mbcharlen(read_charset, chr);
          i++)
        chr = GET;
      if (chr == escape_char)
        continue;
    }
    if (chr == my_b_EOF)
    {
      eof=1;
      return 1;
    }
    if (chr == escape_char)
    {
      line_cuted=1;
      if (GET == my_b_EOF)
        return 1;
      continue;
    }
    if (chr == line_term_char && terminator(line_term_ptr,line_term_length))
      return 0;
    line_cuted=1;
  }
}


bool READ_INFO::find_start_of_fields()
{
  int chr;
 try_again:
  do
  {
    if ((chr=GET) == my_b_EOF)
    {
      found_end_of_line=eof=1;
      return 1;
    }
  } while ((char) chr != line_start_ptr[0]);
  for (char *ptr=line_start_ptr+1 ; ptr != line_start_end ; ptr++)
  {
    chr=GET;					// Eof will be checked later
    if ((char) chr != *ptr)
    {						// Can't be line_start
      PUSH(chr);
      while (--ptr != line_start_ptr)
      {						// Restart with next char
	PUSH((unsigned char) *ptr);
      }
      goto try_again;
    }
  }
  return 0;
}


} /* namespace drizzled */
