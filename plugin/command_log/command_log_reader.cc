/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * Implementation of a command reader for the command log.
 *
 * @details
 *
 * This is currently an extremely simple implementation which
 * reads through the command log file one message at a time, using the
 * length-coded bytes to skip through the log.  Once it finds the message
 * which corresponds to the transaction id the caller to read() is looking
 * for, it copies the message into the supplied pointer and returns true.
 *
 * @todo
 *
 * Cache offsets so that readers don't have to continually scan through
 * the log file(s)
 */

#include "command_log_reader.h"

#include <drizzled/gettext.h>
#include <drizzled/message/replication.pb.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace std;
using namespace drizzled;
using namespace google;

bool CommandLogReader::read(const ReplicationServices::GlobalTransactionId &to_read_trx_id, 
                            message::Command *to_fill)
{
  /* 
   * We ask the log to give us the log file containing the
   * command message with the needed transaction id, then
   * we read into the log file to obtain the message, and 
   * fill the supplied pointer to Command message from the
   * raw data in the log file.
   */
  string log_filename_to_read;
  bool log_file_found= log.findLogFilenameContainingTransactionId(to_read_trx_id, log_filename_to_read);
  bool result= true;

  if (unlikely(! log_file_found))
  {
    return false;
  }
  else
  {
    protobuf::io::FileInputStream *log_file_stream;
    message::Command tmp_command; /* Used to check trx id... */

    unsigned char coded_length[8]; /* In network byte order */
    uint64_t length= 0; /* The length of the command to follow in stream */
    ssize_t read_bytes; /* Number bytes read during pread() calls */

    off_t current_offset= 0;

    /* Open the log file and read through the log until the transaction ID is found */
    int log_file= open(log_filename_to_read.c_str(), O_RDONLY | O_NONBLOCK);

    if (log_file == -1)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to open command log file %s.  Got error: %s\n"), log_filename_to_read.c_str(), strerror(errno));
      return false;
    }

    log_file_stream= new protobuf::io::FileInputStream(log_file); /* Zero-copy stream implementation */

    while (true)
    {
      /* Read in the length of the command */
      do
      {
        read_bytes= pread(log_file, coded_length, sizeof(uint64_t), current_offset);
      }
      while (read_bytes == EINTR); /* Just retry the call when interrupted by a signal... */

      if (unlikely(read_bytes < 0))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to read length header at offset %" PRId64 ".  Got error: %s\n"), 
                      (int64_t) current_offset, 
                      strerror(errno));
        result= false;
        break;
      }
      if (read_bytes == 0)
      {
        /* End of file and did not find the command, so return false */
        result= false;
        break;
      }
      
      /* We use korr.h macros when writing and must do the same when reading... */
      length= uint8korr(coded_length);

      /* Skip to the start of the next Command */
      log_file_stream->Skip(8);

      if (unlikely(tmp_command.ParseFromBoundedZeroCopyStream(log_file_stream, length) == false))
      {
        tmp_command.Clear();
        errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to parse command message at offset %" PRId64 ".  Got error: %s\n"), 
                      (int64_t) current_offset, 
                      tmp_command.InitializationErrorString().c_str());
        result= false;
        break;
      }
      else
      {
        /* Cool, message was read.  Check the trx id */
        if (tmp_command.transaction_context().transaction_id() == to_read_trx_id)
        {
          /* Found what we were looking for...copy to the pointer we should fill */
          to_fill->CopyFrom(tmp_command);
          break;
        }
      }

      /* Keep the stream and the pread() calls in sync... */
      current_offset+= length;
    }
    delete log_file_stream;
    close(log_file);
    return result;
  }
}
