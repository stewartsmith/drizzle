/*
  Copyright (C) 2011 Stewart Smith

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
#include <drizzled/plugin/table_function.h>
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/algorithm/crc32.h>

#include "engine_state_history.h"

#include <string>
#include <vector>

using namespace std;
using namespace drizzled;

std::vector<std::string> engine_state_history;

class EngineStateHistory : public drizzled::plugin::TableFunction
{
public:

  EngineStateHistory();

  EngineStateHistory(const char *table_arg) :
    drizzled::plugin::TableFunction("data_dictionary", table_arg)
  { }

  ~EngineStateHistory() {}

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  private:
    std::vector<std::string>::iterator it;
  public:
    Generator(drizzled::Field **arg);
    ~Generator();

    bool populate();
  };

  EngineStateHistory::Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

EngineStateHistory::EngineStateHistory() :
  plugin::TableFunction("DATA_DICTIONARY", "SEAPITESTER_ENGINE_STATE_HISTORY")
{
  add_field("STATE");
}

EngineStateHistory::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  it= engine_state_history.begin();
}

EngineStateHistory::Generator::~Generator()
{
}

bool EngineStateHistory::Generator::populate()
{
  if (engine_state_history.empty())
    return false;

  if (it == engine_state_history.end())
    return false; // No more rows

  push(*it);
  it++;


  return true;
}

class ClearEngineStateHistoryFunction :public Item_int_func
{
public:
  int64_t val_int();

  ClearEngineStateHistoryFunction() :Item_int_func()
  {
    unsigned_flag= true;
  }

  const char *func_name() const
  {
    return "seapitester_clear_engine_state_history";
  }

  void fix_length_and_dec()
  {
    max_length= 10;
  }

  bool check_argument_count(int n)
  {
    return (n == 0);
  }
};

extern uint64_t next_cursor_id;

int64_t ClearEngineStateHistoryFunction::val_int()
{
  engine_state_history.clear();
  null_value= false;
  next_cursor_id= 0;
  return 0;
}


int engine_state_history_table_initialize(drizzled::module::Context &context)
{
  context.add(new EngineStateHistory);
  context.add(new plugin::Create_function<ClearEngineStateHistoryFunction>("SEAPITESTER_CLEAR_ENGINE_STATE_HISTORY"));

  return 0;
}
