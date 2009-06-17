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

#ifndef DRIZZLED_INFO_SCHEMA_H
#define DRIZZLED_INFO_SCHEMA_H

/**
 * @file
 *   info_schema.h
 * @brief 
 *   Header file which contains all classes related to I_S
 */

typedef class Item COND;

/**
 * @class 
 *   InfoSchemaMethods
 * @brief
 *   The methods that an I_S table can support
 */
class InfoSchemaMethods
{
public:
  virtual ~InfoSchemaMethods() {}

  virtual Table *createSchemaTable(Session *session,
                                   TableList *table_list) const;
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
  virtual int oldFormat(Session *session, 
                        InfoSchemaTable *schema_table) const;
};

class CharSetISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

class CollationISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class CollCharISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class ColumnsISMethods : public InfoSchemaMethods
{
public:
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

class StatusISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class VariablesISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class KeyColUsageISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class OpenTablesISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class PluginsISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class ProcessListISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
};

class RefConstraintsISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class SchemataISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

class StatsISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class TablesISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class TabConstraintsISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class TabNamesISMethods : public InfoSchemaMethods
{
public:
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

/**
 * @class 
 *   InfoSchemaTable
 * @brief 
 *   Represents an I_S table.
 */
class InfoSchemaTable
{
public:
  InfoSchemaTable(const char *tabName,
                  ST_FIELD_INFO *inFieldsInfo,
                  int idxField1,
                  int idxField2,
                  bool inHidden,
                  bool inOptPossible,
                  uint32_t reqObject,
                  InfoSchemaMethods *inMethods)
    :
      table_name(tabName),
      hidden(inHidden),
      is_opt_possible(inOptPossible),
      first_field_index(idxField1),
      second_field_index(idxField2),
      requested_object(reqObject),
      fields_info(inFieldsInfo),
      i_s_methods(inMethods)
  {}

  InfoSchemaTable()
    :
      table_name(NULL),
      hidden(false),
      is_opt_possible(false),
      first_field_index(0),
      second_field_index(0),
      requested_object(0),
      fields_info(0),
      i_s_methods(NULL)
  {}

  /**
   * Set the methods available on this I_S table.
   * @param[in] new_methods the methods to use
   */
  void setInfoSchemaMethods(InfoSchemaMethods *new_methods)
  {
    i_s_methods= new_methods;
  }

  /**
   * Create the temporary I_S tables using schema_table data.
   *
   * @param[in] session a session handler
   * @param[in] table_list Used to pass I_S table information (fields,
   *                       tables, parameters, etc.) and table name
   * @retval \# pointer to created table
   * @retval NULL Can't create table
   */
  Table *createSchemaTable(Session *session, TableList *table_list) const
  {
    Table *retval= i_s_methods->createSchemaTable(session, table_list);
    return retval;
  }

  /**
   * Fill I_S table.
   *
   * @param[in] session a session handler
   * @param[in] tables I_S table
   * @param[in] cond 'WHERE' condition
   * @return 0 on success; 1 on error
   */
  int fillTable(Session *session, TableList *tables, COND *cond)
  {
    int retval= i_s_methods->fillTable(session, tables, cond);
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
                   bool res, LEX_STRING *db_name, LEX_STRING *tab_name) const
  {
    int retval= i_s_methods->processTable(session, tables, table,
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
   * Set the I_S tables name.
   * @param[in] new_name the name to set the table to
   */
  void setTableName(const char *new_name)
  {
    table_name= new_name;
  }

  /**
   * @param[in] new_first_index value to set first field index to
   */
  void setFirstFieldIndex(int new_first_index)
  {
    first_field_index= new_first_index;
  }

  /**
   * @param[in] new_second_index value to set second field index to
   */
  void setSecondFieldIndex(int new_second_index)
  {
    second_field_index= new_second_index;
  }

  /**
   * @param[in] inFieldsInfo the fields info to use for this I_S table
   */
  void setFieldsInfo(ST_FIELD_INFO *in_fields_info)
  {
    fields_info= in_fields_info;
  }

  /**
   * @return the name of the I_S table.
   */
  const char *getTableName() const
  {
    return table_name;
  }

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
  int getFirstFieldIndex() const
  {
    return first_field_index;
  }

  /**
   * @return the index the second field.
   */
  int getSecondFieldIndex() const
  {
    return second_field_index;
  }

  /**
   * @return the requested object.
   */
  uint32_t getRequestedObject() const
  {
    return requested_object;
  }

  /**
   * @return the fields info for this I_S table.
   */
  ST_FIELD_INFO *getFieldsInfo() const
  {
    return fields_info;
  }

  /**
   * @param[in] field_index the index of this field
   * @return the field at the given index
   */
  ST_FIELD_INFO *getSpecificField(int field_index) const
  {
    return &fields_info[field_index];
  }

  /**
   * @param[in] field_index the index of this field
   * @return the name for the field at the given index
   */
  const char *getFieldName(int field_index) const
  {
    return fields_info[field_index].field_name;
  }

  /**
   * @param[in] field_index the index of this field
   * @return the open method for the field at the given index
   */
  int getFieldOpenMethod(int field_index) const
  {
    return fields_info[field_index].open_method;
  }

private:
  /**
   * I_S table name.
   */
  const char *table_name;

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
   * The index of the first field.
   */
  int first_field_index;

  /**
   * The index of the second field.
   */
  int second_field_index;

  /**
   * The object to open (TABLE | VIEW).
   */
  uint32_t requested_object;

  /**
   * The fields for this I_S table.
   */
  ST_FIELD_INFO *fields_info;

  /**
   * Contains the methods available on this I_S table.
   */
  InfoSchemaMethods *i_s_methods;

};

#endif /* DRIZZLED_INFO_SCHEMA_H */
