/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Zimin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <drizzled/field.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "filesystem.h"
#include "utility.h"

#include <fcntl.h>

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

using namespace std;
using namespace drizzled;

#define FILESYSTEM_EXT ".FST"

static const char* FILESYSTEM_OPTION_FILE_PATH= "FILE";
static const char* FILESYSTEM_OPTION_ROW_SEPARATOR= "ROW_SEPARATOR";
static const char* FILESYSTEM_OPTION_COL_SEPARATOR= "COL_SEPARATOR";
static const char* FILESYSTEM_OPTION_SEPARATOR_MODE= "SEPARATOR_MODE";
static const char* FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT= "STRICT";
static const char* FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL= "GENERAL";
static const char* FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK= "WEAK";
enum filesystem_option_separator_mode_type
{
  FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT_ENUM= 1,
  FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM,
  FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK_ENUM
};

static const char* DEFAULT_ROW_SEPARATOR= "\n";
static const char* DEFAULT_COL_SEPARATOR= " \t";

/* Stuff for shares */
pthread_mutex_t filesystem_mutex;

static const char *ha_filesystem_exts[] = {
  FILESYSTEM_EXT,
  NULL
};

class FilesystemEngine : public drizzled::plugin::StorageEngine
{
private:
  typedef std::map<string, FilesystemTableShare*> FilesystemMap;
  FilesystemMap fs_open_tables;
public:
  FilesystemEngine(const string& name_arg)
   : drizzled::plugin::StorageEngine(name_arg,
                                     HTON_NULL_IN_KEY |
                                     HTON_CAN_INDEX_BLOBS |
                                     HTON_AUTO_PART_KEY),
     fs_open_tables()
  {
    table_definition_ext = FILESYSTEM_EXT;
    pthread_mutex_init(&filesystem_mutex, MY_MUTEX_INIT_FAST);
  }
  virtual ~FilesystemEngine()
  {
    pthread_mutex_destroy(&filesystem_mutex);
  }

  virtual Cursor *create(TableShare &table,
                         drizzled::memory::Root *mem_root)
  {
    return new (mem_root) FilesystemCursor(*this, table);
  }

  const char **bas_ext() const {
    return ha_filesystem_exts;
  }

  bool validateCreateTableOption(const std::string &key, const std::string &state);

  int doCreateTable(Session &,
                    Table &table_arg,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table&);

  int doGetTableDefinition(Session& ,
                           const drizzled::TableIdentifier &,
                           drizzled::message::Table &);

  /* Temp only engine, so do not return values. */
  void doGetTableNames(drizzled::CachedDirectory &, drizzled::SchemaIdentifier &, drizzled::plugin::TableNameList &);

  int doDropTable(Session&, const TableIdentifier &);

  /* operations on FilesystemTableShare */
  FilesystemTableShare *findOpenTable(const string table_name);
  void addOpenTable(const string &table_name, FilesystemTableShare *);
  void deleteOpenTable(const string &table_name);

  uint32_t max_keys()          const { return 0; }
  uint32_t max_key_parts()     const { return 0; }
  uint32_t max_key_length()    const { return 0; }
  bool doDoesTableExist(Session& , const TableIdentifier &);
  int doRenameTable(Session&, const TableIdentifier &, const TableIdentifier &);
  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             drizzled::SchemaIdentifier &schema_identifier,
                             drizzled::TableIdentifiers &set_of_identifiers);
private:
  void getTableNamesFromFilesystem(drizzled::CachedDirectory &directory,
                                   drizzled::SchemaIdentifier &schema_identifier,
                                   drizzled::plugin::TableNameList *set_of_names,
                                   drizzled::TableIdentifiers *set_of_identifiers);
};

void FilesystemEngine::getTableNamesFromFilesystem(drizzled::CachedDirectory &directory,
                                                   drizzled::SchemaIdentifier &schema_identifier,
                                                   drizzled::plugin::TableNameList *set_of_names,
                                                   drizzled::TableIdentifiers *set_of_identifiers)
{
  drizzled::CachedDirectory::Entries entries= directory.getEntries();

  for (drizzled::CachedDirectory::Entries::iterator entry_iter= entries.begin();
      entry_iter != entries.end();
      ++entry_iter)
  {
    drizzled::CachedDirectory::Entry *entry= *entry_iter;
    const string *filename= &entry->filename;

    assert(not filename->empty());

    string::size_type suffix_pos= filename->rfind('.');

    if (suffix_pos != string::npos &&
        boost::iequals(filename->substr(suffix_pos), FILESYSTEM_EXT) &&
        filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX))
    {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len= TableIdentifier::filename_to_tablename(filename->c_str(), uname, sizeof(uname));
      uname[file_name_len - sizeof(FILESYSTEM_EXT) + 1]= '\0';
      if (set_of_names)
        set_of_names->insert(uname);
      if (set_of_identifiers)
        set_of_identifiers->push_back(TableIdentifier(schema_identifier, uname));
    }
  }
}

void FilesystemEngine::doGetTableNames(drizzled::CachedDirectory &directory,
                                       drizzled::SchemaIdentifier &schema_identifier,
                                       drizzled::plugin::TableNameList &set_of_names)
{
  getTableNamesFromFilesystem(directory, schema_identifier, &set_of_names, NULL);
}

void FilesystemEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                                             drizzled::SchemaIdentifier &schema_identifier,
                                             drizzled::TableIdentifiers &set_of_identifiers)
{
  getTableNamesFromFilesystem(directory, schema_identifier, NULL, &set_of_identifiers);
}

int FilesystemEngine::doDropTable(Session &, const TableIdentifier &identifier)
{
  string new_path(identifier.getPath());
  new_path+= FILESYSTEM_EXT;
  int err= unlink(new_path.c_str());
  if (err)
  {
    err= errno;
  }
  return err;
}

bool FilesystemEngine::doDoesTableExist(Session &, const TableIdentifier &identifier)
{
  string proto_path(identifier.getPath());
  proto_path.append(FILESYSTEM_EXT);

  if (access(proto_path.c_str(), F_OK))
  {
    return false;
  }

  return true;
}

FilesystemTableShare *FilesystemEngine::findOpenTable(const string table_name)
{
  FilesystemMap::iterator find_iter=
    fs_open_tables.find(table_name);

  if (find_iter != fs_open_tables.end())
    return (*find_iter).second;
  else
    return NULL;
}

void FilesystemEngine::addOpenTable(const string &table_name, FilesystemTableShare *share)
{
  fs_open_tables[table_name]= share;
}

void FilesystemEngine::deleteOpenTable(const string &table_name)
{
  fs_open_tables.erase(table_name);
}

int FilesystemEngine::doGetTableDefinition(Session &,
                               const drizzled::TableIdentifier &identifier,
                               drizzled::message::Table &table_proto)
{
  string new_path(identifier.getPath());
  new_path.append(FILESYSTEM_EXT);

  int fd= ::open(new_path.c_str(), O_RDONLY);
  if (fd < 0)
    return ENOENT;

  google::protobuf::io::ZeroCopyInputStream* input=
    new google::protobuf::io::FileInputStream(fd);

  if (not input)
    return HA_ERR_CRASHED_ON_USAGE;

  if (not table_proto.ParseFromZeroCopyStream(input))
  {
    close(fd);
    delete input;
    if (not table_proto.IsInitialized())
    {
      my_error(ER_CORRUPT_TABLE_DEFINITION, MYF(0),
               table_proto.InitializationErrorString().c_str());
      return ER_CORRUPT_TABLE_DEFINITION;
    }

    return HA_ERR_CRASHED_ON_USAGE;
  }

  delete input;

  return EEXIST;
}

FilesystemTableShare::FilesystemTableShare(const string table_name_arg)
  : use_count(0), table_name(table_name_arg),
  update_file_opened(false),
  needs_reopen(false),
  row_separator(DEFAULT_ROW_SEPARATOR),
  col_separator(DEFAULT_COL_SEPARATOR),
  separator_mode(FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM)
{
  thr_lock_init(&lock);
}

FilesystemTableShare::~FilesystemTableShare()
{
  thr_lock_delete(&lock);
  pthread_mutex_destroy(&mutex);
}

FilesystemTableShare *FilesystemCursor::get_share(const char *table_name)
{
  pthread_mutex_lock(&filesystem_mutex);

  FilesystemEngine *a_engine= static_cast<FilesystemEngine *>(engine);
  share= a_engine->findOpenTable(table_name);

  /*
    If share is not present in the hash, create a new share and
    initialize its members.
  */
  if (share == NULL)
  {
    share= new (nothrow) FilesystemTableShare(table_name);
    if (share == NULL)
    {
      pthread_mutex_unlock(&filesystem_mutex);
      return NULL;
    }
    message::Table* table_proto = table->getShare()->getTableProto();

    share->real_file_name.clear();
    for (int i = 0; i < table_proto->engine().options_size(); i++)
    {
      const message::Engine::Option& option= table_proto->engine().options(i);

      if (boost::iequals(option.name(), FILESYSTEM_OPTION_FILE_PATH))
        share->real_file_name= option.state();
      else if (boost::iequals(option.name(), FILESYSTEM_OPTION_ROW_SEPARATOR))
        share->row_separator= option.state();
      else if (boost::iequals(option.name(), FILESYSTEM_OPTION_COL_SEPARATOR))
        share->col_separator= option.state();
      else if (boost::iequals(option.name(), FILESYSTEM_OPTION_SEPARATOR_MODE))
      {
        if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT))
          share->separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT_ENUM;
        else if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL))
          share->separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM;
        else if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK))
          share->separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK_ENUM;
      }
    }

    if (share->real_file_name.empty())
    {
      pthread_mutex_unlock(&filesystem_mutex);
      return NULL;
    }

    a_engine->addOpenTable(share->table_name, share);

    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&filesystem_mutex);

  return share;
}

void FilesystemCursor::free_share()
{
  pthread_mutex_lock(&filesystem_mutex);
  if (!--share->use_count){
    FilesystemEngine *a_engine= static_cast<FilesystemEngine *>(engine);
    a_engine->deleteOpenTable(share->table_name);
    delete share;
  }
  pthread_mutex_unlock(&filesystem_mutex);
}

FilesystemCursor::FilesystemCursor(drizzled::plugin::StorageEngine &engine_arg, TableShare &table_arg)
  : Cursor(engine_arg, table_arg)
{
  file_buff= new TransparentFile();
}

int FilesystemCursor::doOpen(const drizzled::TableIdentifier &identifier, int, uint32_t)
{
  if (!(share= get_share(identifier.getPath().c_str())))
    return ENOENT;

  file_desc= ::open(share->real_file_name.c_str(), O_RDONLY);
  if (file_desc < 0)
  {
    free_share();
    return ER_CANT_OPEN_FILE;
  }

  ref_length= sizeof(off_t);
  thr_lock_data_init(&share->lock, &lock, NULL);
  return 0;
}

int FilesystemCursor::close(void)
{
  int err;
  while ((err= ::close(file_desc)) < 0 && errno == EINTR)
    ;
  if (err < 0)
    err= errno;
  free_share();
  return err;
}

int FilesystemCursor::doStartTableScan(bool)
{
  current_position= 0;
  next_position= 0;
  slots.clear();
  if (share->needs_reopen)
  {
    file_desc= ::open(share->real_file_name.c_str(), O_RDONLY);
    if (file_desc < 0)
      return HA_ERR_CRASHED_ON_USAGE;
    share->needs_reopen= false;
  }
  file_buff->init_buff(file_desc);
  return 0;
}

int FilesystemCursor::find_current_row(unsigned char *buf)
{
  ptrdiff_t row_offset= buf - table->record[0];

  next_position= current_position;

  string content;
  bool line_done= false;
  bool line_blank= true;
  Field **field= table->getFields();
  for (; !line_done && *field; ++next_position)
  {
    char ch= file_buff->get_value(next_position);
    if (ch == '\0')
      return HA_ERR_END_OF_FILE;

    // if we find separator
    bool is_row= (share->row_separator.find(ch) != string::npos);
    bool is_col= (share->col_separator.find(ch) != string::npos);
    if (content.empty())
    {
      if (share->separator_mode >= FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM
          && is_row && line_blank)
        continue;
      if (share->separator_mode >= FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK_ENUM
          && is_col)
        continue;
    }

    if (is_row || is_col)
    {
      (*field)->move_field_offset(row_offset);
      if (!content.empty())
      {
        (*field)->set_notnull();
        if ((*field)->isReadSet() || (*field)->isWriteSet())
        {
          (*field)->setWriteSet();
          (*field)->store(content.c_str(),
                          (uint32_t)content.length(),
                          &my_charset_bin,
                          CHECK_FIELD_WARN);
        }
        else
          (*field)->set_default();
      }
      else
        (*field)->set_null();
      (*field)->move_field_offset(-row_offset);

      content.clear();
      ++field;

      line_blank= false;
      if (is_row)
        line_done= true;

      continue;
    }
    content.push_back(ch);
  }
  if (line_done)
  {
    for (; *field; ++field)
    {
      (*field)->move_field_offset(row_offset);
      (*field)->set_notnull();
      (*field)->set_default();
      (*field)->move_field_offset(-row_offset);
    }
  }
  else
  {
    // eat up characters when line_done
    while (!line_done)
    {
      char ch= file_buff->get_value(next_position);
      if (share->row_separator.find(ch) != string::npos)
        line_done= true;
      ++next_position;
    }
  }
  return 0;
}

int FilesystemCursor::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  current_position= next_position;
  return find_current_row(buf);
}

void FilesystemCursor::position(const unsigned char *)
{
  *reinterpret_cast<off_t *>(ref)= current_position;
}

int FilesystemCursor::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  current_position= *reinterpret_cast<off_t *>(pos);
  return find_current_row(buf);
}

int FilesystemCursor::info(uint32_t)
{
  if (stats.records < 2)
    stats.records= 2;
  return 0;
}

int FilesystemCursor::openUpdateFile()
{
  if (!share->update_file_opened)
  {
    struct stat st;
    if (stat(share->real_file_name.c_str(), &st) < 0)
      return -1;
    update_file_name= share->real_file_name;
    update_file_name.append(".UPDATE");
    unlink(update_file_name.c_str());
    update_file_desc= ::open(update_file_name.c_str(),
                             O_RDWR | O_CREAT | O_TRUNC,
                             st.st_mode);
    if (update_file_desc < 0)
    {
      return -1;
    }
    share->update_file_opened= true;
  }
  return 0;
}

int FilesystemCursor::doEndTableScan()
{
  if (slots.size() == 0)
    return 0;

  int err= -1;
  sort(slots.begin(), slots.end());
  vector< pair<off_t, off_t> >::iterator slot_iter= slots.begin();
  off_t write_start= 0;
  off_t write_end= 0;
  off_t file_buffer_start= 0;

  pthread_mutex_lock(&share->mutex);

  file_buff->init_buff(file_desc);
  if (openUpdateFile() < 0)
    goto error;

  while (file_buffer_start != -1)
  {
    bool in_hole= false;

    write_end= file_buff->end();
    if (slot_iter != slots.end() &&
      write_end >= slot_iter->first)
    {
      write_end= slot_iter->first;
      in_hole= true;
    }

    off_t write_length= write_end - write_start;
    if (xwrite(update_file_desc,
               file_buff->ptr() + (write_start - file_buff->start()),
               write_length) != write_length)
    {
      err= errno;
      goto error;
    }

    if (in_hole)
    {
      while (file_buff->end() <= slot_iter->second && file_buffer_start != -1)
        file_buffer_start= file_buff->read_next();
      write_start= slot_iter->second;
      ++slot_iter;
    }
    else
      write_start= write_end;

    if (write_end == file_buff->end())
      file_buffer_start= file_buff->read_next();
  }
  // close update file
  if (::fsync(update_file_desc) ||
      ::close(update_file_desc))
  {
    goto error;
  }
  share->update_file_opened= false;

  // close current file
  ::close(file_desc);
  if (::rename(update_file_name.c_str(), share->real_file_name.c_str()))
    goto error;

  // reopen the data file
  file_desc= ::open(share->real_file_name.c_str(), O_RDONLY);
  share->needs_reopen= true;
  if (file_desc < 0)
    goto error;
  err= 0;
error:
  pthread_mutex_unlock(&share->mutex);
  return err;
}

void FilesystemCursor::recordToString(string& output)
{
  bool first = true;
  drizzled::String attribute;
  for (Field **field= table->getFields(); *field; ++field)
  {
    if (first == true)
    {
      first = false;
    }
    else
    {
      output.append(share->col_separator.substr(0, 1));
    }

    if (not (*field)->is_null())
    {
      (*field)->setReadSet();
      (*field)->val_str(&attribute, &attribute);

      output.append(attribute.ptr(), attribute.length());
    }
    else
    {
      output.append("0");
    }
  }
  output.append(share->row_separator.substr(0, 1));
}

int FilesystemCursor::doInsertRecord(unsigned char * buf)
{
  (void)buf;
  int err_write= 0;
  int err_close= 0;

  string output_line;
  recordToString(output_line);

  pthread_mutex_lock(&share->mutex);
  int fd= ::open(share->real_file_name.c_str(), O_WRONLY | O_APPEND);
  if (fd < 0)
  {
    pthread_mutex_unlock(&share->mutex);
    return ENOENT;
  }

  err_write= xwrite(fd, output_line.c_str(), output_line.length());
  if (err_write < 0)
    err_write= errno;
  else
    err_write= 0;

  err_close= xclose(fd);
  if (err_close < 0)
    err_close= errno;

  pthread_mutex_unlock(&share->mutex);

  if (err_write)
    return err_write;
  if (err_close)
    return err_close;
  return 0;
}

int FilesystemCursor::doUpdateRecord(const unsigned char *, unsigned char *)
{
  if (openUpdateFile())
    return errno;

  // get the update information
  string str;
  recordToString(str);

  if (xwrite(update_file_desc, str.c_str(), str.length()) < 0)
    return errno;

  addSlot();

  return 0;
}

void FilesystemCursor::addSlot()
{
  if (slots.size() > 0 && slots.back().second == current_position)
    slots.back().second= next_position;
  else
    slots.push_back(make_pair(current_position, next_position));
}

int FilesystemCursor::doDeleteRecord(const unsigned char *)
{
  addSlot();
  return 0;
}

int FilesystemEngine::doRenameTable(Session&, const TableIdentifier &from, const TableIdentifier &to)
{
  if (rename_file_ext(from.getPath().c_str(), to.getPath().c_str(), FILESYSTEM_EXT))
    return errno;
  return 0;
}

bool FilesystemEngine::validateCreateTableOption(const std::string &key,
                                                 const std::string &state)
{
  if (boost::iequals(key, FILESYSTEM_OPTION_FILE_PATH) &&
      ! state.empty())
    return true;
  if ((boost::iequals(key, FILESYSTEM_OPTION_ROW_SEPARATOR) ||
       boost::iequals(key, FILESYSTEM_OPTION_COL_SEPARATOR)) &&
      ! state.empty())
    return true;
  if (boost::iequals(key, FILESYSTEM_OPTION_SEPARATOR_MODE) &&
      (boost::iequals(state, FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT) ||
       boost::iequals(state, FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL) ||
       boost::iequals(state, FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK)))
    return true;
  return false;
}

THR_LOCK_DATA **FilesystemCursor::store_lock(Session *,
                                             THR_LOCK_DATA **to,
                                             thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

int FilesystemEngine::doCreateTable(Session &,
                        Table&,
                        const drizzled::TableIdentifier &identifier,
                        drizzled::message::Table &proto)
{
  for (int i = 0; i < proto.engine().options_size(); i++)
  {
    const message::Engine::Option& option= proto.engine().options(i);

    if (boost::iequals(option.name(), FILESYSTEM_OPTION_FILE_PATH))
    {
      int err= ::open(option.state().c_str(), O_RDONLY);
      if (err < 0)
        return errno;
      break;
    }
  }

  string new_path(identifier.getPath());
  new_path+= FILESYSTEM_EXT;
  fstream output(new_path.c_str(), ios::out | ios::binary);

  if (! output)
    return 1;

  if (! proto.SerializeToOstream(&output))
  {
    output.close();
    unlink(new_path.c_str());
    return 1;
  }

  return 0;
}

static FilesystemEngine *filesystem_engine= NULL;

static int filesystem_init_func(drizzled::module::Context &context)
{
  filesystem_engine = new FilesystemEngine("FILESYSTEM");
  context.add(filesystem_engine);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "FILESYSTEM",
  "1.0",
  "Zimin",
  "Filesystem Engine",
  PLUGIN_LICENSE_GPL,
  filesystem_init_func, /* Plugin Init */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;
