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

#include "config.h"

#include "replication_dictionary.h"

#include "drizzled/current_session.h"

extern "C" {
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
#include "mysql_addons.h"
#include "create_replication.h"
}
#include "handler0vars.h"

#include "drizzled/replication_services.h"
#include "drizzled/message/transaction.pb.h"

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>

using namespace drizzled;

/*
 * Fill the dynamic table data_dictionary.INNODB_CMP and INNODB_CMP_RESET
 *
 */
InnodbReplicationTable::InnodbReplicationTable() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_REPLICATION_LOG")
{
  add_field("TRANSACTION_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("TRANSACTION_MESSAGE", plugin::TableFunction::STRING, 512, false);
  add_field("TRANSACTION_LENGTH", plugin::TableFunction::NUMBER, 0, false);
}

extern "C" {
  void my_replication_print_callback(void *ptr, uint64_t);
}

void my_replication_print_callback(void *ptr, uint64_t transaction_id, const char *buffer_arg, size_t buffer_length)
{
  StructRecorder *myrec= static_cast<StructRecorder *>(ptr);

  myrec->push(transaction_id, buffer_arg, buffer_length);
}

InnodbReplicationTable::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  replication_print_with_callback(my_replication_print_callback, &recorder);
  recorder.start();
}

bool InnodbReplicationTable::Generator::populate()
{
  log_record_st id;
  bool more= recorder.next(id);

  if (not more)
    return false;

  /* TABLE_NAME */
  push(id.id);

  /* Message in viewable format */
  std::string transaction_text;
  message::Transaction message;
  bool result= message.ParseFromArray(&id.buffer[0], id.buffer.size());
  if (result == false)
  {
    fprintf(stderr, _("Unable to parse transaction. Got error: %s.\n"), message.InitializationErrorString().c_str());
    push("error");
  }
  else
  {
    google::protobuf::TextFormat::PrintToString(message, &transaction_text);
    push(transaction_text);
  }
  push(static_cast<int64_t>(id.buffer.size()));

  return true;
}
