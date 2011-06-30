/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *    Jay Pipes <jaypipes@gmail.com.com>
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
 * Defines the implementation of the transaction log file descriptor.
 *
 * @details
 *
 * Currently, the transaction log file uses a simple, single-file, append-only
 * format.
 *
 * We have an atomic off_t called log_offset which keeps track of the 
 * offset into the log file for writing the next log entry.  The log
 * entries are written, one after the other, in the following way:
 *
 * <pre>
 * --------------------------------------
 * |<- 4 bytes ->|<- # Bytes of Entry ->|
 * --------------------------------------
 * |  Entry Type |  Serialized Entry    |
 * --------------------------------------
 * </pre>
 *
 * The Entry Type is an integer defined as an enumeration in the 
 * /drizzled/message/transaction.proto file called TransactionLogEntry::Type.
 *
 * Each transaction log entry type is written to the log differently.  Here,
 * we cover the format of each log entry type.
 *
 * Committed and Prepared Transaction Log Entries
 * -----------------------------------------------
 * 
 * <pre>
 * ------------------------------------------------------------------
 * |<- 4 bytes ->|<- # Bytes of Transaction Message ->|<- 4 bytes ->|
 * ------------------------------------------------------------------
 * |   Length    |   Serialized Transaction Message   |   Checksum  |
 * ------------------------------------------------------------------
 * </pre>
 *
 * @todo
 *
 * Possibly look at a scoreboard approach with multiple file segments.  For
 * right now, though, this is just a quick simple implementation to serve
 * as a skeleton and a springboard.
 */

#include <config.h>
#include "transaction_log.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <vector>
#include <string>

#include <drizzled/internal/my_sys.h> /* for internal::my_sync */
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/transaction_services.h>
#include <drizzled/algorithm/crc32.h>

#include <google/protobuf/io/coded_stream.h>

using namespace std;
using namespace drizzled;
using namespace google;

TransactionLog *transaction_log= NULL; /* The singleton transaction log */

TransactionLog::TransactionLog(const string &in_log_file_path,
                               uint32_t in_flush_frequency,
                               bool in_do_checksum) : 
    state(OFFLINE),
    log_file_path(in_log_file_path),
    has_error(false),
    error_message(),
    flush_frequency(in_flush_frequency),
    do_checksum(in_do_checksum)
{
  /* Setup our log file and determine the next write offset... */
  log_file= open(log_file_path.c_str(), O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU);
  if (log_file == -1)
  {
    char errmsg[STRERROR_MAX];
    strerror_r(errno, errmsg, sizeof(errmsg));
    error_message.assign(_("Failed to open transaction log file "));
    error_message.append(log_file_path);
    error_message.append("  Got error: ");
    error_message.append(errmsg);
    error_message.push_back('\n');
    has_error= true;
    return;
  }

  /* For convenience, grab the log file name from the path */
  if (log_file_path.find_first_of('/') != string::npos)
  {
    /* Strip to last / */
    string tmp;
    tmp= log_file_path.substr(log_file_path.find_last_of('/') + 1);
    log_file_name.assign(tmp);
  }
  else
    log_file_name.assign(log_file_path);

  /* 
   * The offset of the next write is the current position of the log
   * file, since it's opened in append mode...
   */
  log_offset= lseek(log_file, 0, SEEK_END);

  state= ONLINE;
}

uint8_t *TransactionLog::packTransactionIntoLogEntry(const message::Transaction &trx,
                                                     uint8_t *buffer,
                                                     uint32_t *checksum_out)
{
  uint8_t *orig_buffer= buffer;
  size_t message_byte_length= trx.ByteSize();

  /*
   * Write the header information, which is the message type and
   * the length of the transaction message into the buffer
   */
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(
      static_cast<uint32_t>(ReplicationServices::TRANSACTION), buffer);
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(
      static_cast<uint32_t>(message_byte_length), buffer);
  
  /*
   * Now write the serialized transaction message, followed
   * by the optional checksum into the buffer.
   */
  buffer= trx.SerializeWithCachedSizesToArray(buffer);

  if (do_checksum)
  {
    *checksum_out= drizzled::algorithm::crc32(
        reinterpret_cast<char *>(buffer) - message_byte_length, message_byte_length);
  }
  else
    *checksum_out= 0;

  /* We always write in network byte order */
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(*checksum_out, buffer);
  /* Reset the pointer back to its original location... */
  buffer= orig_buffer;
  return orig_buffer;
}

off_t TransactionLog::writeEntry(const uint8_t *data, size_t data_length)
{
  ssize_t written= 0;

  /*
   * Do an atomic increment on the offset of the log file position
   */
  off_t cur_offset= log_offset.fetch_and_add(static_cast<off_t>(data_length));

  /* 
   * Quick safety...if an error occurs above in another writer, the log 
   * file will be in a crashed state.
   */
  if (unlikely(state == CRASHED))
  {
    /* 
     * Reset the log's offset in case we want to produce a decent error message including
     * the original offset where an error occurred.
     */
    log_offset= cur_offset;
    return log_offset;
  }

  /* Write the full buffer in one swoop */
  do
  {
    written= pwrite(log_file, data, data_length, cur_offset);
  }
  while (written == -1 && errno == EINTR); /* Just retry the write when interrupted by a signal... */

  if (unlikely(written != static_cast<ssize_t>(data_length)))
  {
    char errmsg[STRERROR_MAX];
    strerror_r(errno, errmsg, sizeof(errmsg));
    errmsg_printf(error::ERROR, 
                  _("Failed to write full size of log entry.  Tried to write %" PRId64
                    " bytes at offset %" PRId64 ", but only wrote %" PRId32 " bytes.  Error: %s\n"), 
                  static_cast<int64_t>(data_length),
                  static_cast<int64_t>(cur_offset),
                  static_cast<int32_t>(written), 
                  errmsg);
    state= CRASHED;
    /* 
     * Reset the log's offset in case we want to produce a decent error message including
     * the original offset where an error occurred.
     */
    log_offset= cur_offset;
  }

  int error_code= syncLogFile();

  if (unlikely(error_code != 0))
  {
    sql_perror(_("Failed to sync log file."));
  }

  return cur_offset;
}

int TransactionLog::syncLogFile()
{
  switch (flush_frequency)
  {
  case FLUSH_FREQUENCY_EVERY_WRITE:
    return internal::my_sync(log_file, 0);
  case FLUSH_FREQUENCY_EVERY_SECOND:
    {
      time_t now_time= time(NULL);
      if (last_sync_time <= (now_time - 1))
      {
        last_sync_time= now_time;
        return internal::my_sync(log_file, 0);
      }
      return 0;
    }
  case FLUSH_FREQUENCY_OS:
  default:
    return 0;
  }
}

const string &TransactionLog::getLogFilename()
{
  return log_file_name;
}

const string &TransactionLog::getLogFilepath()
{
  return log_file_path;
}

void TransactionLog::truncate()
{
  /* 
   * @note
   *
   * This is NOT THREAD SAFE! DEBUG/TEST code only!
   */
  log_offset= (off_t) 0;
  int result;
  do
  {
    result= ftruncate(log_file, log_offset);
  }
  while (result == -1 && errno == EINTR);
}

bool TransactionLog::findLogFilenameContainingTransactionId(const ReplicationServices::GlobalTransactionId&,
                                                            string &out_filename) const
{
  /* 
   * Currently, we simply return the single logfile name
   * Eventually, we'll have an index/hash with upper and
   * lower bounds to look up a log file with a transaction id
   */
  out_filename.assign(log_file_path);
  return true;
}

bool TransactionLog::hasError() const
{
  return has_error;
}

void TransactionLog::clearError()
{
  has_error= false;
  error_message.clear();
}

const string &TransactionLog::getErrorMessage() const
{
  return error_message;
}

size_t TransactionLog::getLogEntrySize(const message::Transaction &trx)
{
  return trx.ByteSize() + HEADER_TRAILER_BYTES;
}
