/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_INFO_SCHEMA_TABLE_H
#define DRIZZLED_PLUGIN_INFO_SCHEMA_TABLE_H

#include "drizzled/plugin/plugin.h"
#include "drizzled/algorithm/crc32.h"
#include "drizzled/common.h"

#include <cstring>
#include <string>
#include <set>
#include <algorithm>

class Session;
class TableList;
class Table;

typedef struct drizzle_lex_string LEX_STRING;

extern const std::string INFORMATION_SCHEMA_NAME;

namespace drizzled
{
namespace plugin
{

/**
 * @file
 *   info_schema.h
 * @brief 
 *   Header file which contains all classes related to I_S
 */

typedef class Item COND;
class InfoSchemaTable;


/**
 * @class ColumnInfo
 * @brief
 *   Represents a field (column) in an I_S table.
 */
class ColumnInfo 
{ 
public: 
  ColumnInfo(const std::string& in_name, 
             uint32_t in_length, 
             enum enum_field_types in_type,
             int32_t in_value,
             uint32_t in_flags,
             const std::string& in_old_name)
    :
      name(in_name),
      length(in_length),
      type(in_type),
      value(in_value),
      flags(in_flags),
      old_name(in_old_name)
  {}

  ColumnInfo()
    :
      name(),
      length(0),
      type(DRIZZLE_TYPE_VARCHAR),
      flags(0),
      old_name()
  {}

  /**
   * @return the name of this column.
   */
  const std::string &getName() const
  {
    return name;
  }

  /**
   * This method is only ever called from the
   * InfoSchemaMethods::oldFormat() methods. It is mostly
   * for old SHOW compatability. It is used when a list
   * of fields need to be generated for SHOW. The names
   * for those fields (or columns) are found by calling
   * this method on each column in the I_S table.
   *
   * @return the old name of this column.
   */
  const std::string &getOldName() const
  {
    return old_name;
  }

  /**
   * @return the flags for this column.
   */
  uint32_t getFlags() const
  {
    return flags;
  }

  /**
   * @return the length of this column.
   */
  uint32_t getLength() const
  {
    return length;
  }

  /**
   * @return the value of this column.
   */
  int32_t getValue() const
  {
    return value;
  }

  /**
   * @return this column's type.
   */
  enum enum_field_types getType() const
  {
    return type;
  }

private:

  /**
   * This is used as column name.
   */
  const std::string name;

  /**
   * For string-type columns, this is the maximum number of
   * characters. Otherwise, it is the 'display-length' for the column.
   */
  uint32_t length;

  /**
   * This denotes data type for the column. For the most part, there seems to
   * be one entry in the enum for each SQL data type, although there seem to
   * be a number of additional entries in the enum.
   */
  enum enum_field_types type;

  int32_t value;

  /**
   * This is used to set column attributes. By default, columns are @c NOT
   * @c NULL and @c SIGNED, and you can deviate from the default
   * by setting the appopriate flags. You can use either one of the flags
   * @c MY_I_S_MAYBE_NULL and @cMY_I_S_UNSIGNED or
   * combine them using the bitwise or operator @c |. Both flags are
   * defined in table.h.
   */
  uint32_t flags;

  /**
   * The name of this column which is used for old SHOW
   * compatability.
   */
  const std::string old_name;

};

/**
 * @class InfoSchemaMethods
 * @brief
 *   The methods that an I_S table can support
 */
class InfoSchemaMethods
{
public:
  virtual ~InfoSchemaMethods() {}

  virtual int fillTable(Session *session, 
                        Table *table,
                        InfoSchemaTable *schema_table);
  virtual int processTable(InfoSchemaTable *store_table, 
                           Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name);
  virtual int oldFormat(Session *session, 
                        InfoSchemaTable *schema_table) const;
};

/**
 * @class InfoSchemaRecord
 * @brief represents a row in an I_S table
 */
class InfoSchemaRecord
{
public:
  InfoSchemaRecord();
  InfoSchemaRecord(unsigned char *buf, size_t in_len);
  InfoSchemaRecord(const InfoSchemaRecord &rhs);

  ~InfoSchemaRecord()
  {
    if (record)
    {
      delete [] record;
    }
  }

  /**
   * Copy this record into the memory passed to this function.
   *
   * @param[out] buf copy the record into this memory
   */
  void copyRecordInto(unsigned char *buf)
  {
    memcpy(buf, record, rec_len);
  }

  /**
   * @return the length of this record
   */
  size_t getRecordLength() const
  {
    return rec_len;
  }

  /**
   * @return the checksum of the data in this record
   */
  uint32_t getChecksum() const
  {
    return checksum;
  }

  /**
   * @param[in] crc a checksum to compare
   * @return true if the input checksum matches the checksum for this record; false otherwise
   */
  bool checksumMatches(uint32_t crc) const
  {
    return (checksum == crc);
  }

private:

  unsigned char *record;

  size_t rec_len;

  uint32_t checksum;

};

class DeleteRows
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

class FindRowByChecksum
{
  uint32_t cs;
public:
  FindRowByChecksum(uint32_t in_cs)
    :
      cs(in_cs)
  {}

  inline bool operator()(const InfoSchemaRecord *rec) const
  {
    return (cs == rec->getChecksum());
  }
};

/**
 * @class InfoSchemaTable
 * @brief 
 *   Represents an I_S table.
 */
class InfoSchemaTable : public Plugin
{
  InfoSchemaTable();
  InfoSchemaTable(const InfoSchemaTable &);
  InfoSchemaTable& operator=(const InfoSchemaTable &);
public:

  typedef std::vector<const ColumnInfo *> Columns;
  typedef std::vector<InfoSchemaRecord *> Rows;
  
  InfoSchemaTable(const std::string& tab_name,
                  Columns& in_column_info,
                  int idx_col1,
                  int idx_col2,
                  bool in_hidden,
                  bool in_opt_possible,
                  uint32_t req_object,
                  InfoSchemaMethods *in_methods)
    :
      Plugin(tab_name, "InfoSchemaTable"),
      hidden(in_hidden),
      is_opt_possible(in_opt_possible),
      first_column_index(idx_col1),
      second_column_index(idx_col2),
      requested_object(req_object),
      column_info(in_column_info),
      rows(),
      i_s_methods(in_methods)
  {}

  explicit InfoSchemaTable(const std::string& tab_name)
    :
      Plugin(tab_name, "InfoSchemaTable"),
      hidden(false),
      is_opt_possible(false),
      first_column_index(0),
      second_column_index(0),
      requested_object(0),
      column_info(),
      rows(),
      i_s_methods(NULL)
  {}

  virtual ~InfoSchemaTable()
  {
    std::for_each(rows.begin(),
                  rows.end(),
                  DeleteRows());
    rows.clear();
  }

  /**
   * Set the methods available on this I_S table.
   * @param[in] new_methods the methods to use
   */
  void setInfoSchemaMethods(InfoSchemaMethods *new_methods)
  {
    i_s_methods= new_methods;
  }

  /**
   * Fill I_S table.
   *
   * @param[in] session a session handler
   * @return 0 on success; 1 on error
   */
  int fillTable(Session *session, Table *table)
  {
    int retval= i_s_methods->fillTable(session, table, this);
    return retval;
  }

  /**
   * Fill and store records into an I_S table.
   *
   * @param[in] session a session handler
   * @param[in] tables table list (processed table)
   * @param[in] table I_S table
   * @param[in] res 1 means error during opening of the processed table
   *                0 means processed table opened without error
   * @param[in] db_name database name
   * @param[in] tab_name table name
   * @return 0 on success; 1 on error
   */
  int processTable(Session *session, TableList *tables, Table *table,
                   bool res, LEX_STRING *db_name, LEX_STRING *tab_name)
  {
    int retval= i_s_methods->processTable(this, session, tables, table,
                                          res, db_name, tab_name);
    return retval;
  }

  /**
   * For old SHOW compatibility. It is used when old SHOW doesn't
   * have generated column names. Generates the list of fields
   * for SHOW.
   *
   * @param[in] session a session handler
   * @param[in] schema_table pointer to element of the I_S tables list
   */
  int oldFormat(Session *session, InfoSchemaTable *schema_table) const
  {
    int retval= i_s_methods->oldFormat(session, schema_table);
    return retval;
  }

  /**
   * @param[in] new_first_index value to set first column index to
   */
  void setFirstColumnIndex(int32_t new_first_index)
  {
    first_column_index= new_first_index;
  }

  /**
   * @param[in] new_second_index value to set second column index to
   */
  void setSecondColumnIndex(int32_t new_second_index)
  {
    second_column_index= new_second_index;
  }

  /**
   * @param[in] in_column_info the columns info to use for this I_S table
   */
  void setColumnInfo(ColumnInfo *in_column_info)
  {
    ColumnInfo *tmp= in_column_info;
    for (; tmp->getName().length() != 0; tmp++)
    {
      column_info.push_back(tmp);
    }
  }

  /**
   * @return the name of the I_S table.
   */
  const std::string &getTableName() const
  {
    return getName();
  }

  /**
   * @return the names of the I_S tables.
   */
  static void getTableNames(std::set<std::string>& tables_names);

  /**
   * @return true if this I_S table is hidden; false otherwise.
   */
  bool isHidden() const
  {
    return hidden;
  }

  /**
   * @return true if I_S optimizations can be performed on this
   *         I_S table when running the fillTable method; false
   *         otherwise.
   */
  bool isOptimizationPossible() const
  {
    return is_opt_possible;
  }

  /**
   * @return the index for the first field.
   */
  int32_t getFirstColumnIndex() const
  {
    return first_column_index;
  }

  /**
   * @return the index the second field.
   */
  int32_t getSecondColumnIndex() const
  {
    return second_column_index;
  }

  /**
   * @return the requested object.
   */
  uint32_t getRequestedObject() const
  {
    return requested_object;
  }

  /**
   * @return the columns for this I_S table
   */
  const Columns &getColumns() const
  {
    return column_info;
  }

  /**
   * @return the rows for this I_S table.
   */
  Rows &getRows()
  {
    return rows;
  }

  /**
   * Clear the rows for this table and de-allocate all memory for this rows.
   */
  void clearRows()
  {
    std::for_each(rows.begin(),
                  rows.end(),
                  DeleteRows());
    rows.clear();
  }

  /**
   * Add a row to the std::vector of rows for this I_S table. For the moment, we check to make sure
   * that we do not add any duplicate rows. This is done in a niave manner for the moment by
   * checking the crc of the raw data for each row. We will not add this row to the std::vector of
   * rows for this I_S table is a duplicate row already exists in the std::vector.
   *
   * @param[in] buf raw data for the record to add
   * @param[in] len size of the raw data for the record to add
   */
  void addRow(unsigned char *buf, size_t len)
  {
    uint32_t cs= drizzled::algorithm::crc32((const char *) buf, len);
    Rows::iterator it= std::find_if(rows.begin(),
                                    rows.end(),
                                    FindRowByChecksum(cs));
    if (it == rows.end())
    {
      InfoSchemaRecord *record= new InfoSchemaRecord(buf, len);
      rows.push_back(record);
    }
  }

  /**
   * @param[in] index the index of this column
   * @return the name for the column at the given index
   */
  const std::string &getColumnName(int index) const
  {
    return column_info[index]->getName();
  }

private:

  /**
   * Boolean which indicates whether this I_S table
   * is hidden or not. If it is hidden, it will not show
   * up in the list of I_S tables.
   */
  bool hidden;

  /**
   * Boolean which indicates whether optimizations are
   * possible on this I_S table when performing the
   * fillTable method.
   */
  bool is_opt_possible;

  /**
   * The index of the first column.
   */
  int32_t first_column_index;

  /**
   * The index of the second column.
   */
  int32_t second_column_index;

  /**
   * The object to open (TABLE | VIEW).
   */
  uint32_t requested_object;

  /**
   * The columns for this I_S table.
   */
  Columns column_info;

  /**
   * The rows for this I_S table.
   */
  Rows rows;

  /**
   * Contains the methods available on this I_S table.
   */
  InfoSchemaMethods *i_s_methods;

public:
  static bool addPlugin(plugin::InfoSchemaTable *schema_table);
  static void removePlugin(plugin::InfoSchemaTable *table);

  static plugin::InfoSchemaTable *getTable(const char *table_name);
  static int addTableToList(Session *session, std::vector<LEX_STRING*> &files,
                            const char *wild);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_INFO_SCHEMA_TABLE_H */
