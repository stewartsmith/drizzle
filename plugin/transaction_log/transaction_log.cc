/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * Defines the implementation of the default transaction log.
 *
 * @see drizzled/plugin/transaction_replicator.h
 * @see drizzled/plugin/transaction_applier.h
 *
 * @details
 *
 * Currently, the log file uses this implementation:
 *
 * We have an atomic off_t called log_offset which keeps track of the 
 * offset into the log file for writing the next Transaction.
 *
 * We write Transaction message encapsulated in an 8-byte length header and a
 * 4-byte checksum trailer.
 *
 * When writing a Transaction to the log, we calculate the length of the 
 * Transaction to be written.  We then increment log_offset by the length
 * of the Transaction plus 2 * sizeof(uint32_t) plus sizeof(uint32_t) and store 
 * this new offset in a local off_t called cur_offset (see TransactionLog::apply().  
 * This compare and set is done in an atomic instruction.
 *
 * We then adjust the local off_t (cur_offset) back to the original
 * offset by subtracting the length and sizeof(uint32_t) and sizeof(uint32_t).
 *
 * We then first write a 64-bit length and then the serialized transaction/transaction
 * and optional checksum to our log file at our local cur_offset.
 *
 * --------------------------------------------------------------------------------
 * |<- 4 bytes ->|<- 4 bytes ->|<- # Bytes of Transaction Message ->|<- 4 bytes ->|
 * --------------------------------------------------------------------------------
 * |  Msg Type   |   Length    |   Serialized Transaction Message   |   Checksum  |
 * --------------------------------------------------------------------------------
 *
 * @todo
 *
 * Possibly look at a scoreboard approach with multiple file segments.  For
 * right now, though, this is just a quick simple implementation to serve
 * as a skeleton and a springboard.
 */

#include <drizzled/server_includes.h>
#include "transaction_log.h"

#include <unistd.h>

#include <vector>
#include <string>

#include <drizzled/session.h>
#include <drizzled/set_var.h>
#include <drizzled/gettext.h>
#include <drizzled/hash/crc32.h>
#include <drizzled/message/transaction.pb.h>
#include <google/protobuf/io/coded_stream.h>

using namespace std;
using namespace drizzled;
using namespace google;

/** 
 * Transaction Log plugin system variable - Is the log enabled? Only used on init().  
 * The enable() and disable() methods of the TransactionLog class control online
 * disabling.
 */
static bool sysvar_transaction_log_enabled= false;
/** Transaction Log plugin system variable - The path to the log file used */
static char* sysvar_transaction_log_file= NULL;
/** 
 * Transaction Log plugin system variable - A debugging variable to assist 
 * in truncating the log file. 
 */
static bool sysvar_transaction_log_truncate_debug= false;
static const char DEFAULT_LOG_FILE_PATH[]= "transaction.log"; /* In datadir... */
/** 
 * Transaction Log plugin system variable - Should we write a CRC32 checksum for 
 * each written Transaction message?
 */
static bool sysvar_transaction_log_checksum_enabled= false;

TransactionLog::TransactionLog(string name_arg,
                               const char *in_log_file_path,
                               bool in_do_checksum)
  : plugin::TransactionApplier(name_arg),
    state(OFFLINE),
    log_file_path(in_log_file_path)
{
  do_checksum= in_do_checksum; /* Have to do here, not in initialization list b/c atomic<> */

  /* Setup our log file and determine the next write offset... */
  log_file= open(log_file_path, O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU);
  if (log_file == -1)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to open transaction log file %s.  Got error: %s\n"), 
                  log_file_path, 
                  strerror(errno));
    deactivate();
    return;
  }

  /* 
   * The offset of the next write is the current position of the log
   * file, since it's opened in append mode...
   */
  log_offset= lseek(log_file, 0, SEEK_END);

  state= ONLINE;
}

TransactionLog::~TransactionLog()
{
  /* Clear up any resources we've consumed */
  if (isEnabled() && log_file != -1)
  {
    (void) close(log_file);
  }
}

void TransactionLog::apply(const message::Transaction &to_apply)
{
  uint8_t *buffer; /* Buffer we will write serialized header, 
                      message and trailing checksum to */
  uint8_t *orig_buffer;

  int error_code;
  size_t message_byte_length= to_apply.ByteSize();
  ssize_t written;
  off_t cur_offset;
  size_t total_envelope_length= HEADER_TRAILER_BYTES + message_byte_length;

  /* 
   * Attempt allocation of raw memory buffer for the header, 
   * message and trailing checksum bytes.
   */
  buffer= static_cast<uint8_t *>(malloc(total_envelope_length));
  if (buffer == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to allocate enough memory to buffer header, transaction message, and trailing checksum bytes. Tried to allocate %" PRId64
                    " bytes.  Error: %s\n"), 
                  static_cast<int64_t>(total_envelope_length),
                  strerror(errno));
    state= CRASHED;
    deactivate();
    return;
  }
  else
    orig_buffer= buffer; /* We will free() orig_buffer, as buffer is moved during write */

  /*
   * Do an atomic increment on the offset of the log file position
   */
  cur_offset= log_offset.fetch_and_add(static_cast<off_t>(total_envelope_length));

  /*
   * We adjust cur_offset back to the original log_offset before
   * the increment above...
   */
  cur_offset-= static_cast<off_t>((total_envelope_length));

  /*
   * Write the header information, which is the message type and
   * the length of the transaction message into the buffer
   */
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(static_cast<uint32_t>(ReplicationServices::TRANSACTION), buffer);
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(static_cast<uint32_t>(message_byte_length), buffer);
  
  /*
   * Now write the serialized transaction message, followed
   * by the optional checksum into the buffer.
   */
  buffer= to_apply.SerializeWithCachedSizesToArray(buffer);

  uint32_t checksum= 0;
  if (do_checksum)
  {
    checksum= drizzled::hash::crc32(reinterpret_cast<char *>(buffer) - message_byte_length, message_byte_length);
  }

  /* We always write in network byte order */
  buffer= protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(checksum, buffer);

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
    free(orig_buffer);
    return;
  }

  /* Write the full buffer in one swoop */
  do
  {
    written= pwrite(log_file, orig_buffer, total_envelope_length, cur_offset);
  }
  while (written == -1 && errno == EINTR); /* Just retry the write when interrupted by a signal... */

  if (unlikely(written != static_cast<ssize_t>(total_envelope_length)))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to write full size of transaction.  Tried to write %" PRId64
                    " bytes at offset %" PRId64 ", but only wrote %" PRId32 " bytes.  Error: %s\n"), 
                  static_cast<int64_t>(total_envelope_length),
                  static_cast<int64_t>(cur_offset),
                  static_cast<int64_t>(written), 
                  strerror(errno));
    state= CRASHED;
    /* 
     * Reset the log's offset in case we want to produce a decent error message including
     * the original offset where an error occurred.
     */
    log_offset= cur_offset;
    deactivate();
  }
  free(orig_buffer);

  do
  {
    error_code= fdatasync(log_file);
  }
  while (error_code != 0 && errno == EINTR); /* Just retry the sync when interrupted by a signal... */

  if (unlikely(error_code != 0))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to sync log file. Got error: %s\n"), 
                  strerror(errno));
  }
}

void TransactionLog::truncate()
{
  bool orig_is_enabled= isEnabled();
  disable();
  
  /* 
   * Wait a short amount of time before truncating.  This just prevents error messages
   * from being produced during a call to apply().  Calling disable() above
   * means that once the current caller to apply() is done, no other calls are made to
   * apply() before enable is reset to its original state
   *
   * @note
   *
   * This is DEBUG code only!
   */
  usleep(500); /* Sleep for half a second */
  log_offset= (off_t) 0;
  int result;
  do
  {
    result= ftruncate(log_file, log_offset);
  }
  while (result == -1 && errno == EINTR);

  if (orig_is_enabled)
    enable();
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

static TransactionLog *transaction_log= NULL; /* The singleton transaction log */

static int init(drizzled::plugin::Registry &registry)
{
  if (sysvar_transaction_log_enabled)
  {
    transaction_log= new TransactionLog("transaction_log",
                                        sysvar_transaction_log_file, 
                                        sysvar_transaction_log_checksum_enabled);
    registry.add(transaction_log);
  }
  return 0;
}

static int deinit(drizzled::plugin::Registry &registry)
{
  if (transaction_log)
  {
    registry.remove(transaction_log);
    delete transaction_log;
  }
  return 0;
}

static void set_truncate_debug(Session *,
                               struct st_mysql_sys_var *, 
                               void *, 
                               const void *save)
{
  /* 
   * The const void * save comes directly from the check function, 
   * which should simply return the result from the set statement. 
   */
  if (transaction_log)
    if (*(bool *)save != false)
      transaction_log->truncate();
}

static DRIZZLE_SYSVAR_BOOL(enable,
                          sysvar_transaction_log_enabled,
                          PLUGIN_VAR_NOCMDARG,
                          N_("Enable transaction log"),
                          NULL, /* check func */
                          NULL, /* update func */
                          false /* default */);

static DRIZZLE_SYSVAR_BOOL(truncate_debug,
                          sysvar_transaction_log_truncate_debug,
                          PLUGIN_VAR_NOCMDARG,
                          N_("DEBUGGING - Truncate transaction log"),
                          NULL, /* check func */
                          set_truncate_debug, /* update func */
                          false /* default */);

static DRIZZLE_SYSVAR_STR(log_file,
                          sysvar_transaction_log_file,
                          PLUGIN_VAR_READONLY,
                          N_("Path to the file to use for transaction log."),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_LOG_FILE_PATH /* default */);

static DRIZZLE_SYSVAR_BOOL(enable_checksum,
                          sysvar_transaction_log_checksum_enabled,
                          PLUGIN_VAR_NOCMDARG,
                          N_("Enable CRC32 Checksumming"),
                          NULL, /* check func */
                          NULL, /* update func */
                          false /* default */);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(truncate_debug),
  DRIZZLE_SYSVAR(log_file),
  DRIZZLE_SYSVAR(enable_checksum),
  NULL
};

drizzle_declare_plugin(transaction_log)
{
  "transaction_log",
  "0.1",
  "Jay Pipes",
  N_("Transaction Message Log"),
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL, /* status variables */
  system_variables, /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
