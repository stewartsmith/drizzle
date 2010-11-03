/*
  Copyright (C) 2010 Zimin

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
#include <drizzled/field.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "filesystem_engine.h"
#include "utility.h"

#include <fcntl.h>

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace drizzled;

#define FILESYSTEM_EXT ".FST"

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
                                     HTON_SKIP_STORE_LOCK |
                                     HTON_CAN_INDEX_BLOBS |
                                     HTON_AUTO_PART_KEY),
     fs_open_tables()
  {
    table_definition_ext= FILESYSTEM_EXT;
    pthread_mutex_init(&filesystem_mutex, MY_MUTEX_INIT_FAST);
  }
  virtual ~FilesystemEngine()
  {
    pthread_mutex_destroy(&filesystem_mutex);
  }

  virtual Cursor *create(Table &table)
  {
    return new FilesystemCursor(*this, table);
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
                             const drizzled::SchemaIdentifier &schema_identifier,
                             drizzled::TableIdentifiers &set_of_identifiers);
private:
  void getTableNamesFromFilesystem(drizzled::CachedDirectory &directory,
                                   const drizzled::SchemaIdentifier &schema_identifier,
                                   drizzled::plugin::TableNameList *set_of_names,
                                   drizzled::TableIdentifiers *set_of_identifiers);
};

void FilesystemEngine::getTableNamesFromFilesystem(drizzled::CachedDirectory &directory,
                                                   const drizzled::SchemaIdentifier &schema_identifier,
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

void FilesystemEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                                             const drizzled::SchemaIdentifier &schema_identifier,
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

static int parseTaggedFile(const FormatInfo &fi, vector< map<string, string> > &v)
{
  int filedesc= ::open(fi.getFileName().c_str(), O_RDONLY);
  if (filedesc < 0)
    return errno;

  boost::scoped_ptr<TransparentFile> filebuffer(new TransparentFile);
  filebuffer->init_buff(filedesc);

  bool last_line_empty= false;
  map<string, string> kv;
  int pos= 0;
  string line;
  while (1)
  {
    char ch= filebuffer->get_value(pos);
    if (ch == '\0')
    {
      if (!last_line_empty)
      {
        v.push_back(kv);
        kv.clear();
      }
      break;
    }
    ++pos;

    if (!fi.isRowSeparator(ch))
    {
      line.push_back(ch);
      continue;
    }

    // if we have a new empty line,
    // it means we got the end of a section, push it to vector
    if (line.empty())
    {
      if (!last_line_empty)
      {
        v.push_back(kv);
        kv.clear();
      }
      last_line_empty= true;
      continue;
    }

    // parse the line
    vector<string> sv, svcopy;
    boost::split(sv, line, boost::is_any_of(fi.getColSeparator()));
    for (vector<string>::iterator iter= sv.begin();
         iter != sv.end();
         ++iter)
    {
      if (!iter->empty())
        svcopy.push_back(*iter);
    }

    // the first splitted string as key,
    // and the second splitted string as value.
    string key(svcopy[0]);
    boost::trim(key);
    if (svcopy.size() >= 2)
    {
      string value(svcopy[1]);
      boost::trim(value);
      kv[key]= value;
    }
    else if (svcopy.size() >= 1)
      kv[key]= "";

    last_line_empty= false;
    line.clear();
  }
  close(filedesc);
  return 0;
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

  // if the file is a tagged file such as /proc/meminfo
  // then columns of this table are added dynamically here.
  FormatInfo format;
  format.parseFromTable(&table_proto);
  if (!format.isTagFormat() || !format.isFileGiven()) {
    close(fd);
    return EEXIST;
  }

  vector< map<string, string> > vm;
  if (parseTaggedFile(format, vm) != 0) {
    close(fd);

    return EEXIST;
  }
  if (vm.size() == 0) {
    close(fd);
    return EEXIST;
  }

  // we don't care what user provides, just clear them all
  table_proto.clear_field();
  // we take the first section as sample
  map<string, string> kv= vm[0];
  for (map<string, string>::iterator iter= kv.begin();
       iter != kv.end();
       ++iter)
  {
    // add columns to table proto
    message::Table::Field *field= table_proto.add_field();
    field->set_name(iter->first);
    field->set_type(drizzled::message::Table::Field::VARCHAR);
    message::Table::Field::StringFieldOptions *stringoption= field->mutable_string_options();
    stringoption->set_length(iter->second.length() + 1);
  }

  close(fd);
  return EEXIST;
}

FilesystemTableShare::FilesystemTableShare(const string table_name_arg)
  : use_count(0), table_name(table_name_arg),
  update_file_opened(false),
  needs_reopen(false)
{
}

FilesystemTableShare::~FilesystemTableShare()
{
  pthread_mutex_destroy(&mutex);
}

FilesystemTableShare *FilesystemCursor::get_share(const char *table_name)
{
  Guard g(filesystem_mutex);

  FilesystemEngine *a_engine= static_cast<FilesystemEngine *>(getEngine());
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
      return NULL;
    }

    share->format.parseFromTable(getTable()->getShare()->getTableProto());
    if (!share->format.isFileGiven())
    {
      return NULL;
    }
    /*
     * for taggered file such as /proc/meminfo,
     * we pre-process it first, and store the parsing result in a map.
     */
    if (share->format.isTagFormat())
    {
      if (parseTaggedFile(share->format, share->vm) != 0)
      {
        return NULL;
      }
    }
    a_engine->addOpenTable(share->table_name, share);

    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  }
  share->use_count++;

  return share;
}

void FilesystemCursor::free_share()
{
  Guard g(filesystem_mutex);

  if (!--share->use_count){
    FilesystemEngine *a_engine= static_cast<FilesystemEngine *>(getEngine());
    a_engine->deleteOpenTable(share->table_name);
    pthread_mutex_destroy(&share->mutex);
    delete share;
  }
}

void FilesystemCursor::critical_section_enter()
{
  if (sql_command_type == SQLCOM_ALTER_TABLE ||
      sql_command_type == SQLCOM_UPDATE ||
      sql_command_type == SQLCOM_DELETE ||
      sql_command_type == SQLCOM_INSERT ||
      sql_command_type == SQLCOM_INSERT_SELECT ||
      sql_command_type == SQLCOM_REPLACE ||
      sql_command_type == SQLCOM_REPLACE_SELECT)
    share->filesystem_lock.scan_update_begin();
  else
    share->filesystem_lock.scan_begin();

  thread_locked = true;
}

void FilesystemCursor::critical_section_exit()
{
  if (sql_command_type == SQLCOM_ALTER_TABLE ||
      sql_command_type == SQLCOM_UPDATE ||
      sql_command_type == SQLCOM_DELETE ||
      sql_command_type == SQLCOM_INSERT ||
      sql_command_type == SQLCOM_INSERT_SELECT ||
      sql_command_type == SQLCOM_REPLACE ||
      sql_command_type == SQLCOM_REPLACE_SELECT)
    share->filesystem_lock.scan_update_end();
  else
    share->filesystem_lock.scan_end();

  thread_locked = false;
}

FilesystemCursor::FilesystemCursor(drizzled::plugin::StorageEngine &engine_arg, Table &table_arg)
  : Cursor(engine_arg, table_arg),
    file_buff(new TransparentFile),
    thread_locked(false)
{
}

int FilesystemCursor::doOpen(const drizzled::TableIdentifier &identifier, int, uint32_t)
{
  if (!(share= get_share(identifier.getPath().c_str())))
    return ENOENT;

  file_desc= ::open(share->format.getFileName().c_str(), O_RDONLY);
  if (file_desc < 0)
  {
    free_share();
    return ER_CANT_OPEN_FILE;
  }

  ref_length= sizeof(off_t);
  return 0;
}

int FilesystemCursor::close(void)
{
  int err= ::close(file_desc);
  if (err < 0)
    err= errno;
  free_share();
  return err;
}

int FilesystemCursor::doStartTableScan(bool)
{
  sql_command_type = session_sql_command(getTable()->getSession());

  if (thread_locked)
    critical_section_exit();
  critical_section_enter();

  if (share->format.isTagFormat())
  {
    tag_depth= 0;
    return 0;
  }

  current_position= 0;
  next_position= 0;
  slots.clear();
  if (share->needs_reopen)
  {
    file_desc= ::open(share->format.getFileName().c_str(), O_RDONLY);
    if (file_desc < 0)
      return HA_ERR_CRASHED_ON_USAGE;
    share->needs_reopen= false;
  }
  file_buff->init_buff(file_desc);
  return 0;
}

int FilesystemCursor::find_current_row(unsigned char *buf)
{
  ptrdiff_t row_offset= buf - getTable()->record[0];

  next_position= current_position;

  string content;
  bool line_done= false;
  bool line_blank= true;
  Field **field= getTable()->getFields();
  for (; !line_done && *field; ++next_position)
  {
    char ch= file_buff->get_value(next_position);
    if (ch == '\0')
      return HA_ERR_END_OF_FILE;

    if (share->format.isEscapedChar(ch))
    {
      // read next character
      ch= file_buff->get_value(++next_position);
      if (ch == '\0')
        return HA_ERR_END_OF_FILE;

      content.push_back(FormatInfo::getEscapedChar(ch));

      continue;
    }

    // if we find separator
    bool is_row= share->format.isRowSeparator(ch);
    bool is_col= share->format.isColSeparator(ch);
    if (content.empty())
    {
      if (share->format.isSeparatorModeGeneral() && is_row && line_blank)
        continue;
      if (share->format.isSeparatorModeWeak() && is_col)
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
      if (share->format.isRowSeparator(ch))
        line_done= true;
      ++next_position;
    }
  }
  return 0;
}

int FilesystemCursor::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  if (share->format.isTagFormat())
  {
    if (tag_depth >= share->vm.size())
      return HA_ERR_END_OF_FILE;

    ptrdiff_t row_offset= buf - getTable()->record[0];
    for (Field **field= getTable()->getFields(); *field; field++)
    {
      string key((*field)->field_name);
      string content= share->vm[tag_depth][key];

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
        {
          (*field)->set_default();
        }
      }
      else
      {
        (*field)->set_null();
      }
      (*field)->move_field_offset(-row_offset);
    }
    ++tag_depth;
    return 0;
  }
  // normal file
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
    if (stat(share->format.getFileName().c_str(), &st) < 0)
      return -1;
    update_file_name= share->format.getFileName();
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
  sql_command_type = session_sql_command(getTable()->getSession());

  if (share->format.isTagFormat())
  {
    if (thread_locked)
      critical_section_exit();
    return 0;
  }

  if (slots.size() == 0)
  {
    if (thread_locked)
      critical_section_exit();
    return 0;
  }

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
    if (write_in_all(update_file_desc,
               file_buff->ptr() + (write_start - file_buff->start()),
               write_length) != write_length)
      goto error;

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
    goto error;
  share->update_file_opened= false;

  // close current file
  if (::close(file_desc))
    goto error;
  if (::rename(update_file_name.c_str(), share->format.getFileName().c_str()))
    goto error;

  share->needs_reopen= true;

error:
  err= errno;
  pthread_mutex_unlock(&share->mutex);

  if (thread_locked)
    critical_section_exit();

  return err;
}

void FilesystemCursor::recordToString(string& output)
{
  bool first= true;
  drizzled::String attribute;
  for (Field **field= getTable()->getFields(); *field; ++field)
  {
    if (first == true)
    {
      first= false;
    }
    else
    {
      output.append(share->format.getColSeparatorHead());
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
  output.append(share->format.getRowSeparatorHead());
}

int FilesystemCursor::doInsertRecord(unsigned char * buf)
{
  (void)buf;

  if (share->format.isTagFormat())
    return 0;

  sql_command_type = session_sql_command(getTable()->getSession());

  critical_section_enter();

  int err_write= 0;
  int err_close= 0;

  string output_line;
  recordToString(output_line);

  int fd= ::open(share->format.getFileName().c_str(), O_WRONLY | O_APPEND);
  if (fd < 0)
  {
    critical_section_exit();
    return ENOENT;
  }

  err_write= write_in_all(fd, output_line.c_str(), output_line.length());
  if (err_write < 0)
    err_write= errno;
  else
    err_write= 0;

  err_close= ::close(fd);
  if (err_close < 0)
    err_close= errno;

  critical_section_exit();

  if (err_write)
    return err_write;
  if (err_close)
    return err_close;
  return 0;
}

int FilesystemCursor::doUpdateRecord(const unsigned char *, unsigned char *)
{
  if (share->format.isTagFormat())
    return 0;
  if (openUpdateFile())
    return errno;

  // get the update information
  string str;
  recordToString(str);

  if (write_in_all(update_file_desc, str.c_str(), str.length()) < 0)
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
  if (share->format.isTagFormat())
    return 0;
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
  return FormatInfo::validateOption(key, state);
}

int FilesystemEngine::doCreateTable(Session &,
                        Table&,
                        const drizzled::TableIdentifier &identifier,
                        drizzled::message::Table &proto)
{
  FormatInfo format;
  format.parseFromTable(&proto);
  if (format.isFileGiven())
  {
    int err= ::open(format.getFileName().c_str(), O_RDONLY);
    if (err < 0)
      return errno;
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
  filesystem_engine= new FilesystemEngine("FILESYSTEM");
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
