/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Implementation of a transaction reader for the transaction log.
 *
 * @details
 *
 * This is currently an extremely simple implementation which
 * reads through the transaction log file one message at a time, using the
 * length-coded bytes to skip through the log.  Once it finds the message
 * which corresponds to the transaction id the caller to read() is looking
 * for, it copies the message into the supplied pointer and returns true.
 *
 * @todo
 *
 * Cache offsets so that readers don't have to continually scan through
 * the log file(s)
 */

#include <config.h>

#include <fcntl.h>

#include <climits>
#include <cerrno>
#include <cstdio>

#include "transaction_log_reader.h"
#include "transaction_log.h"

#include <drizzled/gettext.h>
#include <drizzled/message/transaction.pb.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <drizzled/algorithm/crc32.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/definitions.h>

using namespace std;
using namespace drizzled;
using namespace google;

bool TransactionLogReader::read(const ReplicationServices::GlobalTransactionId &to_read_trx_id, 
                            message::Transaction *to_fill)
{
  /* 
   * We ask the log to give us the log file containing the
   * transaction message with the needed transaction id, then
   * we read into the log file to obtain the message, and 
   * fill the supplied pointer to Transaction message from the
   * raw data in the log file.
   */
  string log_filename_to_read;
  bool log_file_found= log.findLogFilenameContainingTransactionId(to_read_trx_id, log_filename_to_read);
  bool result= true;
  bool do_checksum= false;

  if (unlikely(! log_file_found))
  {
    return false;
  }
  else
  {
    /* Open the log file and read through the log until the transaction ID is found */
    int log_file= open(log_filename_to_read.c_str(), O_RDONLY | O_NONBLOCK);

    if (log_file == -1)
    {
      sql_perror(_("Failed to open transaction log file"), log_filename_to_read);
      return false;
    }

    protobuf::io::ZeroCopyInputStream *raw_input= new protobuf::io::FileInputStream(log_file);
    protobuf::io::CodedInputStream *coded_input= new protobuf::io::CodedInputStream(raw_input);

    char *buffer= NULL;
    char *temp_buffer= NULL;
    uint32_t length= 0;
    uint32_t previous_length= 0;
    uint32_t checksum= 0;

    message::Transaction transaction;

    /* Read in the length of the command */
    while (result == true && coded_input->ReadLittleEndian32(&length) == true)
    {
      if (length > INT_MAX)
      {
        fprintf(stderr, _("Attempted to read record bigger than INT_MAX\n"));
        exit(1);
      }

      if (buffer == NULL)
      {
        /* 
        * First time around...just malloc the length.  This block gets rid
        * of a GCC warning about uninitialized temp_buffer.
        */
        temp_buffer= (char *) malloc(static_cast<size_t>(length));
      }
      /* No need to allocate if we have a buffer big enough... */
      else if (length > previous_length)
      {
        temp_buffer= (char *) realloc(buffer, static_cast<size_t>(length));
      }

      if (temp_buffer == NULL)
      {
        fprintf(stderr, _("Memory allocation failure trying to allocate %" PRIu64 " bytes.\n"),
                static_cast<uint64_t>(length));
        break;
      }
      else
        buffer= temp_buffer;

      /* Read the Command */
      result= coded_input->ReadRaw(buffer, length);
      if (result == false)
      {
        char errmsg[STRERROR_MAX];
        strerror_r(errno, errmsg, sizeof(errmsg));
        fprintf(stderr, _("Could not read transaction message.\n"));
        fprintf(stderr, _("GPB ERROR: %s.\n"), errmsg);
        fprintf(stderr, _("Raw buffer read: %s.\n"), buffer);
        break;
      }

      result= transaction.ParseFromArray(buffer, static_cast<int32_t>(length));
      if (result == false)
      {
        fprintf(stderr, _("Unable to parse transaction. Got error: %s.\n"), transaction.InitializationErrorString().c_str());
        if (buffer != NULL)
          fprintf(stderr, _("BUFFER: %s\n"), buffer);
        break;
      }

      /* Skip 4 byte checksum */
      coded_input->ReadLittleEndian32(&checksum);

      if (do_checksum)
      {
        if (checksum != drizzled::algorithm::crc32(buffer, static_cast<size_t>(length)))
        {
          fprintf(stderr, _("Checksum failed. Wanted %" PRIu32 " got %" PRIu32 "\n"), checksum, drizzled::algorithm::crc32(buffer, static_cast<size_t>(length)));
        }
      }

      /* Cool, message was read.  Check the trx id */
      if (transaction.transaction_context().transaction_id() == to_read_trx_id)
      {
        /* Found what we were looking for...copy to the pointer we should fill */
        to_fill->CopyFrom(transaction);
        break;
      }

      previous_length= length;
    }
    free(buffer);
    
    delete coded_input;
    delete raw_input;

    return result;
  }
}
