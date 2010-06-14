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
                                     HTON_SKIP_STORE_LOCK |
                                     HTON_AUTO_PART_KEY),
     fs_open_tables()
  {
    table_definition_ext = FILESYSTEM_EXT;  // should we set this first?
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
                    drizzled::TableIdentifier &identifier,
                    drizzled::message::Table&);

  int doGetTableDefinition(Session& ,
                           TableIdentifier &,
                           drizzled::message::Table &);

  /* Temp only engine, so do not return values. */
  void doGetTableNames(drizzled::CachedDirectory &, drizzled::SchemaIdentifier &, drizzled::plugin::TableNameList &);

  int doDropTable(Session&, TableIdentifier &);

  /* operations on FilesystemTableShare */
  FilesystemTableShare *findOpenTable(const string table_name);
  void addOpenTable(const string &table_name, FilesystemTableShare *);
  void deleteOpenTable(const string &table_name);

  uint32_t max_keys()          const { return 0; }
  uint32_t max_key_parts()     const { return 0; }
  uint32_t max_key_length()    const { return 0; }
  bool doDoesTableExist(Session& , TableIdentifier &);
  int doRenameTable(Session&, TableIdentifier &, TableIdentifier &) { return false;}
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

int FilesystemEngine::doDropTable(Session &, TableIdentifier &identifier)
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

bool FilesystemEngine::doDoesTableExist(Session &, TableIdentifier &identifier)
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
                               drizzled::TableIdentifier &identifier,
                               drizzled::message::Table &table_proto)
{
  string new_path;

  new_path= identifier.getPath();
  new_path+= FILESYSTEM_EXT;

  int fd= open(new_path.c_str(), O_RDONLY);

  if (fd == -1)
  {
    cerr << "can't open!" << endl;
    return errno;
  }

  google::protobuf::io::ZeroCopyInputStream* input=
    new google::protobuf::io::FileInputStream(fd);

  if (not input) {
    return HA_ERR_CRASHED_ON_USAGE;
  }

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
  : use_count(0), table_name(table_name_arg), update_file_opened(false)
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

    a_engine->addOpenTable(share->table_name, share);

    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
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
  : Cursor(engine_arg, table_arg),
  row_separator(DEFAULT_ROW_SEPARATOR),
  col_separator(DEFAULT_COL_SEPARATOR)
{
  file_buff= new TransparentFile();
}

int FilesystemCursor::open(const char *name, int, uint32_t)
{
  message::Table* table_proto = table->getShare()->getTableProto();

  real_file_name.clear();
  for (int i = 0; i < table_proto->engine().options_size(); i++)
  {
    const message::Engine::Option& option= table_proto->engine().options(i);

    if (boost::iequals(option.name(), FILESYSTEM_OPTION_FILE_PATH))
      real_file_name= option.state();
    else if (boost::iequals(option.name(), FILESYSTEM_OPTION_ROW_SEPARATOR))
      row_separator= option.state();
    else if (boost::iequals(option.name(), FILESYSTEM_OPTION_COL_SEPARATOR))
      col_separator= option.state();
  }

  if (real_file_name.empty())
    return ER_FILE_NOT_FOUND;
  file_desc= ::open(real_file_name.c_str(), O_RDONLY);
  if (file_desc < 0)
    return ER_CANT_OPEN_FILE;
  file_buff->init_buff(file_desc);

  if (!(share= get_share(name)))
  {
    ::close(file_desc);
    return ENOENT;
  }

  thr_lock_data_init(&share->lock, &lock, NULL);
  return 0;
}

int FilesystemCursor::close(void)
{
  ::close(file_desc);
  free_share();
  return 0;
}

int FilesystemCursor::doStartTableScan(bool)
{
  current_position= 0;
  next_position= 0;
  slots.clear();
  file_buff->init_buff(file_desc);
  return 0;
}

int FilesystemCursor::find_current_row(unsigned char *buf)
{
  ptrdiff_t row_offset= buf - table->record[0];

  current_position= next_position;

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
    bool is_row= (row_separator.find(ch) != string::npos);
    bool is_col= (col_separator.find(ch) != string::npos);
    if (is_row && content.empty() && line_blank)
      continue;

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
      if (row_separator.find(ch) != string::npos)
        line_done= true;
      ++next_position;
    }
  }
  return 0;
}

int FilesystemCursor::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  return find_current_row(buf);
}

void FilesystemCursor::position(const unsigned char *)
{
  internal::my_store_ptr(ref, ref_length, current_position);
}

int FilesystemCursor::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  current_position= (off_t)internal::my_get_ptr(pos,ref_length);
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
    if (stat(real_file_name.c_str(), &st) < 0)
      return -1;
    update_file_name= real_file_name;
    update_file_name.append(".UPDATE");
    update_file_desc= ::open(update_file_name.c_str(),
                             O_RDWR | O_CREAT | O_TRUNC,
                             st.st_mode);
    if (update_file_desc < 0)
    {
      cerr << "update file error!" << endl;
      return -1;
    }
    share->update_file_opened= true;
    update_file_length= 0;
  }
  return 0;
}

int FilesystemCursor::doEndTableScan()
{
  if (slots.size() == 0)
    return 0;

  if (openUpdateFile() < 0)
    return -1;

  sort(slots.begin(), slots.end());
  vector< pair<off_t, off_t> >::iterator slot_iter= slots.begin();
  file_buff->init_buff(file_desc);
  off_t write_start= 0;
  off_t write_end= 0;
  off_t file_buffer_start= 0;
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
    if (::write(update_file_desc,
                file_buff->ptr() + (write_start - file_buff->start()),
                write_length) != write_length)
    {
    }
    update_file_length+= write_length;
    cerr << "write: " << write_start << " -> " << write_end << endl;

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
    return -1;
  share->update_file_opened= false;

  // close current file
  ::close(file_desc);
  if (::rename(update_file_name.c_str(), real_file_name.c_str()))
    return -1;

  // reopen the data file
  file_desc= ::open(real_file_name.c_str(), O_RDONLY);
  if (file_desc < 0)
    return -1;
  return 0;
}

void FilesystemCursor::getAllFields(drizzled::String& output)
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
      output.append(col_separator[0]);
    }

    if (not (*field)->is_null())
    {
      (*field)->setReadSet();
      (*field)->val_str(&attribute, &attribute);

      output.append(attribute);
    }
    else
    {
      output.append("0");
    }
  }
  output.append(row_separator[0]);
}

int FilesystemCursor::doInsertRecord(unsigned char * buf)
{
  (void)buf;
  ha_statistic_increment(&system_status_var::ha_write_count);

  drizzled::String output_line;
  getAllFields(output_line);

  // write output_line to real file
  ofstream fout(real_file_name.c_str(), ios::app);
  if (not fout.is_open())
  {
    return -5;
  }
  fout.write(output_line.ptr(), output_line.length());
  fout.close();

  return 0;
}

int FilesystemCursor::doUpdateRecord(const unsigned char *, unsigned char *)
{
  ha_statistic_increment(&system_status_var::ha_update_count);

  if (openUpdateFile())
    return -1;

  addSlot();

  // get the update information
  drizzled::String output_line;
  getAllFields(output_line);

  if (::write(update_file_desc, output_line.ptr(), output_line.length())
      != output_line.length())
    return -1;
  update_file_length+= output_line.length();

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
  ha_statistic_increment(&system_status_var::ha_delete_count);
  addSlot();
  return 0;
}

bool FilesystemEngine::validateCreateTableOption(const std::string &key,
                                                 const std::string &state)
{
  if (boost::iequals(key, FILESYSTEM_OPTION_FILE_PATH))
  {
    if (state.empty() || ::access(state.c_str(), F_OK))
      return false;
    return true;
  }
  if ((boost::iequals(key, FILESYSTEM_OPTION_ROW_SEPARATOR) ||
       boost::iequals(key, FILESYSTEM_OPTION_COL_SEPARATOR)) &&
      ! state.empty())
    return true;
  return false;
}

int FilesystemEngine::doCreateTable(Session &,
                        Table&,
                        drizzled::TableIdentifier &identifier,
                        drizzled::message::Table &proto)
{
  string serialized_proto;
  string new_path;

  new_path= identifier.getPath();
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

  pthread_mutex_init(&filesystem_mutex, MY_MUTEX_INIT_FAST);
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
