/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/*
  This class is shared between different table objects. There is one
  instance of table share per one table in the database.
*/

#pragma once

#include <string>

#include <boost/unordered_map.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <drizzled/memory/root.h>
#include <drizzled/message.h>
#include <drizzled/util/string.h>
#include <drizzled/lex_string.h>
#include <drizzled/key_map.h>
#include <drizzled/field.h>

namespace drizzled {

const static std::string NO_PROTOBUFFER_AVAILABLE("NO PROTOBUFFER AVAILABLE");

class TableShare
{
  typedef std::vector<std::string> StringVector;

public:
  typedef boost::shared_ptr<TableShare> shared_ptr;
  typedef std::vector <shared_ptr> vector;

  TableShare(const identifier::Table::Type type_arg);

  TableShare(const identifier::Table &identifier, const identifier::Table::Key &key); // Used by placeholder

  TableShare(const identifier::Table &identifier); // Just used during createTable()

  TableShare(const identifier::Table::Type type_arg,
             const identifier::Table &identifier,
             char *path_arg= NULL, uint32_t path_length_arg= 0); // Shares for cache

  virtual ~TableShare();

private:
  /** Category of this table. */
  enum_table_category table_category;

public:
  bool isTemporaryCategory() const
  {
    return (table_category == TABLE_CATEGORY_TEMPORARY);
  }

  void setTableCategory(enum_table_category arg)
  {
    table_category= arg;
  }

  /* The following is copied to each Table on OPEN */
  typedef std::vector<Field *> Fields;

private:
  Fields _fields;

public:
  const Fields getFields() const
  {
    return _fields;
  }

  Fields getFields()
  {
    return _fields;
  }

  Field ** getFields(bool)
  {
    return &_fields[0];
  }

  void setFields(uint32_t arg)
  {
    _fields.resize(arg);
  }

  uint32_t positionFields(Field **arg) const
  {
    return (arg - (Field **)&_fields[0]);
  }

  void pushField(Field *arg)
  {
    _field_size++;
    _fields.push_back(arg);
  }

  Field **found_next_number_field;

private:
  Field *timestamp_field;               /* Used only during open */

public:

  Field *getTimestampField() const               /* Used only during open */
  {
    return timestamp_field;
  }

  void setTimestampField(Field *arg) /* Used only during open */
  {
    timestamp_field= arg;
  }


private:
  KeyInfo  *key_info;			/* data of keys in database */

public:
  KeyInfo &getKeyInfo(uint32_t arg) const
  {
    return key_info[arg];
  }
  std::vector<uint>	blob_field;			/* Index to blobs in Field arrray*/

private:
  /* hash of field names (contains pointers to elements of field array) */
  typedef boost::unordered_map < std::string, Field **, util::insensitive_hash, util::insensitive_equal_to> FieldMap;
  typedef std::pair< std::string, Field ** > FieldMapPair;
  FieldMap name_hash; /* hash of field names */

public:
  size_t getNamedFieldSize() const
  {
    return name_hash.size();
  }

  Field **getNamedField(const std::string &arg)
  {
    FieldMap::iterator iter= name_hash.find(arg);

    if (iter == name_hash.end())
        return 0;

    return iter->second;
  }

private:
  memory::Root mem_root;

  unsigned char* alloc(size_t arg)
  {
    return mem_root.alloc(arg);
  }

  char *strmake(const char *str_arg, size_t len_arg)
  {
    return mem_root.strmake(str_arg, len_arg);
  }

  memory::Root& mem()
  {
    return mem_root;
  }

  std::vector<std::string> _keynames;

  void addKeyName(std::string arg)
  {
    std::transform(arg.begin(), arg.end(),
                   arg.begin(), ::toupper);
    _keynames.push_back(arg);
  }

public:
  bool doesKeyNameExist(const char *name_arg, uint32_t name_length, uint32_t &position) const
  {
    return doesKeyNameExist(std::string(name_arg, name_length), position);
  }

  bool doesKeyNameExist(std::string arg, uint32_t &position) const
  {
    std::transform(arg.begin(), arg.end(),
                   arg.begin(), ::toupper);

    std::vector<std::string>::const_iterator iter= std::find(_keynames.begin(), _keynames.end(), arg);

    if (iter == _keynames.end())
    {
      position= UINT32_MAX; //historical, required for finding primary key from unique
      return false;
    }

    position= iter -  _keynames.begin();

    return true;
  }

private:
  std::vector<TYPELIB> intervals;			/* pointer to interval info */

public:
  virtual void lock()
  { }

  virtual void unlock()
  { }

private:
  std::vector<unsigned char> default_values;		/* row with default values */

public:
  // @note This needs to be made to be const in the future
  unsigned char *getDefaultValues()
  {
    return &default_values[0];
  }
  void resizeDefaultValues(size_t arg)
  {
    default_values.resize(arg);
  }

  const charset_info_st *table_charset; /* Default charset of string fields */

  boost::dynamic_bitset<> all_set;

  /*
    Key which is used for looking-up table in table cache and in the list
    of thread's temporary tables. Has the form of:
    "database_name\0table_name\0" + optional part for temporary tables.

    Note that all three 'table_cache_key', 'db' and 'table_name' members
    must be set (and be non-zero) for tables in table cache. They also
    should correspond to each other.
    To ensure this one can use set_table_cache() methods.
  */
private:
  identifier::Table::Key private_key_for_cache; // This will not exist in the final design.
  std::vector<char> private_normalized_path; // This will not exist in the final design.
  LEX_STRING db;                        /* Pointer to db */
  LEX_STRING table_name;                /* Table name (for open) */
  LEX_STRING path;	/* Path to table (from datadir) */
  LEX_STRING normalized_path;		/* unpack_filename(path) */

public:

  const char *getNormalizedPath() const
  {
    return normalized_path.str;
  }

  const char *getPath() const
  {
    return path.str;
  }

  const identifier::Table::Key& getCacheKey() const // This should never be called when we aren't looking at a cache.
  {
    assert(private_key_for_cache.size());
    return private_key_for_cache;
  }

  size_t getCacheKeySize() const
  {
    return private_key_for_cache.size();
  }

private:
  void setPath(char *str_arg, uint32_t size_arg)
  {
    path.str= str_arg;
    path.length= size_arg;
  }

  void setNormalizedPath(char *str_arg, uint32_t size_arg)
  {
    normalized_path.str= str_arg;
    normalized_path.length= size_arg;
  }

public:

  const char *getTableName() const
  {
    return table_name.str;
  }

  uint32_t getTableNameSize() const
  {
    return table_name.length;
  }

  const std::string &getTableName(std::string &name_arg) const
  {
    name_arg.clear();
    name_arg.append(table_name.str, table_name.length);

    return name_arg;
  }

  const char *getSchemaName() const
  {
    return db.str;
  }

  const std::string &getSchemaName(std::string &schema_name_arg) const
  {
    schema_name_arg.clear();
    schema_name_arg.append(db.str, db.length);

    return schema_name_arg;
  }

  uint32_t   block_size;                   /* create information */

private:
  uint64_t   version;

public:
  uint64_t getVersion() const
  {
    return version;
  }

  void refreshVersion();

  void resetVersion()
  {
    version= 0;
  }

private:
  uint32_t   timestamp_offset;		/* Set to offset+1 of record */

  uint32_t reclength;			/* Recordlength */
  uint32_t stored_rec_length;         /* Stored record length*/

public:
  uint32_t sizeStoredRecord() const
  {
    return stored_rec_length;
  }

  uint32_t getRecordLength() const
  {
    return reclength;
  }

  void setRecordLength(uint32_t arg)
  {
    reclength= arg;
  }

  const Field_blob *getBlobFieldAt(uint32_t arg) const
  {
    if (arg < blob_fields)
      return (Field_blob*) _fields[blob_field[arg]];

    return NULL;
  }

private:
  /* Max rows is a hint to HEAP during a create tmp table */
  uint64_t max_rows;

  boost::scoped_ptr<message::Table> _table_message;

public:
  /*
    @note Without a _table_message, we assume we are building a STANDARD table.
    This will be modified once we use Identifiers in the Share itself.
  */
  message::Table::TableType getTableType() const
  {
    return getTableMessage() ? getTableMessage()->type() : message::Table::STANDARD;
  }

  const std::string &getTableTypeAsString() const
  {
    if (getTableMessage())
      return message::type(getTableMessage()->type());

    return NO_PROTOBUFFER_AVAILABLE;
  }

  /* This is only used in one location currently */
  inline message::Table *getTableMessage() const
  {
    return _table_message.get();
  }

  void setTableMessage(const message::Table &arg)
  {
    assert(not getTableMessage());
    _table_message.reset(new message::Table(arg));
  }

  const message::Table::Field &field(int32_t field_position) const
  {
    assert(getTableMessage());
    return getTableMessage()->field(field_position);
  }

  inline bool hasComment() const
  {
    return getTableMessage() ?  getTableMessage()->options().has_comment() : false; 
  }

  inline const char *getComment()
  {
    return (getTableMessage() && getTableMessage()->has_options()) ?  getTableMessage()->options().comment().c_str() : NULL; 
  }

  inline uint32_t getCommentLength() const
  {
    return (getTableMessage()) ? getTableMessage()->options().comment().length() : 0; 
  }

  inline uint64_t getMaxRows() const
  {
    return max_rows;
  }

  inline void setMaxRows(uint64_t arg)
  {
    max_rows= arg;
  }

  /**
   * Returns true if the supplied Field object
   * is part of the table's primary key.
 */
  bool fieldInPrimaryKey(Field *field) const;

  plugin::StorageEngine *storage_engine;			/* storage engine plugin */
  inline plugin::StorageEngine *db_type() const	/* table_type for handler */
  {
    return storage_engine;
  }
  inline plugin::StorageEngine *getEngine() const	/* table_type for handler */
  {
    return storage_engine;
  }

private:
  identifier::Table::Type tmp_table;
public:

  identifier::Table::Type getType() const
  {
    return tmp_table;
  }

private:
  uint32_t _ref_count;       /* How many Table objects uses this */

public:
  uint32_t getTableCount() const
  {
    return _ref_count;
  }

  void incrementTableCount()
  {
    lock();
    _ref_count++;
    unlock();
  }

  uint32_t decrementTableCount()
  {
    return --_ref_count;
  }

  uint32_t null_bytes;
  uint32_t last_null_bit_pos;
private:
  uint32_t _field_size;				/* Number of fields */

public:
  void setFieldSize(uint32_t arg)
  {
    _field_size= arg;
  }

  uint32_t sizeFields() const
  {
    return _field_size;
  }

  uint32_t rec_buff_length;                 /* Size of table->record[] buffer */
  uint32_t keys;

  uint32_t sizeKeys() const
  {
    return keys;
  }
  uint32_t key_parts;
  uint32_t max_key_length, max_unique_length, total_key_length;
  uint32_t uniques;                         /* Number of UNIQUE index */
  uint32_t null_fields;			/* number of null fields */
  uint32_t blob_fields;			/* number of blob fields */
private:
  bool has_variable_width;                  /* number of varchar fields */

public:
  bool hasVariableWidth() const
  {
    return has_variable_width; // We should calculate this.
  }
  void setVariableWidth()
  {
    has_variable_width= true;
  }
  uint32_t db_create_options;		/* Create options from database */
  uint32_t db_options_in_use;		/* Options in use */
  uint32_t db_record_offset;		/* if HA_REC_IN_SEQ */
  uint32_t rowid_field_offset;		/* Field_nr +1 to rowid field */
  /**
   * @TODO 
   *
   * Currently the replication services component uses
   * the primary_key member to determine which field is the table's
   * primary key.  However, as it exists, because this member is scalar, it
   * only supports a single-column primary key. Is there a better way
   * to ask for the fields which are in a primary key?
 */
private:
  uint32_t primary_key;
public:

  uint32_t getPrimaryKey() const
  {
    return primary_key;
  }

  bool hasPrimaryKey() const
  {
    return primary_key != MAX_KEY;
  }

  /* Index of auto-updated TIMESTAMP field in field array */
  uint32_t next_number_index;               /* autoincrement key number */
  uint32_t next_number_key_offset;          /* autoinc keypart offset in a key */
  uint32_t next_number_keypart;             /* autoinc keypart number in a key */
  uint32_t error, open_errno, errarg;       /* error from open_table_def() */

private:
  uint8_t blob_ptr_size;			/* 4 or 8 */

public:
  uint8_t sizeBlobPtr() const
  {
    return blob_ptr_size;
  }

  bool db_low_byte_first;		/* Portable row format */

  /*
    Set of keys in use, implemented as a Bitmap.
    Excludes keys disabled by ALTER Table ... DISABLE KEYS.
  */
  key_map keys_in_use;
  key_map keys_for_keyread;

  /* 
    event_observers is a class containing all the event plugins that have 
    registered an interest in this table.
  */
  virtual plugin::EventObserverList *getTableObservers() 
  { 
    return NULL;
  }
  
  virtual void setTableObservers(plugin::EventObserverList *) 
  { }
  
  /*
    Set share's identifier information.

    SYNOPSIS
    setIdentifier()

    NOTES
  */

  void setIdentifier(const identifier::Table &identifier_arg);

  /*
    Initialize share for temporary tables

    SYNOPSIS
    init()
    share	Share to fill
    key		Table_cache_key, as generated from create_table_def_key.
    must start with db name.
    key_length	Length of key
    table_name	Table name
    path	Path to table (possible in lower case)

    NOTES
    
  */

private:
  void init(const char *new_table_name,
            const char *new_path);

protected:
  void open_table_error(int pass_error, int db_errno, int pass_errarg);

public:

  static TableShare::shared_ptr getShareCreate(Session *session, 
                                               const identifier::Table &identifier,
                                               int &error);

  friend std::ostream& operator<<(std::ostream& output, const TableShare &share)
  {
    output << "TableShare:(";
    output <<  share.getSchemaName();
    output << ", ";
    output << share.getTableName();
    output << ", ";
    output << share.getTableTypeAsString();
    output << ", ";
    output << share.getPath();
    output << ")";

    return output;  // for multiple << operators.
  }

protected:
  friend class drizzled::table::Singular;

  Field *make_field(const message::Table::Field &pfield,
                    unsigned char *ptr,
                    uint32_t field_length,
                    bool is_nullable,
                    unsigned char *null_pos,
                    unsigned char null_bit,
                    uint8_t decimals,
                    enum_field_types field_type,
                    const charset_info_st * field_charset,
                    Field::utype unireg_check,
                    TYPELIB *interval,
                    const char *field_name);

  Field *make_field(const message::Table::Field &pfield,
                    unsigned char *ptr,
                    uint32_t field_length,
                    bool is_nullable,
                    unsigned char *null_pos,
                    unsigned char null_bit,
                    uint8_t decimals,
                    enum_field_types field_type,
                    const charset_info_st * field_charset,
                    Field::utype unireg_check,
                    TYPELIB *interval,
                    const char *field_name, 
                    bool is_unsigned);

public:
  int open_table_def(Session& session, const identifier::Table &identifier);

  int open_table_from_share(Session *session,
                            const identifier::Table &identifier,
                            const char *alias,
                            uint32_t db_stat, uint32_t ha_open_flags,
                            Table &outparam);
private:
  int open_table_from_share_inner(Session *session,
                                  const char *alias,
                                  uint32_t db_stat,
                                  Table &outparam);
  int open_table_cursor_inner(const identifier::Table &identifier,
                              uint32_t db_stat, uint32_t ha_open_flags,
                              Table &outparam,
                              bool &error_reported);
public:
  bool parse_table_proto(Session& session, const message::Table &table);

  virtual bool is_replicated() const
  {
    return false;
  }
};

} /* namespace drizzled */

