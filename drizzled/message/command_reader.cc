/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <drizzled/global.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <drizzled/message/replication.pb.h>

#include "drizzled/gettext.h"
#include "drizzled/korr.h"

using namespace std;
using namespace drizzled;

/**
 * @file Example application for reading change records (Command messages)
 *
 * @note
 *
 * This program is used in the serial_event_log test suite to verify
 * the log written by that plugin.
 */

static void printInsert(const message::Command &container,
                        const message::InsertRecord &record)
{

  cout << "INSERT INTO `" << container.schema() << "`.`" << container.table() << "` (";
  
  assert(record.insert_field_size() > 0);
  assert(record.insert_value_size() > 0);
  assert(record.insert_value_size() % record.insert_field_size() == 0);

  int32_t num_fields= record.insert_field_size();

  int32_t x;
  for (x= 0; x < num_fields; x++)
  {
    if (x != 0)
      cout << ", ";

    const message::Table::Field f= record.insert_field(x);

    cout << "`" << f.name() << "`";
  }

  cout << ") VALUES ";

  /* 
   * There may be an INSERT VALUES (),() type statement.  We know the
   * number of records is equal to the field_values array size divided
   * by the number of fields.
   *
   * So, we do an inner and an outer loop.  Outer loop is on the number
   * of records and the inner loop on the number of fields.  In this way, 
   * we know that record.field_values(outer_loop * num_fields) + inner_loop))
   * always gives us our correct field value.
   */
  int32_t num_records= (record.insert_value_size() / num_fields);
  int32_t y;
  for (x= 0; x < num_records; x++)
  {
    if (x != 0)
      cout << ", ";

    cout << "(";
    for (y= 0; y < num_fields; y++)
    {
      if (y != 0)
        cout << ", ";

      cout << "\"" << record.insert_value((x * num_fields) + y) << "\"";
    }
    cout << ")";
  }

  cout << ";";
}

static void printDeleteWithPK(const message::Command &container,
                              const message::DeleteRecord &record)
{
  cout << "DELETE FROM `" << container.schema() << "`.`" << container.table() << "`";
  
  assert(record.where_field_size() > 0);
  assert(record.where_value_size() == record.where_field_size());

  int32_t num_where_fields= record.where_field_size();
  /* 
   * Make sure we catch anywhere we're not aligning the fields with
   * the field_values arrays...
   */
  assert(num_where_fields == record.where_value_size());

  cout << " WHERE ";
  int32_t x;
  for (x= 0; x < num_where_fields; x++)
  {
    if (x != 0)
      cout << " AND "; /* Always AND condition with a multi-column PK */

    const message::Table::Field f= record.where_field(x);

    /* Always equality conditions */
    cout << "`" << f.name() << "` = \"" << record.where_value(x) << "\"";
  }

  cout << ";";
}

static void printUpdateWithPK(const message::Command &container,
                              const message::UpdateRecord &record)
{
  int32_t num_update_fields= record.update_field_size();
  int32_t x;
  
  assert(record.update_field_size() > 0);
  assert(record.where_field_size() > 0);
  assert(record.where_value_size() == record.where_field_size());

  cout << "UPDATE `" << container.schema() << "`.`" << container.table() << "` SET ";

  for (x= 0;x < num_update_fields; x++)
  {
    message::Table::Field f= record.update_field(x);
    
    if (x != 0)
      cout << ", ";

    cout << "`" << f.name() << "` = \"" << record.after_value(x) << "\"";
  }

  int32_t num_where_fields= record.where_field_size();
  /* 
   * Make sure we catch anywhere we're not aligning the fields with
   * the field_values arrays...
   */
  assert(num_where_fields == record.where_value_size());

  cout << " WHERE ";
  for (x= 0;x < num_where_fields; x++)
  {
    if (x != 0)
      cout << " AND "; /* Always AND condition with a multi-column PK */

    const message::Table::Field f= record.where_field(x);

    /* Always equality conditions */
    cout << "`" << f.name() << "` = \"" << record.where_value(x) << "\"";
  }
  cout << ";";
}

static void printCommand(const message::Command &command)
{
  cout << "/* Timestamp: " << command.timestamp() << " */"<< endl;

  message::TransactionContext trx= command.transaction_context();

  cout << "/* SERVER ID: " << trx.server_id() << " TRX ID: " << trx.transaction_id();
  
  if (command.has_session_id())
    cout << " SESSION ID: " << command.session_id();

  cout << " */ ";

  switch (command.type())
  {
    case message::Command::START_TRANSACTION:
      cout << "START TRANSACTION;";
      break;
    case message::Command::COMMIT:
      cout << "COMMIT;";
      break;
    case message::Command::ROLLBACK:
      cout << "ROLLBACK;";
      break;
    case message::Command::INSERT:
    {
      printInsert(command, command.insert_record());
      break;
    }
    case message::Command::DELETE:
    {
      printDeleteWithPK(command, command.delete_record());
      break;
    }
    case message::Command::UPDATE:
    {
      printUpdateWithPK(command, command.update_record());
      break;
    }
    case message::Command::RAW_SQL:
    {
      std::string sql= command.sql();
      /* Replace \n with spaces */
      const std::string newline= "\n";
      while (sql.find(newline) != std::string::npos)
        sql.replace(sql.find(newline), 1, " ");

      cout << sql << ";";
      break;
    }
    default:
      cout << "Received an unknown Command type: " << (int32_t) command.type();
  }
  cout << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int file;

  if (argc != 2)
  {
    fprintf(stderr, _("Usage: %s COMMAND_LOG\n"), argv[0]);
    return -1;
  }

  message::Command command;

  file= open(argv[1], O_RDONLY);
  if (file == -1)
  {
    fprintf(stderr, _("Cannot open file: %s\n"), argv[1]);
  }

  char *buffer= NULL;
  char *temp_buffer= NULL;
  uint64_t previous_length= 0;
  ssize_t read_bytes= 0;
  uint64_t length= 0;
  uint32_t checksum= 0;

  /* We use korr.h macros when writing and must do the same when reading... */
  unsigned char coded_length[8];
  unsigned char coded_checksum[4];

  /* Read in the length of the command */
  while ((read_bytes= read(file, coded_length, sizeof(uint64_t))) != 0)
  {
    if (read_bytes == -1)
    {
      fprintf(stderr, _("Failed to read initial length header\n"));
      exit(1);
    }
    length= uint8korr(coded_length);

    if (length > SIZE_MAX)
    {
      fprintf(stderr, _("Attempted to read record bigger than SIZE_MAX\n"));
      exit(1);
    }

    if (buffer == NULL)
    {
      /* 
       * First time around...just malloc the length.  This block gets rid
       * of a GCC warning about uninitialized temp_buffer.
       */
      temp_buffer= (char *) malloc((size_t) length);
    }
    /* No need to allocate if we have a buffer big enough... */
    else if (length > previous_length)
    {
      temp_buffer= (char *) realloc(buffer, (size_t) length);
    }

    if (temp_buffer == NULL)
    {
      fprintf(stderr, _("Memory allocation failure trying to allocate %" PRIu64 " bytes.\n"), length);
      exit(1);
    }
    else
      buffer= temp_buffer;

    /* Read the Command */
    read_bytes= read(file, buffer, (size_t) length);
    if ((read_bytes != (ssize_t) length))
    {
      fprintf(stderr, _("Could not read entire transaction. Read %" PRIu64 " bytes instead of %" PRIu64 " bytes.\n"), (uint64_t) read_bytes, (uint64_t) length);
      exit(1);
    }

    if (! command.ParseFromArray(buffer, (int) length))
    {
      fprintf(stderr, _("Unable to parse command. Got error: %s.\n"), command.InitializationErrorString().c_str());
      if (buffer != NULL)
        fprintf(stderr, _("BUFFER: %s\n"), buffer);
      exit(1);
    }

    /* Read the checksum */
    read_bytes= read(file, coded_checksum, sizeof(uint32_t));
    if ((read_bytes != (ssize_t) sizeof(uint32_t)))
    {
      fprintf(stderr, _("Could not read entire checksum. Read %" PRIu64 " bytes instead of 4 bytes.\n"), (uint64_t) read_bytes);
      exit(1);
    }
    checksum= uint4korr(coded_checksum);

    if (checksum != 0)
    {
      /* @TODO checksumming.. */
    }

    /* Print the command */
    printCommand(command);

    /* Reset our length check */
    previous_length= length;
    memset(coded_length, 0, sizeof(coded_length));
  }
  if (buffer)
    free(buffer);
  return 0;
}
