/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *  Copyright (c) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *  Jay Pipes <jaypipes@gmail.com.com>
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
 * Defines the implementation of the transaction applier plugin
 * for the transaction log.
 *
 * @see drizzled/plugin/transaction_replicator.h
 * @see drizzled/plugin/transaction_applier.h
 *
 * @details
 *
 * The TransactionLogApplier::apply() method constructs the entry
 * in the transaction log from the supplied Transaction message and
 * asks its associated TransactionLog object to write this entry.
 *
 * Upon a successful write, the applier adds some information about
 * the written transaction to the transaction log index.
 */

#include "config.h"
#include "transaction_log.h"
#include "transaction_log_applier.h"
#include "transaction_log_index.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/algorithm/crc32.h>
#include <drizzled/message/transaction.pb.h>
#include <google/protobuf/io/coded_stream.h>

using namespace std;
using namespace drizzled;
using namespace google;

TransactionLogApplier *transaction_log_applier= NULL; /* The singleton transaction log applier */

extern TransactionLogIndex *transaction_log_index;

TransactionLogApplier::TransactionLogApplier(const string name_arg,
                                             TransactionLog &in_transaction_log,
                                             bool in_do_checksum) :
  plugin::TransactionApplier(name_arg),
  transaction_log(in_transaction_log),  
  do_checksum(in_do_checksum)
{
}

TransactionLogApplier::~TransactionLogApplier()
{
}

plugin::ReplicationReturnCode
TransactionLogApplier::apply(const message::Transaction &to_apply)
{
  uint8_t *buffer; /* Buffer we will write serialized header, 
                      message and trailing checksum to */
  uint8_t *orig_buffer;

  size_t message_byte_length= to_apply.ByteSize();
  size_t total_envelope_length= TransactionLog::HEADER_TRAILER_BYTES + message_byte_length;

  /* 
   * Attempt allocation of raw memory buffer for the header, 
   * message and trailing checksum bytes.
   */
  buffer= static_cast<uint8_t *>(malloc(total_envelope_length));
  if (buffer == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to allocate enough memory to buffer header, "
                    "transaction message, and trailing checksum bytes. Tried to allocate %" PRId64
                    " bytes.  Error: %s\n"), 
                  static_cast<int64_t>(total_envelope_length),
                  strerror(errno));
    return plugin::UNKNOWN_ERROR;
  }
  else
    orig_buffer= buffer; /* We will free() orig_buffer, as buffer is moved during write */

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
  buffer= to_apply.SerializeWithCachedSizesToArray(buffer);

  uint32_t checksum= 0;
  if (do_checksum)
  {
    checksum= drizzled::algorithm::crc32(
        reinterpret_cast<char *>(buffer) - message_byte_length, message_byte_length);
  }

  /* We always write in network byte order */
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(checksum, buffer);

  /* Ask the transaction log to write the entry and return where it wrote it */
  off_t written_to= transaction_log.writeEntry(orig_buffer, total_envelope_length);

  free(orig_buffer);

  /* Add an entry to the index describing what was just applied */
  transaction_log_index->addEntry(TransactionLogEntry(ReplicationServices::TRANSACTION,
                                                      written_to,
                                                      total_envelope_length),
                                  to_apply,
                                  checksum);
  return plugin::SUCCESS;
}
