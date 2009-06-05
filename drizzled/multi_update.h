/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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


#ifndef DRIZZLED_MULTI_UPDATE_H
#define DRIZZLED_MULTI_UPDATE_H

#include <list>

class multi_update :public select_result_interceptor
{
  TableList *all_tables; /* query/update command tables */
  TableList *leaves;     /* list of leves of join table tree */
  std::list<TableList*> update_tables;
  TableList *table_being_updated;
  Table **tmp_tables, *main_table, *table_to_update;
  Tmp_Table_Param *tmp_table_param;
  ha_rows updated, found;
  List <Item> *fields, *values;
  List <Item> **fields_for_table, **values_for_table;
  uint32_t table_count;
  /*
   List of tables referenced in the CHECK OPTION condition of
   the updated view excluding the updated table.
  */
  List <Table> unupdated_check_opt_tables;
  CopyField *copy_field;
  enum enum_duplicates handle_duplicates;
  bool do_update, trans_safe;
  /* True if the update operation has made a change in a transactional table */
  bool transactional_tables;
  bool ignore;
  /*
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

public:
  multi_update(TableList *ut, TableList *leaves_list,
	       List<Item> *fields, List<Item> *values,
	       enum_duplicates handle_duplicates, bool ignore);
  ~multi_update();
  int prepare(List<Item> &list, Select_Lex_Unit *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint32_t errcode,const char *err);
  int  do_updates();
  bool send_eof();
  virtual void abort();
};

#endif /* DRIZZLED_MULTI_UPDATE_H */
