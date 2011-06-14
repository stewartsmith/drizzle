/*
  Copyright (C) 2010 Stewart Smith

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

#include <config.h>
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>
#include "haildb_datadict_dump_func.h"

#include <haildb.h>

#include <sstream>
#include <string>

using namespace std;
using namespace drizzled;

class HailDBDatadictDumpFunction : public Item_str_func
{
public:
  HailDBDatadictDumpFunction() : Item_str_func() {}
  String *val_str(String*);

  void fix_length_and_dec()
  {
    max_length= 32767;
  }

  const char *func_name() const
  {
    return "haildb_datadict_dump";
  }

  bool check_argument_count(int n)
  {
    return (n == 0);
  }
};

struct schema_visitor_arg
{
  ib_trx_t transaction;
  string *str;
};

extern "C"
{

static int visit_table(void* arg_param, const char* name, ib_tbl_fmt_t tbl_fmt,
                       ib_ulint_t page_size, int n_cols, int n_indexes)
{
  struct schema_visitor_arg *arg= (struct schema_visitor_arg*)arg_param;
  std::stringstream ss;

  ss << name << " Format: ";

  switch (tbl_fmt)
  {
  case IB_TBL_REDUNDANT:
    ss << "REDUNDANT ";
    break;
  case IB_TBL_COMPACT:
    ss << "COMPACT ";
    break;
  case IB_TBL_DYNAMIC:
    ss << "DYNAMIC ";
    break;
  case IB_TBL_COMPRESSED:
    ss << "COMPRESSED ";
    break;
  default:
    ss << "UNKNOWN(" << tbl_fmt << ") ";
  }

  ss << "Page size: " << page_size
     << " Columns: " << n_cols
     << " Indexes: " << n_indexes
     << endl;

  arg->str->append(ss.str());

  return 0;
}

static int visit_table_col(void *arg_param, const char* name, ib_col_type_t, ib_ulint_t, ib_col_attr_t)
{
  struct schema_visitor_arg *arg= (struct schema_visitor_arg*)arg_param;
  std::stringstream ss;

  ss << "  COL: " << name << endl;

  arg->str->append(ss.str());

  return 0;
}

static int visit_index(void *arg_param, const char* name, ib_bool_t, ib_bool_t, int)
{
  struct schema_visitor_arg *arg= (struct schema_visitor_arg*)arg_param;
  std::stringstream ss;

  ss << "  IDX: " << name << endl;

  arg->str->append(ss.str());

  return 0;
}

static int visit_index_col(void* arg_param, const char* name, ib_ulint_t)
{
  struct schema_visitor_arg *arg= (struct schema_visitor_arg*)arg_param;
  std::stringstream ss;

  ss << "    IDXCOL: " << name << endl;

  arg->str->append(ss.str());

  return 0;
}

}

static const ib_schema_visitor_t visitor = {
  IB_SCHEMA_VISITOR_TABLE_AND_INDEX_COL,
  visit_table,
  visit_table_col,
  visit_index,
  visit_index_col
};

extern "C"
{

static int visit_tables(void* arg_param, const char *name, int len)
{
  ib_err_t        err;
  struct schema_visitor_arg *arg = (struct schema_visitor_arg*) arg_param;
  string table_name(name, len);

  err= ib_table_schema_visit(arg->transaction, table_name.c_str(), &visitor, arg_param);

  return(err == DB_SUCCESS ? 0 : -1);
}

}
String *HailDBDatadictDumpFunction::val_str(String *str)
{
  assert(fixed);

  str->alloc(50);
  null_value= false;

  string dict_dump("HailDB Data Dictionary Contents\n"
                   "-------------------------------\n");

  struct schema_visitor_arg arg;
  arg.str= &dict_dump;
  arg.transaction=  ib_trx_begin(IB_TRX_REPEATABLE_READ);

  ib_err_t err= ib_schema_lock_exclusive(arg.transaction);

  err = ib_schema_tables_iterate(arg.transaction, visit_tables, &arg);

  str->alloc(dict_dump.length());
  str->length(dict_dump.length());
  strncpy(str->ptr(), dict_dump.c_str(), dict_dump.length());

  ib_schema_unlock(arg.transaction);

  err= ib_trx_rollback(arg.transaction);
  assert (err == DB_SUCCESS);

  return str;
}


plugin::Create_function<HailDBDatadictDumpFunction> *haildb_datadict_dump_func= NULL;

int haildb_datadict_dump_func_initialize(module::Context &context)
{
  haildb_datadict_dump_func= new plugin::Create_function<HailDBDatadictDumpFunction>("haildb_datadict_dump");
  context.add(haildb_datadict_dump_func);
  return 0;
}
