/* Copyright (C) 2003 MySQL AB
   Copyright (C) 2010 Brian Aker

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


#include "config.h"

#include "plugin/archive/archive_engine.h"

using namespace std;
using namespace drizzled;


/*
  First, if you want to understand storage engines you should look at
  ha_example.cc and ha_example.h.

  This example was written as a test case for a customer who needed
  a storage engine without indexes that could compress data very well.
  So, welcome to a completely compressed storage engine. This storage
  engine only does inserts. No replace, deletes, or updates. All reads are
  complete table scans. Compression is done through a combination of packing
  and making use of the zlib library

  We keep a file pointer open for each instance of ha_archive for each read
  but for writes we keep one open file handle just for that. We flush it
  only if we have a read occur. azip handles compressing lots of records
  at once much better then doing lots of little records between writes.
  It is possible to not lock on writes but this would then mean we couldn't
  handle bulk inserts as well (that is if someone was trying to read at
  the same time since we would want to flush).

  A "meta" file is kept alongside the data file. This file serves two purpose.
  The first purpose is to track the number of rows in the table. The second
  purpose is to determine if the table was closed properly or not. When the
  meta file is first opened it is marked as dirty. It is opened when the table
  itself is opened for writing. When the table is closed the new count for rows
  is written to the meta file and the file is marked as clean. If the meta file
  is opened and it is marked as dirty, it is assumed that a crash occured. At
  this point an error occurs and the user is told to rebuild the file.
  A rebuild scans the rows and rewrites the meta file. If corruption is found
  in the data file then the meta file is not repaired.

  At some point a recovery method for such a drastic case needs to be divised.

  Locks are row level, and you will get a consistant read.

  For performance as far as table scans go it is quite fast. I don't have
  good numbers but locally it has out performed both Innodb and MyISAM. For
  Innodb the question will be if the table can be fit into the buffer
  pool. For MyISAM its a question of how much the file system caches the
  MyISAM file. With enough free memory MyISAM is faster. Its only when the OS
  doesn't have enough memory to cache entire table that archive turns out
  to be any faster.

  Examples between MyISAM (packed) and Archive.

  Table with 76695844 identical rows:
  29680807 a_archive.ARZ
  920350317 a.MYD


  Table with 8991478 rows (all of Slashdot's comments):
  1922964506 comment_archive.ARZ
  2944970297 comment_text.MYD


  TODO:
   Allow users to set compression level.
   Allow adjustable block size.
   Implement versioning, should be easy.
   Allow for errors, find a way to mark bad rows.
   Add optional feature so that rows can be flushed at interval (which will cause less
     compression but may speed up ordered searches).
   Checkpoint the meta file to allow for faster rebuilds.
   Option to allow for dirty reads, this would lower the sync calls, which would make
     inserts a lot faster, but would mean highly arbitrary reads.

    -Brian
*/

/* When the engine starts up set the first version */
static uint64_t global_version= 1;

// We use this to find out the state of the archive aio option.
extern bool archive_aio_state(void);

/*
  Number of rows that will force a bulk insert.
*/
#define ARCHIVE_MIN_ROWS_TO_USE_BULK_INSERT 2

/*
  Size of header used for row
*/
#define ARCHIVE_ROW_HEADER_SIZE 4

ArchiveShare *ArchiveEngine::findOpenTable(const string table_name)
{
  ArchiveMap::iterator find_iter=
    archive_open_tables.find(table_name);

  if (find_iter != archive_open_tables.end())
    return (*find_iter).second;
  else
    return NULL;
}

void ArchiveEngine::addOpenTable(const string &table_name, ArchiveShare *share)
{
  archive_open_tables[table_name]= share;
}

void ArchiveEngine::deleteOpenTable(const string &table_name)
{
  archive_open_tables.erase(table_name);
}


int ArchiveEngine::doDropTable(Session&, const TableIdentifier &identifier)
{
  string new_path(identifier.getPath());

  new_path+= ARZ;

  int error= unlink(new_path.c_str());

  if (error != 0)
  {
    error= errno= errno;
  }

  return error;
}

int ArchiveEngine::doGetTableDefinition(Session&,
                                        const TableIdentifier &identifier,
                                        drizzled::message::Table &table_proto)
{
  struct stat stat_info;
  int error= ENOENT;
  string proto_path;

  proto_path.reserve(FN_REFLEN);
  proto_path.assign(identifier.getPath());

  proto_path.append(ARZ);

  if (stat(proto_path.c_str(),&stat_info))
    return errno;
  else
    error= EEXIST;

  {
    azio_stream proto_stream;
    char* proto_string;
    if (azopen(&proto_stream, proto_path.c_str(), O_RDONLY, AZ_METHOD_BLOCK) == 0)
      return HA_ERR_CRASHED_ON_USAGE;

    proto_string= (char*)malloc(sizeof(char) * proto_stream.frm_length);
    if (proto_string == NULL)
    {
      azclose(&proto_stream);
      return ENOMEM;
    }

    azread_frm(&proto_stream, proto_string);

    if (table_proto.ParseFromArray(proto_string, proto_stream.frm_length) == false)
      error= HA_ERR_CRASHED_ON_USAGE;

    azclose(&proto_stream);
    free(proto_string);
  }

  /* We set the name from what we've asked for as in RENAME TABLE for ARCHIVE
     we do not rewrite the table proto (as it's wedged in the file header)
  */
  table_proto.set_schema(identifier.getSchemaName());
  table_proto.set_name(identifier.getTableName());

  return error;
}


ha_archive::ha_archive(drizzled::plugin::StorageEngine &engine_arg,
                       Table &table_arg)
  :Cursor(engine_arg, table_arg), delayed_insert(0), bulk_insert(0)
{
  /* Set our original buffer from pre-allocated memory */
  buffer.set((char *)byte_buffer, IO_SIZE, system_charset_info);

  /* The size of the offset value we will use for position() */
  ref_length= sizeof(internal::my_off_t);
  archive_reader_open= false;
}

/*
  This method reads the header of a datafile and returns whether or not it was successful.
*/
int ha_archive::read_data_header(azio_stream *file_to_read)
{
  if (azread_init(file_to_read) == -1)
    return(HA_ERR_CRASHED_ON_USAGE);

  if (file_to_read->version >= 3)
    return(0);

  return(1);
}

ArchiveShare::ArchiveShare():
  use_count(0), archive_write_open(false), dirty(false), crashed(false),
  mean_rec_length(0), version(0), rows_recorded(0), version_rows(0)
{
  assert(1);
}

ArchiveShare::ArchiveShare(const char *name):
  use_count(0), archive_write_open(false), dirty(false), crashed(false),
  mean_rec_length(0), version(0), rows_recorded(0), version_rows(0)
{
  memset(&archive_write, 0, sizeof(azio_stream));     /* Archive file we are working with */
  table_name.append(name);
  data_file_name.assign(table_name);
  data_file_name.append(ARZ);
  /*
    We will use this lock for rows.
  */
  pthread_mutex_init(&_mutex,MY_MUTEX_INIT_FAST);
}

ArchiveShare::~ArchiveShare()
{
  _lock.deinit();
  pthread_mutex_destroy(&_mutex);
  /*
    We need to make sure we don't reset the crashed state.
    If we open a crashed file, wee need to close it as crashed unless
    it has been repaired.
    Since we will close the data down after this, we go on and count
    the flush on close;
  */
  if (archive_write_open == true)
    (void)azclose(&archive_write);
}

bool ArchiveShare::prime(uint64_t *auto_increment)
{
  azio_stream archive_tmp;

  /*
    We read the meta file, but do not mark it dirty. Since we are not
    doing a write we won't mark it dirty (and we won't open it for
    anything but reading... open it for write and we will generate null
    compression writes).
  */
  if (!(azopen(&archive_tmp, data_file_name.c_str(), O_RDONLY,
               AZ_METHOD_BLOCK)))
    return false;

  *auto_increment= archive_tmp.auto_increment + 1;
  rows_recorded= (ha_rows)archive_tmp.rows;
  crashed= archive_tmp.dirty;
  if (version < global_version)
  {
    version_rows= rows_recorded;
    version= global_version;
  }
  azclose(&archive_tmp);

  return true;
}


/*
  We create the shared memory space that we will use for the open table.
  No matter what we try to get or create a share. This is so that a repair
  table operation can occur.

  See ha_example.cc for a longer description.
*/
ArchiveShare *ha_archive::get_share(const char *table_name, int *rc)
{
  ArchiveEngine *a_engine= static_cast<ArchiveEngine *>(getEngine());

  pthread_mutex_lock(&a_engine->mutex());

  share= a_engine->findOpenTable(table_name);

  if (!share)
  {
    share= new ArchiveShare(table_name);

    if (share == NULL)
    {
      pthread_mutex_unlock(&a_engine->mutex());
      *rc= HA_ERR_OUT_OF_MEM;
      return(NULL);
    }

    if (share->prime(&stats.auto_increment_value) == false)
    {
      pthread_mutex_unlock(&a_engine->mutex());
      *rc= HA_ERR_CRASHED_ON_REPAIR;
      delete share;

      return NULL;
    }

    a_engine->addOpenTable(share->table_name, share);
    thr_lock_init(&share->_lock);
  }
  share->use_count++;

  if (share->crashed)
    *rc= HA_ERR_CRASHED_ON_USAGE;
  pthread_mutex_unlock(&a_engine->mutex());

  return(share);
}


/*
  Free the share.
  See ha_example.cc for a description.
*/
int ha_archive::free_share()
{
  ArchiveEngine *a_engine= static_cast<ArchiveEngine *>(getEngine());

  pthread_mutex_lock(&a_engine->mutex());
  if (!--share->use_count)
  {
    a_engine->deleteOpenTable(share->table_name);
    delete share;
  }
  pthread_mutex_unlock(&a_engine->mutex());

  return 0;
}

int ha_archive::init_archive_writer()
{
  /*
    It is expensive to open and close the data files and since you can't have
    a gzip file that can be both read and written we keep a writer open
    that is shared amoung all open tables.
  */
  if (!(azopen(&(share->archive_write), share->data_file_name.c_str(),
               O_RDWR, AZ_METHOD_BLOCK)))
  {
    share->crashed= true;
    return(1);
  }
  share->archive_write_open= true;

  return(0);
}


/*
  No locks are required because it is associated with just one Cursor instance
*/
int ha_archive::init_archive_reader()
{
  /*
    It is expensive to open and close the data files and since you can't have
    a gzip file that can be both read and written we keep a writer open
    that is shared amoung all open tables.
  */
  if (archive_reader_open == false)
  {
    az_method method;

    if (archive_aio_state())
    {
      method= AZ_METHOD_AIO;
    }
    else
    {
      method= AZ_METHOD_BLOCK;
    }
    if (!(azopen(&archive, share->data_file_name.c_str(), O_RDONLY,
                 method)))
    {
      share->crashed= true;
      return(1);
    }
    archive_reader_open= true;
  }

  return(0);
}

/*
  When opening a file we:
  Create/get our shared structure.
  Init out lock.
  We open the file we will read from.
*/
int ha_archive::doOpen(const TableIdentifier &identifier, int , uint32_t )
{
  int rc= 0;
  share= get_share(identifier.getPath().c_str(), &rc);

  /** 
    We either fix it ourselves, or we just take it offline 

    @todo Create some documentation in the recovery tools shipped with the engine.
  */
  if (rc == HA_ERR_CRASHED_ON_USAGE)
  {
    free_share();
    rc= repair();

    return 0;
  }
  else if (rc == HA_ERR_OUT_OF_MEM)
  {
    return(rc);
  }

  assert(share);

  record_buffer.resize(getTable()->getShare()->getRecordLength() + ARCHIVE_ROW_HEADER_SIZE);

  lock.init(&share->_lock);

  return(rc);
}

// Should never be called
int ha_archive::open(const char *, int, uint32_t)
{
  assert(0);
  return -1;
}


/*
  Closes the file.

  SYNOPSIS
    close();

  IMPLEMENTATION:

  We first close this storage engines file handle to the archive and
  then remove our reference count to the table (and possibly free it
  as well).

  RETURN
    0  ok
    1  Error
*/

int ha_archive::close(void)
{
  int rc= 0;

  record_buffer.clear();

  /* First close stream */
  if (archive_reader_open == true)
  {
    if (azclose(&archive))
      rc= 1;
  }
  /* then also close share */
  rc|= free_share();

  return(rc);
}


/*
  We create our data file here. The format is pretty simple.
  You can read about the format of the data file above.
  Unlike other storage engines we do not "pack" our data. Since we
  are about to do a general compression, packing would just be a waste of
  CPU time. If the table has blobs they are written after the row in the order
  of creation.
*/

int ArchiveEngine::doCreateTable(Session &,
                                 Table& table_arg,
                                 const drizzled::TableIdentifier &identifier,
                                 drizzled::message::Table& proto)
{
  int error= 0;
  azio_stream create_stream;            /* Archive file we are working with */
  uint64_t auto_increment_value;
  string serialized_proto;

  auto_increment_value= proto.options().auto_increment_value();

  for (uint32_t key= 0; key < table_arg.sizeKeys(); key++)
  {
    KeyInfo *pos= &table_arg.key_info[key];
    KeyPartInfo *key_part=     pos->key_part;
    KeyPartInfo *key_part_end= key_part + pos->key_parts;

    for (; key_part != key_part_end; key_part++)
    {
      Field *field= key_part->field;

      if (!(field->flags & AUTO_INCREMENT_FLAG))
      {
        return -1;
      }
    }
  }

  std::string named_file= identifier.getPath();
  named_file.append(ARZ);

  errno= 0;
  if (azopen(&create_stream, named_file.c_str(), O_CREAT|O_RDWR,
             AZ_METHOD_BLOCK) == 0)
  {
    error= errno;
    unlink(named_file.c_str());

    return(error ? error : -1);
  }

  try {
    proto.SerializeToString(&serialized_proto);
  }
  catch (...)
  {
    unlink(named_file.c_str());

    return(error ? error : -1);
  }

  if (azwrite_frm(&create_stream, serialized_proto.c_str(),
                  serialized_proto.length()))
  {
    unlink(named_file.c_str());

    return(error ? error : -1);
  }

  if (proto.options().has_comment())
  {
    int write_length;

    write_length= azwrite_comment(&create_stream,
                                  proto.options().comment().c_str(),
                                  proto.options().comment().length());

    if (write_length < 0)
    {
      error= errno;
      unlink(named_file.c_str());

      return(error ? error : -1);
    }
  }

  /*
    Yes you need to do this, because the starting value
    for the autoincrement may not be zero.
  */
  create_stream.auto_increment= auto_increment_value ?
    auto_increment_value - 1 : 0;

  if (azclose(&create_stream))
  {
    error= errno;
    unlink(named_file.c_str());

    return(error ? error : -1);
  }

  return(0);
}

/*
  This is where the actual row is written out.
*/
int ha_archive::real_write_row(unsigned char *buf, azio_stream *writer)
{
  off_t written;
  unsigned int r_pack_length;

  /* We pack the row for writing */
  r_pack_length= pack_row(buf);

  written= azwrite_row(writer, &record_buffer[0], r_pack_length);
  if (written != r_pack_length)
  {
    return(-1);
  }

  if (!delayed_insert || !bulk_insert)
    share->dirty= true;

  return(0);
}


/*
  Calculate max length needed for row. This includes
  the bytes required for the length in the header.
*/

uint32_t ha_archive::max_row_length(const unsigned char *)
{
  uint32_t length= (uint32_t)(getTable()->getRecordLength() + getTable()->sizeFields()*2);
  length+= ARCHIVE_ROW_HEADER_SIZE;

  uint32_t *ptr, *end;
  for (ptr= getTable()->getBlobField(), end=ptr + getTable()->sizeBlobFields();
       ptr != end ;
       ptr++)
  {
      length += 2 + ((Field_blob*)getTable()->getField(*ptr))->get_length();
  }

  return length;
}


unsigned int ha_archive::pack_row(unsigned char *record)
{
  unsigned char *ptr;

  if (fix_rec_buff(max_row_length(record)))
    return(HA_ERR_OUT_OF_MEM);

  /* Copy null bits */
  memcpy(&record_buffer[0], record, getTable()->getShare()->null_bytes);
  ptr= &record_buffer[0] + getTable()->getShare()->null_bytes;

  for (Field **field=getTable()->getFields() ; *field ; field++)
  {
    if (!((*field)->is_null()))
      ptr= (*field)->pack(ptr, record + (*field)->offset(record));
  }

  return((unsigned int) (ptr - &record_buffer[0]));
}


/*
  Look at ha_archive::open() for an explanation of the row format.
  Here we just write out the row.

  Wondering about start_bulk_insert()? We don't implement it for
  archive since it optimizes for lots of writes. The only save
  for implementing start_bulk_insert() is that we could skip
  setting dirty to true each time.
*/
int ha_archive::doInsertRecord(unsigned char *buf)
{
  int rc;
  unsigned char *read_buf= NULL;
  uint64_t temp_auto;
  unsigned char *record=  getTable()->getInsertRecord();

  if (share->crashed)
    return(HA_ERR_CRASHED_ON_USAGE);

  pthread_mutex_lock(&share->mutex());

  if (share->archive_write_open == false)
    if (init_archive_writer())
      return(HA_ERR_CRASHED_ON_USAGE);


  if (getTable()->next_number_field && record == getTable()->getInsertRecord())
  {
    update_auto_increment();
    temp_auto= getTable()->next_number_field->val_int();

    /*
      We don't support decremening auto_increment. They make the performance
      just cry.
    */
    if (temp_auto <= share->archive_write.auto_increment &&
        getTable()->getShare()->getKeyInfo(0).flags & HA_NOSAME)
    {
      rc= HA_ERR_FOUND_DUPP_KEY;
      goto error;
    }
    else
    {
      if (temp_auto > share->archive_write.auto_increment)
        stats.auto_increment_value=
          (share->archive_write.auto_increment= temp_auto) + 1;
    }
  }

  /*
    Notice that the global auto_increment has been increased.
    In case of a failed row write, we will never try to reuse the value.
  */
  share->rows_recorded++;
  rc= real_write_row(buf,  &(share->archive_write));
error:
  pthread_mutex_unlock(&share->mutex());
  if (read_buf)
    free((unsigned char*) read_buf);

  return(rc);
}


void ha_archive::get_auto_increment(uint64_t, uint64_t, uint64_t,
                                    uint64_t *first_value, uint64_t *nb_reserved_values)
{
  *nb_reserved_values= UINT64_MAX;
  *first_value= share->archive_write.auto_increment + 1;
}

/* Initialized at each key walk (called multiple times unlike doStartTableScan()) */
int ha_archive::doStartIndexScan(uint32_t keynr, bool)
{
  active_index= keynr;
  return(0);
}


/*
  No indexes, so if we get a request for an index search since we tell
  the optimizer that we have unique indexes, we scan
*/
int ha_archive::index_read(unsigned char *buf, const unsigned char *key,
                             uint32_t key_len, enum ha_rkey_function)
{
  int rc;
  bool found= 0;
  current_k_offset= getTable()->getShare()->getKeyInfo(0).key_part->offset;
  current_key= key;
  current_key_len= key_len;

  rc= doStartTableScan(true);

  if (rc)
    goto error;

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf + current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  if (found)
    return(0);

error:
  return(rc ? rc : HA_ERR_END_OF_FILE);
}


int ha_archive::index_next(unsigned char * buf)
{
  bool found= 0;

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf+current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  return(found ? 0 : HA_ERR_END_OF_FILE);
}

/*
  All calls that need to scan the table start with this method. If we are told
  that it is a table scan we rewind the file to the beginning, otherwise
  we assume the position will be set.
*/

int ha_archive::doStartTableScan(bool scan)
{
  if (share->crashed)
      return(HA_ERR_CRASHED_ON_USAGE);

  init_archive_reader();

  /* We rewind the file so that we can read from the beginning if scan */
  if (scan)
  {
    if (read_data_header(&archive))
      return(HA_ERR_CRASHED_ON_USAGE);
  }

  return(0);
}


/*
  This is the method that is used to read a row. It assumes that the row is
  positioned where you want it.
*/
int ha_archive::get_row(azio_stream *file_to_read, unsigned char *buf)
{
  int rc;

  if (file_to_read->version == ARCHIVE_VERSION)
    rc= get_row_version3(file_to_read, buf);
  else
    rc= -1;

  return(rc);
}

/* Reallocate buffer if needed */
bool ha_archive::fix_rec_buff(unsigned int length)
{
  record_buffer.resize(length);

  return false;
}

int ha_archive::unpack_row(azio_stream *file_to_read, unsigned char *record)
{
  unsigned int read;
  int error;
  const unsigned char *ptr;

  read= azread_row(file_to_read, &error);
  ptr= (const unsigned char *)file_to_read->row_ptr;

  if (error || read == 0)
  {
    return(-1);
  }

  /* Copy null bits */
  memcpy(record, ptr, getTable()->getNullBytes());
  ptr+= getTable()->getNullBytes();
  for (Field **field= getTable()->getFields() ; *field ; field++)
  {
    if (!((*field)->is_null()))
    {
      ptr= (*field)->unpack(record + (*field)->offset(getTable()->getInsertRecord()), ptr);
    }
  }
  return(0);
}


int ha_archive::get_row_version3(azio_stream *file_to_read, unsigned char *buf)
{
  int returnable= unpack_row(file_to_read, buf);

  return(returnable);
}


/*
  Called during ORDER BY. Its position is either from being called sequentially
  or by having had ha_archive::rnd_pos() called before it is called.
*/

int ha_archive::rnd_next(unsigned char *buf)
{
  int rc;

  if (share->crashed)
      return(HA_ERR_CRASHED_ON_USAGE);

  if (!scan_rows)
    return(HA_ERR_END_OF_FILE);
  scan_rows--;

  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  current_position= aztell(&archive);
  rc= get_row(&archive, buf);

  getTable()->status=rc ? STATUS_NOT_FOUND: 0;

  return(rc);
}


/*
  Thanks to the table bool is_ordered this will be called after
  each call to ha_archive::rnd_next() if an ordering of the rows is
  needed.
*/

void ha_archive::position(const unsigned char *)
{
  internal::my_store_ptr(ref, ref_length, current_position);
  return;
}


/*
  This is called after a table scan for each row if the results of the
  scan need to be ordered. It will take *pos and use it to move the
  cursor in the file so that the next row that is called is the
  correctly ordered row.
*/

int ha_archive::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  current_position= (internal::my_off_t)internal::my_get_ptr(pos, ref_length);
  if (azseek(&archive, (size_t)current_position, SEEK_SET) == (size_t)(-1L))
    return(HA_ERR_CRASHED_ON_USAGE);
  return(get_row(&archive, buf));
}

/*
  This method repairs the meta file. It does this by walking the datafile and
  rewriting the meta file. Currently it does this by calling optimize with
  the extended flag.
*/
int ha_archive::repair()
{
  int rc= optimize();

  if (rc)
    return(HA_ERR_CRASHED_ON_REPAIR);

  share->crashed= false;
  return(0);
}

/*
  The table can become fragmented if data was inserted, read, and then
  inserted again. What we do is open up the file and recompress it completely.
*/
int ha_archive::optimize()
{
  int rc= 0;
  azio_stream writer;

  init_archive_reader();

  // now we close both our writer and our reader for the rename
  if (share->archive_write_open)
  {
    azclose(&(share->archive_write));
    share->archive_write_open= false;
  }

  char* proto_string;
  proto_string= (char*)malloc(sizeof(char) * archive.frm_length);
  if (proto_string == NULL)
  {
    return ENOMEM;
  }
  azread_frm(&archive, proto_string);

  /* Lets create a file to contain the new data */
  std::string writer_filename= share->table_name;
  writer_filename.append(ARN);

  if (!(azopen(&writer, writer_filename.c_str(), O_CREAT|O_RDWR, AZ_METHOD_BLOCK)))
  {
    free(proto_string);
    return(HA_ERR_CRASHED_ON_USAGE);
  }

  azwrite_frm(&writer, proto_string, archive.frm_length);

  /*
    An extended rebuild is a lot more effort. We open up each row and re-record it.
    Any dead rows are removed (aka rows that may have been partially recorded).

    As of Archive format 3, this is the only type that is performed, before this
    version it was just done on T_EXTEND
  */
  if (1)
  {
    /*
      Now we will rewind the archive file so that we are positioned at the
      start of the file.
    */
    azflush(&archive, Z_SYNC_FLUSH);
    rc= read_data_header(&archive);

    /*
      On success of writing out the new header, we now fetch each row and
      insert it into the new archive file.
    */
    if (!rc)
    {
      uint64_t rows_restored;
      share->rows_recorded= 0;
      stats.auto_increment_value= 1;
      share->archive_write.auto_increment= 0;

      rows_restored= archive.rows;

      for (uint64_t x= 0; x < rows_restored ; x++)
      {
        rc= get_row(&archive, getTable()->getInsertRecord());

        if (rc != 0)
          break;

        real_write_row(getTable()->getInsertRecord(), &writer);
        /*
          Long term it should be possible to optimize this so that
          it is not called on each row.
        */
        if (getTable()->found_next_number_field)
        {
          Field *field= getTable()->found_next_number_field;

          /* Since we will need to use field to translate, we need to flip its read bit */
          field->setReadSet();

          uint64_t auto_value=
            (uint64_t) field->val_int(getTable()->getInsertRecord() +
                                       field->offset(getTable()->getInsertRecord()));
          if (share->archive_write.auto_increment < auto_value)
            stats.auto_increment_value=
              (share->archive_write.auto_increment= auto_value) + 1;
        }
      }
      share->rows_recorded= (ha_rows)writer.rows;
    }

    if (rc && rc != HA_ERR_END_OF_FILE)
    {
      goto error;
    }
  }

  azclose(&writer);
  share->dirty= false;

  azclose(&archive);

  // make the file we just wrote be our data file
  rc = internal::my_rename(writer_filename.c_str(), share->data_file_name.c_str(), MYF(0));

  free(proto_string);
  return(rc);
error:
  free(proto_string);
  azclose(&writer);

  return(rc);
}

/*
  Below is an example of how to setup row level locking.
*/
THR_LOCK_DATA **ha_archive::store_lock(Session *session,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  delayed_insert= false;

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK Table or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE)
        && !session_tablespace_op(session))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT)
      lock_type = TL_READ;

    lock.type=lock_type;
  }

  *to++= &lock;

  return to;
}

/*
  Hints for optimizer, see ha_tina for more information
*/
int ha_archive::info(uint32_t flag)
{
  /*
    If dirty, we lock, and then reset/flush the data.
    I found that just calling azflush() doesn't always work.
  */
  pthread_mutex_lock(&share->mutex());
  if (share->dirty == true)
  {
    azflush(&(share->archive_write), Z_SYNC_FLUSH);
    share->rows_recorded= share->archive_write.rows;
    share->dirty= false;
    if (share->version < global_version)
    {
      share->version_rows= share->rows_recorded;
      share->version= global_version;
    }

  }

  /*
    This should be an accurate number now, though bulk and delayed inserts can
    cause the number to be inaccurate.
  */
  stats.records= share->rows_recorded;
  pthread_mutex_unlock(&share->mutex());

  scan_rows= stats.records;
  stats.deleted= 0;

  /* Costs quite a bit more to get all information */
  if (flag & HA_STATUS_TIME)
  {
    struct stat file_stat;  // Stat information for the data file

    stat(share->data_file_name.c_str(), &file_stat);

    stats.mean_rec_length= getTable()->getRecordLength()+ buffer.alloced_length();
    stats.data_file_length= file_stat.st_size;
    stats.create_time= file_stat.st_ctime;
    stats.update_time= file_stat.st_mtime;
    stats.max_data_file_length= share->rows_recorded * stats.mean_rec_length;
  }
  stats.delete_length= 0;
  stats.index_file_length=0;

  if (flag & HA_STATUS_AUTO)
  {
    init_archive_reader();
    pthread_mutex_lock(&share->mutex());
    azflush(&archive, Z_SYNC_FLUSH);
    pthread_mutex_unlock(&share->mutex());
    stats.auto_increment_value= archive.auto_increment + 1;
  }

  return(0);
}


/*
  This method tells us that a bulk insert operation is about to occur. We set
  a flag which will keep doInsertRecord from saying that its data is dirty. This in
  turn will keep selects from causing a sync to occur.
  Basically, yet another optimizations to keep compression working well.
*/
void ha_archive::start_bulk_insert(ha_rows rows)
{
  if (!rows || rows >= ARCHIVE_MIN_ROWS_TO_USE_BULK_INSERT)
    bulk_insert= true;
  return;
}


/*
  Other side of start_bulk_insert, is end_bulk_insert. Here we turn off the bulk insert
  flag, and set the share dirty so that the next select will call sync for us.
*/
int ha_archive::end_bulk_insert()
{
  bulk_insert= false;
  share->dirty= true;
  return(0);
}

/*
  We cancel a truncate command. The only way to delete an archive table is to drop it.
  This is done for security reasons. In a later version we will enable this by
  allowing the user to select a different row format.
*/
int ha_archive::delete_all_rows()
{
  return(HA_ERR_WRONG_COMMAND);
}

/*
  Simple scan of the tables to make sure everything is ok.
*/

int ha_archive::check(Session* session)
{
  int rc= 0;
  const char *old_proc_info;

  old_proc_info= get_session_proc_info(session);
  set_session_proc_info(session, "Checking table");
  /* Flush any waiting data */
  pthread_mutex_lock(&share->mutex());
  azflush(&(share->archive_write), Z_SYNC_FLUSH);
  pthread_mutex_unlock(&share->mutex());

  /*
    Now we will rewind the archive file so that we are positioned at the
    start of the file.
  */
  init_archive_reader();
  azflush(&archive, Z_SYNC_FLUSH);
  read_data_header(&archive);
  for (uint64_t x= 0; x < share->archive_write.rows; x++)
  {
    rc= get_row(&archive, getTable()->getInsertRecord());

    if (rc != 0)
      break;
  }

  set_session_proc_info(session, old_proc_info);

  if ((rc && rc != HA_ERR_END_OF_FILE))
  {
    share->crashed= false;
    return(HA_ADMIN_CORRUPT);
  }
  else
  {
    return(HA_ADMIN_OK);
  }
}

int ArchiveEngine::doRenameTable(Session&, const TableIdentifier &from, const TableIdentifier &to)
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

  return error;
}

bool ArchiveEngine::doDoesTableExist(Session&,
                                     const TableIdentifier &identifier)
{
  string proto_path(identifier.getPath());
  proto_path.append(ARZ);

  if (access(proto_path.c_str(), F_OK))
  {
    return false;
  }

  return true;
}

void ArchiveEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                                          const drizzled::SchemaIdentifier &schema_identifier,
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

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, ARZ) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0))
    {  }
    else
    {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len= TableIdentifier::filename_to_tablename(filename->c_str(), uname, sizeof(uname));
      // TODO: Remove need for memory copy here
      uname[file_name_len - sizeof(ARZ) + 1]= '\0'; // Subtract ending, place NULL 

      set_of_identifiers.push_back(TableIdentifier(schema_identifier, uname));
    }
  }
}
