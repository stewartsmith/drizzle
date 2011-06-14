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

#include <config.h>

#include "replication_dictionary.h"
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
#include "create_replication.h"
#include "read_replication.h"
#include "handler0vars.h"

#include <drizzled/drizzled.h>

#include <drizzled/replication_services.h>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>
#include <string>

using namespace drizzled;

/*
 * Fill the dynamic table data_dictionary.INNODB_CMP and INNODB_CMP_RESET
 *
 */
InnodbReplicationTable::InnodbReplicationTable() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_REPLICATION_LOG")
{
  add_field("TRANSACTION_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("TRANSACTION_SEGMENT_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("COMMIT_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("END_TIMESTAMP", plugin::TableFunction::NUMBER, 0, false);
  add_field("ORIGINATING_SERVER_UUID", plugin::TableFunction::STRING, 36, false);
  add_field("ORIGINATING_COMMIT_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("TRANSACTION_MESSAGE_STRING", plugin::TableFunction::STRING, transaction_message_threshold, false);
  add_field("TRANSACTION_LENGTH", plugin::TableFunction::NUMBER, 0, false);
}

InnodbReplicationTable::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  replication_state =replication_read_init();
}

InnodbReplicationTable::Generator::~Generator()
{
  replication_read_deinit(replication_state);
}

bool InnodbReplicationTable::Generator::populate()
{
  struct read_replication_return_st ret= replication_read_next(replication_state);

  if (ret.message == NULL)
    return false;

  /* Transaction ID */
  push(static_cast<uint64_t>(ret.id));

  /* Segment ID */
  push(static_cast<uint64_t>(ret.seg_id));

  push(static_cast<uint64_t>(ret.commit_id));

  push(static_cast<uint64_t>(ret.end_timestamp));

  push(ret.originating_server_uuid);

  push(static_cast<uint64_t>(ret.originating_commit_id));
  
  /* Message in viewable format */
  bool result= message.ParseFromArray(ret.message, ret.message_length);

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

  push(static_cast<int64_t>(ret.message_length));

  return true;
}
