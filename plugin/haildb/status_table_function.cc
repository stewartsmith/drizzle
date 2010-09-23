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

#include "config.h"
#include "drizzled/plugin/table_function.h"

#if defined(HAVE_HAILDB_H)
# include <haildb.h>
#else
# include <embedded_innodb-1.0/innodb.h>
#endif /* HAVE_HAILDB_H */

#include "status_table_function.h"

using namespace std;
using namespace drizzled;

class LibInnoDBStatusTool : public drizzled::plugin::TableFunction
{
public:

  LibInnoDBStatusTool();

  LibInnoDBStatusTool(const char *table_arg) :
    drizzled::plugin::TableFunction("data_dictionary", table_arg)
  { }

  ~LibInnoDBStatusTool() {}

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  private:
    uint32_t names_next;
  public:
    Generator(drizzled::Field **arg);
    ~Generator();

    bool populate();
  };

  LibInnoDBStatusTool::Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

static const char*      libinnodb_status_var_names[] = {
  "read_req_pending",
  "write_req_pending",
  "fsync_req_pending",
  "write_req_done",
  "read_req_done",
  "fsync_req_done",
  "bytes_total_written",
  "bytes_total_read",

  "buffer_pool_current_size",
  "buffer_pool_data_pages",
  "buffer_pool_dirty_pages",
  "buffer_pool_misc_pages",
  "buffer_pool_free_pages",
  "buffer_pool_read_reqs",
  "buffer_pool_reads",
  "buffer_pool_waited_for_free",
  "buffer_pool_pages_flushed",
  "buffer_pool_write_reqs",
  "buffer_pool_total_pages",
  "buffer_pool_pages_read",
  "buffer_pool_pages_written",

  "double_write_pages_written",
  "double_write_invoked",

  "log_buffer_slot_waits",
  "log_write_reqs",
  "log_write_flush_count",
  "log_bytes_written",
  "log_fsync_req_done",
  "log_write_req_pending",
  "log_fsync_req_pending",

  "lock_row_waits",
  "lock_row_waiting",
  "lock_total_wait_time_in_secs",
  "lock_wait_time_avg_in_secs",
  "lock_max_wait_time_in_secs",

  "row_total_read",
  "row_total_inserted",
  "row_total_updated",
  "row_total_deleted",

  "page_size",
  "have_atomic_builtins",
  NULL
};

LibInnoDBStatusTool::LibInnoDBStatusTool() :
  plugin::TableFunction("DATA_DICTIONARY", "HAILDB_STATUS")
{
  add_field("NAME");
  add_field("VALUE", plugin::TableFunction::NUMBER);
}

LibInnoDBStatusTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  names_next(0)
{
}

LibInnoDBStatusTool::Generator::~Generator()
{
}

bool LibInnoDBStatusTool::Generator::populate()
{
  if (libinnodb_status_var_names[names_next] != NULL)
  {
    const char* config_name= libinnodb_status_var_names[names_next];

    push(config_name);

    ib_i64_t value;
    ib_err_t err= ib_status_get_i64(config_name, &value);
    assert(err == DB_SUCCESS);

    push(value);

    names_next++;
    return true;
  }

  return false; // No more rows
}

static LibInnoDBStatusTool *status_tool;

int status_table_function_initialize(drizzled::module::Context &context)
{
  status_tool= new(std::nothrow)LibInnoDBStatusTool();
  context.add(status_tool);

  return 0;
}
