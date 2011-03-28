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
  class Resultset;
  class Schema;
  class Statement;
  class Transaction;
}

namespace module
{
  class Registry;
}

namespace plugin 
{ 
  class Client;
  class EventObserverList;
  class MonitoredInTransaction;
  class Scheduler;
  class StorageEngine;
  class TransactionApplier;
  class TransactionReplicator;
  class TransactionalStorageEngine;
  class XaResourceManager;
}

namespace optimizer 
{ 
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
class CachedDirectory;
class CopyField;
class CreateField;
class Create_func;
class Date;
class DateTime;
class Diagnostics_area;
class DrizzleLock;
class DrizzleXid;
class Field;
class Field_blob;
class ForeignKeyInfo;
class Hybrid_type;
class Hybrid_type_traits;
class Identifier;
class Internal_error_handler;
class Item;
class Item_bool_func2;
class Item_equal;
class Item_field;
class Item_ident;
class Item_in_subselect;
class Item_row;
class Item_sum;
class Item_sum_hybrid;
class Join;
class JoinTable;
class LEX;
class Lex_input_stream;
class Name_resolution_context;
class ResourceContext;
class SecurityContext;
class Select_Lex;
class Select_Lex_Unit;
class SendField;
class Session;
class SortField;
class SortParam;
class String;
class TYPELIB;
class Table;
class TableList;
class TableShare;
class TableShareInstance;
class Table_ident;
class Time;
class Time_zone;
class select_result;
class sys_var_str;
class system_status_var;
class user_var_entry;
class var;

struct CacheField;
struct Ha_data;
struct charset_info_st;
struct option;

struct Order;

typedef Item COND;
typedef int64_t session_id_t;
typedef uint64_t my_xid;

} // namespace drizzled
