/*****************************************************************************

Copyright (C) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

#include <config.h>

#include "internal_dictionary.h"

#include <drizzled/current_session.h>

#include "univ.i"
#include "btr0sea.h"
#include "os0file.h"
#include "os0thread.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "mtr0mtr.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0sel.h"
#include "row0upd.h"
#include "log0log.h"
#include "lock0lock.h"
#include "dict0crea.h"
#include "btr0cur.h"
#include "btr0btr.h"
#include "fsp0fsp.h"
#include "sync0sync.h"
#include "fil0fil.h"
#include "trx0xa.h"
#include "row0merge.h"
#include "thr0loc.h"
#include "dict0boot.h"
#include "ha_prototypes.h"
#include "ut0mem.h"
#include "ibuf0ibuf.h"
#include "handler0vars.h"

using namespace drizzled;

/*
 * Fill the dynamic table data_dictionary.INNODB_CMP and INNODB_CMP_RESET
 *
 */
InnodbInternalTables::InnodbInternalTables() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_INTERNAL_TABLES")
{
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
}

static void my_dict_print_callback(void *ptr, const char *table_name)
{
  Recorder *myrec= static_cast<Recorder *>(ptr);

  myrec->push(table_name);
}

InnodbInternalTables::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  dict_print_with_callback(my_dict_print_callback, &recorder);
  recorder.start();
}

bool InnodbInternalTables::Generator::populate()
{
  std::string table_name;
  bool more= recorder.next(table_name);

  if (not more)
    return false;

  /* TABLE_NAME */
  push(table_name);

  return true;
}
