/* Copyright (C) 2003 MySQL AB

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

/*
  Make sure to look at ha_tina.h for more details.

  First off, this is a play thing for me, there are a number of things
  wrong with it:
    *) It was designed for csv and therefore its performance is highly
       questionable.
    *) Indexes have not been implemented. This is because the files can
       be traded in and out of the table directory without having to worry
       about rebuilding anything.
    *) NULLs and "" are treated equally (like a spreadsheet).
    *) There was in the beginning no point to anyone seeing this other
       then me, so there is a good chance that I haven't quite documented
       it well.
    *) Less design, more "make it work"

  Now there are a few cool things with it:
    *) Errors can result in corrupted data files.
    *) Data files can be read by spreadsheets directly.

TODO:
 *) Move to a block system for larger files
 *) Error recovery, its all there, just need to finish it
 *) Document how the chains work.

 -Brian
*/
#include "config.h"
#include <drizzled/field.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"

#include "ha_tina.h"

#include <fcntl.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace drizzled;

/*
  unsigned char + unsigned char + uint64_t + uint64_t + uint64_t + uint64_t + unsigned char
*/
#define META_BUFFER_SIZE sizeof(unsigned char) + sizeof(unsigned char) + sizeof(uint64_t) \
  + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(unsigned char)
#define TINA_CHECK_HEADER 254 // The number we use to determine corruption
#define BLOB_MEMROOT_ALLOC_SIZE 8192

/* The file extension */
#define CSV_EXT ".CSV"               // The data file
#define CSN_EXT ".CSN"               // Files used during repair and update
#define CSM_EXT ".CSM"               // Meta file


static int read_meta_file(int meta_file, ha_rows *rows);
static int write_meta_file(int meta_file, ha_rows rows, bool dirty);

/* Stuff for shares */
pthread_mutex_t tina_mutex;

/*****************************************************************************
 ** TINA tables
 *****************************************************************************/

/*
  If frm_error() is called in table.cc this is called to find out what file
  extensions exist for this Cursor.
*/
static const char *ha_tina_exts[] = {
  CSV_EXT,
  CSM_EXT,
  NULL
};

class Tina : public drizzled::plugin::StorageEngine
{
  typedef std::map<string, TinaShare*> TinaMap;
  TinaMap tina_open_tables;
public:
  Tina(const string& name_arg)
   : drizzled::plugin::StorageEngine(name_arg,
                                     HTON_TEMPORARY_ONLY |
                                     HTON_NO_AUTO_INCREMENT |
                                     HTON_SKIP_STORE_LOCK),
    tina_open_tables()
  {}
  virtual ~Tina()
  {
    pthread_mutex_destroy(&tina_mutex);
  }

  virtual Cursor *create(Table &table)
  {
    return new ha_tina(*this, table);
  }

  const char **bas_ext() const {
    return ha_tina_exts;
  }

  int doCreateTable(Session &,
                    Table &table_arg,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table&);

  int doGetTableDefinition(Session& session,
                           const drizzled::TableIdentifier &identifier,
                           drizzled::message::Table &table_message);

  int doDropTable(Session&, const drizzled::TableIdentifier &identifier);
  TinaShare *findOpenTable(const string table_name);
  void addOpenTable(const string &table_name, TinaShare *);
  void deleteOpenTable(const string &table_name);


  uint32_t max_keys()          const { return 0; }
  uint32_t max_key_parts()     const { return 0; }
  uint32_t max_key_length()    const { return 0; }
  bool doDoesTableExist(Session& session, const drizzled::TableIdentifier &identifier);
  int doRenameTable(Session&, const drizzled::TableIdentifier &from, const drizzled::TableIdentifier &to);

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::SchemaIdentifier &schema_identifier,
                             drizzled::TableIdentifier::vector &set_of_identifiers);
};

void Tina::doGetTableIdentifiers(drizzled::CachedDirectory&,
                                 const drizzled::SchemaIdentifier&,
                                 drizzled::TableIdentifier::vector&)
{
}

int Tina::doRenameTable(Session &session,
                        const drizzled::TableIdentifier &from, const drizzled::TableIdentifier &to)
{
  int error= 0;
  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from.getPath().c_str(), to.getPath().c_str(), *ext))
    {
      if ((error=errno) != ENOENT)
        break;
      error= 0;
    }
  }

  session.getMessageCache().renameTableMessage(from, to);

  return error;
}

bool Tina::doDoesTableExist(Session &session, const drizzled::TableIdentifier &identifier)
{
  return session.getMessageCache().doesTableMessageExist(identifier);
}


int Tina::doDropTable(Session &session,
                      const drizzled::TableIdentifier &identifier)
{
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted

  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    std::string full_name= identifier.getPath();
    full_name.append(*ext);

    if (internal::my_delete_with_symlink(full_name.c_str(), MYF(0)))
    {
      if ((error= errno) != ENOENT)
	break;
    }
    else
    {
      enoent_or_zero= 0;                        // No error for ENOENT
    }
    error= enoent_or_zero;
  }

  session.getMessageCache().removeTableMessage(identifier);

  return error;
}

TinaShare *Tina::findOpenTable(const string table_name)
{
  TinaMap::iterator find_iter=
    tina_open_tables.find(table_name);

  if (find_iter != tina_open_tables.end())
    return (*find_iter).second;
  else
    return NULL;
}

void Tina::addOpenTable(const string &table_name, TinaShare *share)
{
  tina_open_tables[table_name]= share;
}

void Tina::deleteOpenTable(const string &table_name)
{
  tina_open_tables.erase(table_name);
}


int Tina::doGetTableDefinition(Session &session,
                               const drizzled::TableIdentifier &identifier,
                               drizzled::message::Table &table_message)
{
  if (session.getMessageCache().getTableMessage(identifier, table_message))
    return EEXIST;

  return ENOENT;
}


static Tina *tina_engine= NULL;

static int tina_init_func(drizzled::module::Context &context)
{

  tina_engine= new Tina("CSV");
  context.add(tina_engine);

  pthread_mutex_init(&tina_mutex,MY_MUTEX_INIT_FAST);
  return 0;
}



TinaShare::TinaShare(const std::string &table_name_arg) : 
  table_name(table_name_arg),
  data_file_name(table_name_arg),
  use_count(0),
  saved_data_file_length(0),
  update_file_opened(false),
  tina_write_opened(false),
  crashed(false),
  rows_recorded(0),
  data_file_version(0)
{
  data_file_name.append(CSV_EXT);
}

TinaShare::~TinaShare()
{
  pthread_mutex_destroy(&mutex);
}

/*
  Simple lock controls.
*/
TinaShare *ha_tina::get_share(const std::string &table_name)
{
  pthread_mutex_lock(&tina_mutex);

  Tina *a_tina= static_cast<Tina *>(getEngine());
  share= a_tina->findOpenTable(table_name);

  std::string meta_file_name;
  struct stat file_stat;

  /*
    If share is not present in the hash, create a new share and
    initialize its members.
  */
  if (! share)
  {
    share= new TinaShare(table_name);

    if (share == NULL)
    {
      pthread_mutex_unlock(&tina_mutex);
      return NULL;
    }

    meta_file_name.assign(table_name);
    meta_file_name.append(CSM_EXT);

    if (stat(share->data_file_name.c_str(), &file_stat))
    {
      pthread_mutex_unlock(&tina_mutex);
      delete share;
      return NULL;
    }
  
    share->saved_data_file_length= file_stat.st_size;

    a_tina->addOpenTable(share->table_name, share);

    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);

    /*
      Open or create the meta file. In the latter case, we'll get
      an error during read_meta_file and mark the table as crashed.
      Usually this will result in auto-repair, and we will get a good
      meta-file in the end.
    */
    if ((share->meta_file= internal::my_open(meta_file_name.c_str(),
                                             O_RDWR|O_CREAT, MYF(0))) == -1)
      share->crashed= true;

    /*
      If the meta file will not open we assume it is crashed and
      mark it as such.
    */
    if (read_meta_file(share->meta_file, &share->rows_recorded))
      share->crashed= true;
  }
  share->use_count++;
  pthread_mutex_unlock(&tina_mutex);

  return share;
}


/*
  Read CSV meta-file

  SYNOPSIS
    read_meta_file()
    meta_file   The meta-file filedes
    ha_rows     Pointer to the var we use to store rows count.
                These are read from the meta-file.

  DESCRIPTION

    Read the meta-file info. For now we are only interested in
    rows counf, crashed bit and magic number.

  RETURN
    0 - OK
    non-zero - error occurred
*/

static int read_meta_file(int meta_file, ha_rows *rows)
{
  unsigned char meta_buffer[META_BUFFER_SIZE];
  unsigned char *ptr= meta_buffer;

  lseek(meta_file, 0, SEEK_SET);
  if (internal::my_read(meta_file, (unsigned char*)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    return(HA_ERR_CRASHED_ON_USAGE);

  /*
    Parse out the meta data, we ignore version at the moment
  */

  ptr+= sizeof(unsigned char)*2; // Move past header
  *rows= (ha_rows)uint8korr(ptr);
  ptr+= sizeof(uint64_t); // Move past rows
  /*
    Move past check_point, auto_increment and forced_flushes fields.
    They are present in the format, but we do not use them yet.
  */
  ptr+= 3*sizeof(uint64_t);

  /* check crashed bit and magic number */
  if ((meta_buffer[0] != (unsigned char)TINA_CHECK_HEADER) ||
      ((bool)(*ptr)== true))
    return(HA_ERR_CRASHED_ON_USAGE);

  internal::my_sync(meta_file, MYF(MY_WME));

  return(0);
}


/*
  Write CSV meta-file

  SYNOPSIS
    write_meta_file()
    meta_file   The meta-file filedes
    ha_rows     The number of rows we have in the datafile.
    dirty       A flag, which marks whether we have a corrupt table

  DESCRIPTION

    Write meta-info the the file. Only rows count, crashed bit and
    magic number matter now.

  RETURN
    0 - OK
    non-zero - error occurred
*/

static int write_meta_file(int meta_file, ha_rows rows, bool dirty)
{
  unsigned char meta_buffer[META_BUFFER_SIZE];
  unsigned char *ptr= meta_buffer;

  *ptr= (unsigned char)TINA_CHECK_HEADER;
  ptr+= sizeof(unsigned char);
  *ptr= (unsigned char)TINA_VERSION;
  ptr+= sizeof(unsigned char);
  int8store(ptr, (uint64_t)rows);
  ptr+= sizeof(uint64_t);
  memset(ptr, 0, 3*sizeof(uint64_t));
  /*
     Skip over checkpoint, autoincrement and forced_flushes fields.
     We'll need them later.
  */
  ptr+= 3*sizeof(uint64_t);
  *ptr= (unsigned char)dirty;

  lseek(meta_file, 0, SEEK_SET);
  if (internal::my_write(meta_file, (unsigned char *)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    return(-1);

  internal::my_sync(meta_file, MYF(MY_WME));

  return(0);
}

int ha_tina::init_tina_writer()
{
  /*
    Mark the file as crashed. We will set the flag back when we close
    the file. In the case of the crash it will remain marked crashed,
    which enforce recovery.
  */
  (void)write_meta_file(share->meta_file, share->rows_recorded, true);

  if ((share->tina_write_filedes=
        internal::my_open(share->data_file_name.c_str(), O_RDWR|O_APPEND, MYF(0))) == -1)
  {
    share->crashed= true;
    return(1);
  }
  share->tina_write_opened= true;

  return(0);
}


/*
  Free lock controls.
*/
int ha_tina::free_share()
{
  pthread_mutex_lock(&tina_mutex);
  int result_code= 0;
  if (!--share->use_count){
    /* Write the meta file. Mark it as crashed if needed. */
    (void)write_meta_file(share->meta_file, share->rows_recorded,
                          share->crashed ? true :false);
    if (internal::my_close(share->meta_file, MYF(0)))
      result_code= 1;
    if (share->tina_write_opened)
    {
      if (internal::my_close(share->tina_write_filedes, MYF(0)))
        result_code= 1;
      share->tina_write_opened= false;
    }

    Tina *a_tina= static_cast<Tina *>(getEngine());
    a_tina->deleteOpenTable(share->table_name);
    delete share;
  }
  pthread_mutex_unlock(&tina_mutex);

  return(result_code);
}


/*
  This function finds the end of a line and returns the length
  of the line ending.

  We support three kinds of line endings:
  '\r'     --  Old Mac OS line ending
  '\n'     --  Traditional Unix and Mac OS X line ending
  '\r''\n' --  DOS\Windows line ending
*/

static off_t find_eoln_buff(Transparent_file *data_buff, off_t begin,
                            off_t end, int *eoln_len)
{
  *eoln_len= 0;

  for (off_t x= begin; x < end; x++)
  {
    /* Unix (includes Mac OS X) */
    if (data_buff->get_value(x) == '\n')
      *eoln_len= 1;
    else
      if (data_buff->get_value(x) == '\r') // Mac or Dos
      {
        /* old Mac line ending */
        if (x + 1 == end || (data_buff->get_value(x + 1) != '\n'))
          *eoln_len= 1;
        else // DOS style ending
          *eoln_len= 2;
      }

    if (*eoln_len)  // end of line was found
      return x;
  }

  return 0;
}



ha_tina::ha_tina(drizzled::plugin::StorageEngine &engine_arg, Table &table_arg)
  :Cursor(engine_arg, table_arg),
  /*
    These definitions are found in Cursor.h
    They are not probably completely right.
  */
  current_position(0), next_position(0), local_saved_data_file_length(0),
  file_buff(0), local_data_file_version(0), records_is_known(0)
{
  /* Set our original buffers from pre-allocated memory */
  buffer.set((char*)byte_buffer, IO_SIZE, &my_charset_bin);
  file_buff= new Transparent_file();
}


/*
  Encode a buffer into the quoted format.
*/

int ha_tina::encode_quote(unsigned char *)
{
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer),
                   &my_charset_bin);

  buffer.length(0);

  for (Field **field= getTable()->getFields() ; *field ; field++)
  {
    const char *ptr;
    const char *end_ptr;
    const bool was_null= (*field)->is_null();

    /*
      assistance for backwards compatibility in production builds.
      note: this will not work for ENUM columns.
    */
    if (was_null)
    {
      (*field)->set_default();
      (*field)->set_notnull();
    }

    /* 
      Since we are needing to "translate" the type into a string we 
      will need to do a val_str(). This would cause an assert() to
      normally occur since we are onlying "writing" values.
    */
    (*field)->setReadSet();
    (*field)->val_str(&attribute,&attribute);

    if (was_null)
      (*field)->set_null();

    if ((*field)->str_needs_quotes())
    {
      ptr= attribute.ptr();
      end_ptr= attribute.length() + ptr;

      buffer.append('"');

      while (ptr < end_ptr)
      {
        if (*ptr == '"')
        {
          buffer.append('\\');
          buffer.append('"');
          (void) *ptr++;
        }
        else if (*ptr == '\r')
        {
          buffer.append('\\');
          buffer.append('r');
          (void) *ptr++;
        }
        else if (*ptr == '\\')
        {
          buffer.append('\\');
          buffer.append('\\');
          (void) *ptr++;
        }
        else if (*ptr == '\n')
        {
          buffer.append('\\');
          buffer.append('n');
          (void) *ptr++;
        }
        else
          buffer.append(*ptr++);
      }
      buffer.append('"');
    }
    else
    {
      buffer.append(attribute);
    }

    buffer.append(',');
  }
  // Remove the comma, add a line feed
  buffer.length(buffer.length() - 1);
  buffer.append('\n');

  //buffer.replace(buffer.length(), 0, "\n", 1);

  return (buffer.length());
}

/*
  chain_append() adds delete positions to the chain that we use to keep
  track of space. Then the chain will be used to cleanup "holes", occurred
  due to deletes and updates.
*/
int ha_tina::chain_append()
{
  if (chain.size() > 0 && chain.back().second == current_position)
    chain.back().second= next_position;
  else
    chain.push_back(make_pair(current_position, next_position));
  return 0;
}


/*
  Scans for a row.
*/
int ha_tina::find_current_row(unsigned char *buf)
{
  off_t end_offset, curr_offset= current_position;
  int eoln_len;
  int error;

  blobroot.free_root(MYF(drizzled::memory::MARK_BLOCKS_FREE));

  /*
    We do not read further then local_saved_data_file_length in order
    not to conflict with undergoing concurrent insert.
  */
  if ((end_offset=
        find_eoln_buff(file_buff, current_position,
                       local_saved_data_file_length, &eoln_len)) == 0)
    return(HA_ERR_END_OF_FILE);

  error= HA_ERR_CRASHED_ON_USAGE;

  memset(buf, 0, getTable()->getShare()->null_bytes);

  for (Field **field= getTable()->getFields() ; *field ; field++)
  {
    char curr_char;

    buffer.length(0);
    if (curr_offset >= end_offset)
      goto err;
    curr_char= file_buff->get_value(curr_offset);
    if (curr_char == '"')
    {
      curr_offset++; // Incrementpast the first quote

      for(; curr_offset < end_offset; curr_offset++)
      {
        curr_char= file_buff->get_value(curr_offset);
        // Need to convert line feeds!
        if (curr_char == '"' &&
            (curr_offset == end_offset - 1 ||
             file_buff->get_value(curr_offset + 1) == ','))
        {
          curr_offset+= 2; // Move past the , and the "
          break;
        }
        if (curr_char == '\\' && curr_offset != (end_offset - 1))
        {
          curr_offset++;
          curr_char= file_buff->get_value(curr_offset);
          if (curr_char == 'r')
            buffer.append('\r');
          else if (curr_char == 'n' )
            buffer.append('\n');
          else if (curr_char == '\\' || curr_char == '"')
            buffer.append(curr_char);
          else  /* This could only happed with an externally created file */
          {
            buffer.append('\\');
            buffer.append(curr_char);
          }
        }
        else // ordinary symbol
        {
          /*
            We are at final symbol and no last quote was found =>
            we are working with a damaged file.
          */
          if (curr_offset == end_offset - 1)
            goto err;
          buffer.append(curr_char);
        }
      }
    }
    else
    {
      for(; curr_offset < end_offset; curr_offset++)
      {
        curr_char= file_buff->get_value(curr_offset);
        if (curr_char == ',')
        {
          curr_offset++;       // Skip the ,
          break;
        }
        buffer.append(curr_char);
      }
    }

    if ((*field)->isReadSet() || (*field)->isWriteSet())
    {
      /* This masks a bug in the logic for a SELECT * */
      (*field)->setWriteSet();
      if ((*field)->store(buffer.ptr(), buffer.length(), buffer.charset(),
                          CHECK_FIELD_WARN))
        goto err;

      if ((*field)->flags & BLOB_FLAG)
      {
        Field_blob *blob= *(Field_blob**) field;
        unsigned char *src, *tgt;
        uint32_t length, packlength;

        packlength= blob->pack_length_no_ptr();
        length= blob->get_length(blob->ptr);
        memcpy(&src, blob->ptr + packlength, sizeof(char*));
        if (src)
        {
          tgt= (unsigned char*) blobroot.alloc_root(length);
          memmove(tgt, src, length);
          memcpy(blob->ptr + packlength, &tgt, sizeof(char*));
        }
      }
    }
  }
  next_position= end_offset + eoln_len;
  error= 0;

err:

  return(error);
}

/*
  Open a database file. Keep in mind that tables are caches, so
  this will not be called for every request. Any sort of positions
  that need to be reset should be kept in the ::extra() call.
*/
int ha_tina::doOpen(const TableIdentifier &identifier, int , uint32_t )
{
  if (not (share= get_share(identifier.getPath().c_str())))
    return(ENOENT);

  if (share->crashed)
  {
    free_share();
    return(HA_ERR_CRASHED_ON_USAGE);
  }

  local_data_file_version= share->data_file_version;
  if ((data_file= internal::my_open(share->data_file_name.c_str(), O_RDONLY, MYF(0))) == -1)
    return(0);

  /*
    Init locking. Pass Cursor object to the locking routines,
    so that they could save/update local_saved_data_file_length value
    during locking. This is needed to enable concurrent inserts.
  */
  ref_length=sizeof(off_t);

  return(0);
}

/*
  Close a database file. We remove ourselves from the shared strucutre.
  If it is empty we destroy it.
*/
int ha_tina::close(void)
{
  int rc= 0;
  rc= internal::my_close(data_file, MYF(0));
  return(free_share() || rc);
}

/*
  This is an INSERT. At the moment this Cursor just seeks to the end
  of the file and appends the data. In an error case it really should
  just truncate to the original position (this is not done yet).
*/
int ha_tina::doInsertRecord(unsigned char * buf)
{
  int size;

  if (share->crashed)
      return(HA_ERR_CRASHED_ON_USAGE);

  size= encode_quote(buf);

  if (!share->tina_write_opened)
    if (init_tina_writer())
      return(-1);

   /* use pwrite, as concurrent reader could have changed the position */
  if (internal::my_write(share->tina_write_filedes, (unsigned char*)buffer.ptr(), size,
               MYF(MY_WME | MY_NABP)))
    return(-1);

  /* update local copy of the max position to see our own changes */
  local_saved_data_file_length+= size;

  /* update shared info */
  pthread_mutex_lock(&share->mutex);
  share->rows_recorded++;
  /* update status for the log tables */
  pthread_mutex_unlock(&share->mutex);

  stats.records++;
  return(0);
}


int ha_tina::open_update_temp_file_if_needed()
{
  char updated_fname[FN_REFLEN];

  if (!share->update_file_opened)
  {
    if ((update_temp_file=
           internal::my_create(internal::fn_format(updated_fname, share->table_name.c_str(),
                               "", CSN_EXT,
                               MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                     0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
      return 1;
    share->update_file_opened= true;
    temp_file_length= 0;
  }
  return 0;
}

/*
  This is called for an update.
  Make sure you put in code to increment the auto increment, also
  update any timestamp data. Currently auto increment is not being
  fixed since autoincrements have yet to be added to this table Cursor.
  This will be called in a table scan right before the previous ::rnd_next()
  call.
*/
int ha_tina::doUpdateRecord(const unsigned char *, unsigned char * new_data)
{
  int size;
  int rc= -1;

  size= encode_quote(new_data);

  /*
    During update we mark each updating record as deleted
    (see the chain_append()) then write new one to the temporary data file.
    At the end of the sequence in the doEndTableScan() we append all non-marked
    records from the data file to the temporary data file then rename it.
    The temp_file_length is used to calculate new data file length.
  */
  if (chain_append())
    goto err;

  if (open_update_temp_file_if_needed())
    goto err;

  if (internal::my_write(update_temp_file, (unsigned char*)buffer.ptr(), size,
               MYF(MY_WME | MY_NABP)))
    goto err;
  temp_file_length+= size;
  rc= 0;

err:
  return(rc);
}


/*
  Deletes a row. First the database will find the row, and then call this
  method. In the case of a table scan, the previous call to this will be
  the ::rnd_next() that found this row.
  The exception to this is an ORDER BY. This will cause the table Cursor
  to walk the table noting the positions of all rows that match a query.
  The table will then be deleted/positioned based on the ORDER (so RANDOM,
  DESC, ASC).
*/
int ha_tina::doDeleteRecord(const unsigned char *)
{

  if (chain_append())
    return(-1);

  stats.records--;
  /* Update shared info */
  assert(share->rows_recorded);
  pthread_mutex_lock(&share->mutex);
  share->rows_recorded--;
  pthread_mutex_unlock(&share->mutex);

  return(0);
}


/**
  @brief Initialize the data file.

  @details Compare the local version of the data file with the shared one.
  If they differ, there are some changes behind and we have to reopen
  the data file to make the changes visible.
  Call @c file_buff->init_buff() at the end to read the beginning of the
  data file into buffer.

  @retval  0  OK.
  @retval  1  There was an error.
*/

int ha_tina::init_data_file()
{
  if (local_data_file_version != share->data_file_version)
  {
    local_data_file_version= share->data_file_version;
    if (internal::my_close(data_file, MYF(0)) ||
        (data_file= internal::my_open(share->data_file_name.c_str(), O_RDONLY, MYF(0))) == -1)
      return 1;
  }
  file_buff->init_buff(data_file);
  return 0;
}


/*
  All table scans call this first.
  The order of a table scan is:

  ha_tina::info
  ha_tina::rnd_init
  ha_tina::extra
  ENUM HA_EXTRA_CACHE   Cash record in HA_rrnd()
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::extra
  ENUM HA_EXTRA_NO_CACHE   End cacheing of records (def)
  ha_tina::extra
  ENUM HA_EXTRA_RESET   Reset database to after open

  Each call to ::rnd_next() represents a row returned in the can. When no more
  rows can be returned, rnd_next() returns a value of HA_ERR_END_OF_FILE.
  The ::info() call is just for the optimizer.

*/

int ha_tina::doStartTableScan(bool)
{
  /* set buffer to the beginning of the file */
  if (share->crashed || init_data_file())
    return(HA_ERR_CRASHED_ON_USAGE);

  current_position= next_position= 0;
  stats.records= 0;
  records_is_known= 0;
  chain.clear();

  blobroot.init_alloc_root(BLOB_MEMROOT_ALLOC_SIZE);

  return(0);
}

/*
  ::rnd_next() does all the heavy lifting for a table scan. You will need to
  populate *buf with the correct field data. You can walk the field to
  determine at what position you should store the data (take a look at how
  ::find_current_row() works). The structure is something like:
  0Foo  Dog  Friend
  The first offset is for the first attribute. All space before that is
  reserved for null count.
  Basically this works as a mask for which rows are nulled (compared to just
  empty).
  This table Cursor doesn't do nulls and does not know the difference between
  NULL and "". This is ok since this table Cursor is for spreadsheets and
  they don't know about them either :)
*/
int ha_tina::rnd_next(unsigned char *buf)
{
  int rc;

  if (share->crashed)
      return(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);

  current_position= next_position;

  /* don't scan an empty file */
  if (!local_saved_data_file_length)
    return(HA_ERR_END_OF_FILE);

  if ((rc= find_current_row(buf)))
    return(rc);

  stats.records++;
  return(0);
}

/*
  In the case of an order by rows will need to be sorted.
  ::position() is called after each call to ::rnd_next(),
  the data it stores is to a byte array. You can store this
  data via my_store_ptr(). ref_length is a variable defined to the
  class that is the sizeof() of position being stored. In our case
  its just a position. Look at the bdb code if you want to see a case
  where something other then a number is stored.
*/
void ha_tina::position(const unsigned char *)
{
  internal::my_store_ptr(ref, ref_length, current_position);
  return;
}


/*
  Used to fetch a row from a posiion stored with ::position().
  internal::my_get_ptr() retrieves the data for you.
*/

int ha_tina::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  current_position= (off_t)internal::my_get_ptr(pos,ref_length);
  return(find_current_row(buf));
}

/*
  ::info() is used to return information to the optimizer.
  Currently this table Cursor doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
int ha_tina::info(uint32_t)
{
  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  if (!records_is_known && stats.records < 2)
    stats.records= 2;
  return(0);
}

/*
  Set end_pos to the last valid byte of continuous area, closest
  to the given "hole", stored in the buffer. "Valid" here means,
  not listed in the chain of deleted records ("holes").
*/
bool ha_tina::get_write_pos(off_t *end_pos, vector< pair<off_t, off_t> >::iterator &closest_hole)
{
  if (closest_hole == chain.end()) /* no more chains */
    *end_pos= file_buff->end();
  else
    *end_pos= std::min(file_buff->end(),
                       closest_hole->first);
  return (closest_hole != chain.end()) && (*end_pos == closest_hole->first);
}


/*
  Called after each table scan. In particular after deletes,
  and updates. In the last case we employ chain of deleted
  slots to clean up all of the dead space we have collected while
  performing deletes/updates.
*/
int ha_tina::doEndTableScan()
{
  off_t file_buffer_start= 0;

  blobroot.free_root(MYF(0));
  records_is_known= 1;

  if (chain.size() > 0)
  {
    vector< pair<off_t, off_t> >::iterator ptr= chain.begin();

    /*
      Re-read the beginning of a file (as the buffer should point to the
      end of file after the scan).
    */
    file_buff->init_buff(data_file);

    /*
      The sort is needed when there were updates/deletes with random orders.
      It sorts so that we move the firts blocks to the beginning.
    */
    sort(chain.begin(), chain.end());

    off_t write_begin= 0, write_end;

    /* create the file to write updated table if it wasn't yet created */
    if (open_update_temp_file_if_needed())
      return(-1);

    /* write the file with updated info */
    while ((file_buffer_start != -1))     // while not end of file
    {
      bool in_hole= get_write_pos(&write_end, ptr);
      off_t write_length= write_end - write_begin;
      if ((uint64_t)write_length > SIZE_MAX)
      {
        goto error;
      }

      /* if there is something to write, write it */
      if (write_length)
      {
        if (internal::my_write(update_temp_file,
                     (unsigned char*) (file_buff->ptr() +
                               (write_begin - file_buff->start())),
                     (size_t)write_length, MYF_RW))
          goto error;
        temp_file_length+= write_length;
      }
      if (in_hole)
      {
        /* skip hole */
        while (file_buff->end() <= ptr->second && file_buffer_start != -1)
          file_buffer_start= file_buff->read_next();
        write_begin= ptr->second;
        ++ptr;
      }
      else
        write_begin= write_end;

      if (write_end == file_buff->end())
        file_buffer_start= file_buff->read_next(); /* shift the buffer */

    }

    if (internal::my_sync(update_temp_file, MYF(MY_WME)) ||
        internal::my_close(update_temp_file, MYF(0)))
      return(-1);

    share->update_file_opened= false;

    if (share->tina_write_opened)
    {
      if (internal::my_close(share->tina_write_filedes, MYF(0)))
        return(-1);
      /*
        Mark that the writer fd is closed, so that init_tina_writer()
        will reopen it later.
      */
      share->tina_write_opened= false;
    }

    /*
      Close opened fildes's. Then move updated file in place
      of the old datafile.
    */
    std::string rename_file= share->table_name;
    rename_file.append(CSN_EXT);
    if (internal::my_close(data_file, MYF(0)) ||
        internal::my_rename(rename_file.c_str(),
                            share->data_file_name.c_str(), MYF(0)))
      return(-1);

    /* Open the file again */
    if (((data_file= internal::my_open(share->data_file_name.c_str(), O_RDONLY, MYF(0))) == -1))
      return(-1);
    /*
      As we reopened the data file, increase share->data_file_version
      in order to force other threads waiting on a table lock and
      have already opened the table to reopen the data file.
      That makes the latest changes become visible to them.
      Update local_data_file_version as no need to reopen it in the
      current thread.
    */
    share->data_file_version++;
    local_data_file_version= share->data_file_version;
    /*
      The datafile is consistent at this point and the write filedes is
      closed, so nothing worrying will happen to it in case of a crash.
      Here we record this fact to the meta-file.
    */
    (void)write_meta_file(share->meta_file, share->rows_recorded, false);
    /*
      Update local_saved_data_file_length with the real length of the
      data file.
    */
    local_saved_data_file_length= temp_file_length;
  }

  return(0);
error:
  internal::my_close(update_temp_file, MYF(0));
  share->update_file_opened= false;
  return(-1);
}


/*
  DELETE without WHERE calls this
*/

int ha_tina::delete_all_rows()
{
  int rc;

  if (!records_is_known)
    return(errno=HA_ERR_WRONG_COMMAND);

  if (!share->tina_write_opened)
    if (init_tina_writer())
      return(-1);

  /* Truncate the file to zero size */
  rc= ftruncate(share->tina_write_filedes, 0);

  stats.records=0;
  /* Update shared info */
  pthread_mutex_lock(&share->mutex);
  share->rows_recorded= 0;
  pthread_mutex_unlock(&share->mutex);
  local_saved_data_file_length= 0;
  return(rc);
}

/*
  Create a table. You do not want to leave the table open after a call to
  this (the database will call ::open() if it needs to).
*/

int Tina::doCreateTable(Session &session,
                        Table& table_arg,
                        const drizzled::TableIdentifier &identifier,
                        drizzled::message::Table &create_proto)
{
  char name_buff[FN_REFLEN];
  int create_file;

  /*
    check columns
  */
  const drizzled::TableShare::Fields fields(table_arg.getShare()->getFields());
  for (drizzled::TableShare::Fields::const_iterator iter= fields.begin();
       iter != fields.end();
       iter++)
  {
    if (not *iter) // Historical legacy for NULL array end.
      continue;

    if ((*iter)->real_maybe_null())
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "nullable columns");
      return(HA_ERR_UNSUPPORTED);
    }
  }


  if ((create_file= internal::my_create(internal::fn_format(name_buff, identifier.getPath().c_str(), "", CSM_EXT,
                                                            MY_REPLACE_EXT|MY_UNPACK_FILENAME), 0,
                                        O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    return(-1);

  write_meta_file(create_file, 0, false);
  internal::my_close(create_file, MYF(0));

  if ((create_file= internal::my_create(internal::fn_format(name_buff, identifier.getPath().c_str(), "", CSV_EXT,
                                                            MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                                        O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    return(-1);

  internal::my_close(create_file, MYF(0));

  session.getMessageCache().storeTableMessage(identifier, create_proto);

  return 0;
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "CSV",
  "1.0",
  "Brian Aker, MySQL AB",
  "CSV storage engine",
  PLUGIN_LICENSE_GPL,
  tina_init_func, /* Plugin Init */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;

