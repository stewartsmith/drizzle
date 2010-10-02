/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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


#ifndef PLUGIN_INNOBASE_HANDLER_REPLICATION_DICTIONARY_H
#define PLUGIN_INNOBASE_HANDLER_REPLICATION_DICTIONARY_H 

#include "drizzled/plugin/table_function.h"
#include "drizzled/field.h"

#include "internal_dictionary.h"

struct log_record_st {
  uint64_t id;
  std::vector<char> buffer;

  log_record_st() :
    id(0)
  {
  }

  log_record_st(uint64_t id_arg, const char *buffer_arg, size_t length_arg) :
    id(id_arg)
  {
    buffer.resize(length_arg);
    memcpy(&buffer[0], buffer_arg, length_arg);
  }
};

class StructRecorder {
  std::vector<log_record_st> list;
  std::vector<log_record_st>::iterator iterator;
public:

  void start()
  {
    iterator= list.begin();
  }

  void push(uint64_t id_arg, const char *buffer_arg, size_t length_arg)
  {
    list.push_back(log_record_st(id_arg, buffer_arg, length_arg));
  }

  bool next(log_record_st &arg)
  {
    if (iterator == list.end())
      return false;

    arg= *iterator;

    iterator++;

    return true;
  }
};

class InnodbReplicationTable : public drizzled::plugin::TableFunction
{
public:
  InnodbReplicationTable();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
    StructRecorder recorder;

  public:
    Generator(drizzled::Field **arg);
                        
    bool populate();
  private:
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

#endif /* PLUGIN_INNOBASE_HANDLER_REPLICATION_DICTIONARY_H */
