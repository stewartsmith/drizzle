/* Drizzle
 * Copyright (C) 2011 Olaf van der Spek
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

namespace drizzled {

namespace catalog
{
  class Instance;

  namespace lock 
  {
    class Create;
    class Erase;
  }
}

namespace field 
{
  class TableShare;
}

namespace generator 
{
  class TableDefinitionCache;

  namespace catalog 
  {
    class Cache;
    class Instance;
  }
}

namespace internal
{
  typedef struct st_io_cache IO_CACHE;
  
  struct st_my_thread_var;
}

namespace identifier
{
  class Catalog;
  class Schema;
  class Table;
  class User;

  typedef int64_t Session;

  namespace catalog
  {
    typedef std::vector<Catalog> vector;
  }

  namespace schema
  {
    typedef std::vector<Schema> vector;
  }

  namespace table
  {
    // typedef message::Table::TableType Type;
    typedef std::vector<Table> vector;
  }

  namespace user
  {
    typedef boost::shared_ptr<const User> ptr;
    typedef boost::shared_ptr<User> mptr;
  }
}

namespace item
{
  class Boolean;
  class False;
  class True;
}

namespace memory
{
  class Root;
}

namespace message
{
  class AlterSchemaStatement;
  class AlterTable;
  class CreateSchemaStatement;
  class CreateTableStatement;
  class DeleteData;
  class DeleteHeader;
  class DeleteRecord;
  class DropSchemaStatement;
  class DropTableStatement;
  class InsertData;
  class InsertHeader;
  class InsertRecord;
  class Resultset;
  class Schema;
  class SetVariableStatement;
  class Statement;
  class Transaction;
  class TruncateTableStatement;
  class UpdateData;
  class UpdateHeader;
  class UpdateRecord;
}

namespace module
{
  class Graph;
  class Library;
  class Manifest;
  class Module;
  class option_map;
  class Registry;
  class VertexHandle;
}

namespace plugin 
{ 
  class Catalog;
  class Client;
  class EventObserverList;
  class Function;
  class MonitoredInTransaction;
  class Scheduler;
  class StorageEngine;
  class TransactionApplier;
  class TransactionReplicator;
  class TransactionalStorageEngine;
  class XaResourceManager;
  class XaStorageEngine;
}

namespace optimizer 
{ 
  class compare_functor;
  class CostVector; 
  class Position;
  class SqlSelect;
}

namespace session 
{ 
  class State; 
  class TableMessages;
  class Transactions;
}

namespace sql
{
  class ResultSet;
}

namespace statement 
{ 
  class Statement; 
}

namespace table 
{ 
  class Placeholder; 
  class Singular; 
}

namespace type 
{ 
  class Decimal;
  class Time; 
}

namespace util
{
  class Storable;

  namespace string
  {
    typedef boost::shared_ptr<const std::string> ptr;
    typedef boost::shared_ptr<std::string> mptr;
    typedef std::vector<std::string> vector;
  }
}

typedef class Item COND;
typedef struct my_locale_st MY_LOCALE;
typedef struct system_status_var system_status_var;

class AlterColumn;
class AlterDrop;
class AlterInfo;
class Arg_comparator;
class Cached_item;
class CachedDirectory;
class CopyField;
class CopyInfo;
class Create_func;
class CreateField;
class Cursor;
class Date;
class DateTime;
class Diagnostics_area;
class DRIZZLE_ERROR;
class DrizzleLock;
class DrizzleXid;
class Field;
class Field_blob;
class file_exchange;
class ForeignKeyInfo;
class Hybrid_type;
class Hybrid_type_traits;
class Identifier;
class Index_hint;
class Internal_error_handler;
class Item;
class Item_bool_func2;
class Item_cache;
class Item_equal;
class Item_field;
class Item_func;
class Item_func_not_all;
class Item_func_set_user_var;
class Item_ident;
class Item_in_optimizer;
class Item_in_subselect;
class Item_maxmin_subselect;
class Item_row;
class Item_subselect;
class Item_sum;
class Item_sum_avg;
class Item_sum_hybrid;
class Item_sum_std;
class Item_sum_variance;
class Join;
class JoinTable;
class KeyInfo;
class LEX;
class Lex_input_stream;
class lex_string_t;
class Name_resolution_context;
class NamedSavepoint;
class Natural_join_column;
class ResourceContext;
class RorIntersectReadPlan; 
class SecurityContext;
class Select_Lex;
class Select_Lex_Unit;
class select_result;
class select_result_interceptor;
class select_union;
class SendField;
class Session;
class SortField;
class SortParam;
class StoredKey;
class String;
class subselect_engine;
class subselect_hash_sj_engine;
class sys_var;
class sys_var_str;
class system_status_var;
class Table;
class Table_ident;
class TableList;
class TableShare;
class TableShareInstance;
class Temporal;
class Time;
class Time_zone;
class Timestamp;
class Tmp_Table_Param;
class TYPELIB;
class Unique;
class user_var_entry;
class var;

struct CacheField;
struct Ha_data;
struct charset_info_st;
struct option;
struct Order;

typedef Item COND;
typedef uint64_t query_id_t;
typedef int64_t session_id_t;
typedef uint64_t my_xid;

} // namespace drizzled
