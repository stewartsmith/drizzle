/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 ziminq
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

#define FST ".FST"

/* Stuff for shares */
pthread_mutex_t filesystem_mutex;

static const char *ha_filesystem_exts[] = {
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
    table_definition_ext = FST;  // should we set this first?
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
};

void FilesystemEngine::doGetTableNames(drizzled::CachedDirectory &directory,
                                       drizzled::SchemaIdentifier &, /* database name? */
                                       drizzled::plugin::TableNameList &set_of_names)
{
  drizzled::CachedDirectory::Entries entries= directory.getEntries();

  for (drizzled::CachedDirectory::Entries::iterator entry_iter= entries.begin();
      entry_iter != entries.end(); ++entry_iter)
  {
    drizzled::CachedDirectory::Entry *entry= *entry_iter;
    const string *filename= &entry->filename;

    assert(filename->size());

    const char *ext= strchr(filename->c_str(), '.');

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, FST) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0))
    {  }
    else
    {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len= TableIdentifier::filename_to_tablename(filename->c_str(), uname, sizeof(uname));
      uname[file_name_len - sizeof(FST) + 1]= '\0'; // Subtract ending, place NULL
      set_of_names.insert(uname);
    }
  }
}

int FilesystemEngine::doDropTable(Session &, TableIdentifier &identifier)
{
  string new_path(identifier.getPath());
  new_path+= FST;
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
  proto_path.append(FST);

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
  new_path+= FST;

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

void FilesystemEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                           drizzled::SchemaIdentifier &schema_identifier,
                           drizzled::TableIdentifiers &set_of_identifiers)
{
  drizzled::CachedDirectory::Entries entries= directory.getEntries();

  for (drizzled::CachedDirectory::Entries::iterator entry_iter= entries.begin();
       entry_iter != entries.end(); ++entry_iter)
  {
    drizzled::CachedDirectory::Entry *entry= *entry_iter;
    const string *filename= &entry->filename;

    assert(filename->size());

    const char *ext= strchr(filename->c_str(), '.');

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, FST) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0))
    {  }
    else
    {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len= TableIdentifier::filename_to_tablename(filename->c_str(), uname, sizeof(uname));
      uname[file_name_len - sizeof(FST) + 1]= '\0'; // Subtract ending, place NULL
      set_of_identifiers.push_back(TableIdentifier(schema_identifier, uname));
    }
  }
}

FilesystemTableShare::FilesystemTableShare(const string table_name_arg)
  : use_count(0), table_name(table_name_arg)
{
  thr_lock_init(&lock);
}

FilesystemTableShare::~FilesystemTableShare()
{
  thr_lock_delete(&lock);
  pthread_mutex_destroy(&mutex);
}

/*
  Simple lock controls.
*/
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

    // initialize FormatInfo if necessary
    // initialize per share mutex
    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&filesystem_mutex);

  return share;
}

FilesystemCursor::FilesystemCursor(drizzled::plugin::StorageEngine &engine_arg, TableShare &table_arg)
  :Cursor(engine_arg, table_arg)
{
}

/*
  Open a database file. Keep in mind that tables are caches, so
  this will not be called for every request. Any sort of positions
  that need to be reset should be kept in the ::extra() call.
*/
int FilesystemCursor::open(const char *name, int, uint32_t)
{
  if (!(share= get_share(name)))
    return HA_ERR_OUT_OF_MEM; // TODO: code style???

  string db_path = share->table_name + FST;
  fd.open(db_path.c_str());
  if (not fd.is_open())
  {
    return -2; // TODO more correct return value;
  }
  message::Table table_proto;
  if (not table_proto.ParseFromIstream(&fd))
  {
    fd.close();
    return -3; // TODO more correct return value;
  }
  fd.close();
  for (int i = 0; i < table_proto.engine().options_size(); i++)
  {
    if (boost::iequals(table_proto.engine().options(i).name(), "FILE"))
    {
      real_file_name = table_proto.engine().options(i).state();
    }
    else if (boost::iequals(table_proto.engine().options(i).name(), "SEP"))
    {
      sep = table_proto.engine().options(i).state();
    }
  }
  thr_lock_data_init(&share->lock, &lock, NULL);
  return 0;
}

/*
  Close a database file. We remove ourselves from the shared strucutre.
  If it is empty we destroy it.
*/
int FilesystemCursor::close(void)
{
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

int FilesystemCursor::doStartTableScan(bool)
{
  // open the real file
  fd.open(real_file_name.c_str());
  if (not fd.is_open())
    cerr << "can't open " << real_file_name << " file." << endl;
  return 0;
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
int FilesystemCursor::rnd_next(unsigned char *buf)
{
  drizzled::String buffer;
  string line;

  if (!fd.is_open())
    return HA_ERR_END_OF_FILE;

  prev_pos = fd.tellg();
  if (!getline(fd, line))
    return HA_ERR_END_OF_FILE;

  memset(buf, 0, table->s->null_bytes); //getNullBytes()

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sepa(sep == "" ? " \t" : sep.c_str());
  tokenizer tokens(line, sepa);
  tokenizer::iterator tok_iter = tokens.begin();
  for (Field **field = table->getFields();
       *field && tok_iter != tokens.end();
       ++field, ++tok_iter)
  {
    buffer.length(0);
    string word = *tok_iter;
    cerr << "field: " << word << endl;
    buffer.append(word.c_str());
    if ((*field)->isReadSet() || (*field)->isWriteSet())
    {
      // this is very important to use 'select',
      // as said in csv file, it's a bug in select ???
      (*field)->setWriteSet();
      (*field)->store(buffer.ptr()/* pointer */,
                      buffer.length()/* length */,
                      buffer.charset()/* charset */,
                      CHECK_FIELD_WARN/* check flag */);
    }
  }
  return 0;
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
void FilesystemCursor::position(const unsigned char *)
{
  return;
}


/*
  Used to fetch a row from a posiion stored with ::position().
  internal::my_get_ptr() retrieves the data for you.
*/

int FilesystemCursor::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  (void)buf;
  (void)pos;
  return 0;
}

/*
  ::info() is used to return information to the optimizer.
  Currently this table Cursor doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
int FilesystemCursor::info(uint32_t)
{
  return 0;
}

int FilesystemCursor::doEndTableScan()
{
  // close the real file
  fd.close();
  return 0;
}

void FilesystemCursor::getAllFields(drizzled::String& output)
{
  bool first = true;
  drizzled::String attribute;
  string s = getSeparator();
  for (Field **field= table->getFields(); *field; ++field)
  {
    if (first == true)
    {
      first = false;
    }
    else
    {
      output.append(s.c_str());
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
  output.append("\n");
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

int FilesystemCursor::updateRealFile(const char *buf, size_t len)
{
  // make up the name of temp file
  string temp_file_name = real_file_name + ".TEMP";

  ifstream fin(real_file_name.c_str());
  ofstream fout(temp_file_name.c_str());
  if (not fin.is_open() || not fout.is_open())
    return HA_ERR_CRASHED_ON_USAGE;

  string line;
  while (fin.tellg() < prev_pos && getline(fin, line))
  {
    line+= "\n";
    fout.write(line.c_str(), line.length());
  }
  // omit this line
  getline(fin, line);
  // update this line if necessary
  if (buf)
  {
    fout.write(buf, len);
  }
  while (getline(fin, line))
  {
    line+= "\n";
    fout.write(line.c_str(), line.length());
  }
  fin.close();
  fout.close();
  // rename the temp file
  int err = rename(temp_file_name.c_str(), real_file_name.c_str());
  if (err != 0)
    return HA_ERR_CRASHED_ON_USAGE;
  return 0;
}

string FilesystemCursor::getSeparator()
{
  char ch;
  if (sep.length() == 0)
    ch = ' ';
  else
    ch = sep[0];
  return string(1, ch);
}

int FilesystemCursor::doUpdateRecord(const unsigned char *, unsigned char *)
{
  if (!fd.is_open())
    return HA_ERR_END_OF_FILE;

  // get the update information
  drizzled::String output_line;
  getAllFields(output_line);

  int err = updateRealFile(output_line.ptr(), output_line.length());
  if (err)
    return HA_ERR_CRASHED_ON_USAGE;

  // re-open this file
  fd.open(real_file_name.c_str());
  if (not fd.is_open())
    return HA_ERR_CRASHED_ON_USAGE;
  fd.seekg(prev_pos);

  return 0;
}

int FilesystemCursor::doDeleteRecord(const unsigned char *)
{
  if (not fd.is_open())
    return HA_ERR_END_OF_FILE;

  // close this file first, as we're scanning this file
  fd.close();

  int err = updateRealFile(NULL, 0);
  if (err)
    return HA_ERR_CRASHED_ON_USAGE;

  // re-open this file
  fd.open(real_file_name.c_str());
  if (not fd.is_open())
    return HA_ERR_CRASHED_ON_USAGE;
  fd.seekg(prev_pos);

  return 0;
}

bool FilesystemEngine::validateCreateTableOption(const std::string &key,
                                                 const std::string &state)
{
  (void)state;
  // FILE and SEP
  if (boost::iequals(key, "FILE"))
  {
    return true;
  }
  else if (boost::iequals(key, "SEP"))
  {
    return true;
  }
  return false;
}

/*
  Create a table. You do not want to leave the table open after a call to
  this (the database will call ::open() if it needs to).
*/
int FilesystemEngine::doCreateTable(Session &,
                        Table&,
                        drizzled::TableIdentifier &identifier,
                        drizzled::message::Table &proto)
{
  string serialized_proto;
  string new_path;

  // check for option proto.engine().options(i).name() / state()

  new_path= identifier.getPath();
  new_path+= FST;
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
  "ziminq",
  "Filesystem Engine",
  PLUGIN_LICENSE_GPL,
  filesystem_init_func, /* Plugin Init */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;
