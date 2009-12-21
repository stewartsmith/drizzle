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
 *
 * @todo
 *
 * Move the Applier piece of this code out into its own source file and leave
 * this for all the glue code of the module.
 */

#include "config.h"
#include "transaction_log.h"
#include "transaction_log_index.h"
#include "info_schema.h"
#include "print_transaction_message.h"
#include "hexdump_transaction_message.h"
#include "background_worker.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <vector>
#include <string>

#include <mysys/my_sys.h> /* for my_sync */

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
/**
 * Numeric option controlling the sync/flush behaviour of the transaction
 * log.  Options are:
 *
 * TransactionLog::SYNC_METHOD_OS == 0            ... let OS do sync'ing
 * TransactionLog::SYNC_METHOD_EVERY_WRITE == 1   ... sync on every write
 * TransactionLog::SYNC_METHOD_EVERY_SECOND == 2  ... sync at most once a second
 */
static uint32_t sysvar_transaction_log_sync_method= 0;

/** Views defined in info_schema.cc */
extern plugin::InfoSchemaTable *transaction_log_view;
extern plugin::InfoSchemaTable *transaction_log_entries_view;
extern plugin::InfoSchemaTable *transaction_log_transactions_view;

/** Index defined in transaction_log_index.cc */
extern TransactionLogIndex *transaction_log_index;

/** Defined in print_transaction_message.cc */
extern plugin::Create_function<PrintTransactionMessageFunction> *print_transaction_message_func_factory;
extern plugin::Create_function<HexdumpTransactionMessageFunction> *hexdump_transaction_message_func_factory;

TransactionLog::TransactionLog(string name_arg,
                               const string &in_log_file_path,
                               bool in_do_checksum)
  : plugin::TransactionApplier(name_arg),
    state(OFFLINE),
    log_file_path(in_log_file_path),
    has_error(false),
    error_message()
{
  do_checksum= in_do_checksum; /* Have to do here, not in initialization list b/c atomic<> */

  /* Setup our log file and determine the next write offset... */
  log_file= open(log_file_path.c_str(), O_APPEND|O_CREAT|O_SYNC|O_WRONLY, S_IRWXU);
  if (log_file == -1)
  {
    error_message.assign(_("Failed to open transaction log file "));
    error_message.append(log_file_path);
    error_message.append("  Got error: ");
    error_message.append(strerror(errno));
    error_message.push_back('\n');
    has_error= true;
    deactivate();
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

  int error_code= syncLogFile();

  transaction_log_index->addEntry(TransactionLogEntry(ReplicationServices::TRANSACTION,
                                                     cur_offset,
                                                     total_envelope_length),
                                  to_apply,
                                  checksum);

  if (unlikely(error_code != 0))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Failed to sync log file. Got error: %s\n"), 
                  strerror(errno));
  }
}

int TransactionLog::syncLogFile()
{
  switch (sysvar_transaction_log_sync_method)
  {
  case SYNC_METHOD_EVERY_WRITE:
    return my_sync(log_file, 0);
  case SYNC_METHOD_EVERY_SECOND:
    {
      time_t now_time= time(NULL);
      if (last_sync_time <= (now_time - 1))
      {
        last_sync_time= now_time;
        return my_sync(log_file, 0);
      }
      return 0;
    }
  case SYNC_METHOD_OS:
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

bool TransactionLog::hasError() const
{
  return has_error;
}

void TransactionLog::clearError()
{
  has_error= false;
  error_message.clear();
}

const std::string &TransactionLog::getErrorMessage() const
{
  return error_message;
}

TransactionLog *transaction_log= NULL; /* The singleton transaction log */

static int init(drizzled::plugin::Registry &registry)
{
  /* Create and initialize the transaction log itself */
  if (sysvar_transaction_log_enabled)
  {
    transaction_log= new (nothrow) TransactionLog("transaction_log_applier",
                                                  string(sysvar_transaction_log_file), 
                                                  sysvar_transaction_log_checksum_enabled);

    if (transaction_log == NULL)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the TransactionLog instance.  Got error: %s\n"), 
                    strerror(errno));
      return 1;
    }
    else
    {
      /* Check to see if the log was not created properly */
      if (transaction_log->hasError())
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to initialize the Transaction Log.  Got error: %s\n"), 
                      transaction_log->getErrorMessage().c_str());
        return 1;
      }
    }
    registry.add(transaction_log);

    /* Setup the INFORMATION_SCHEMA views for the transaction log */
    if (initViewMethods() ||
        initViewColumns() || 
        initViews())
      return 1; /* Error message output handled in functions above */

    registry.add(transaction_log_view);
    registry.add(transaction_log_entries_view);
    registry.add(transaction_log_transactions_view);

    /* Setup the module's UDFs */
    print_transaction_message_func_factory=
      new plugin::Create_function<PrintTransactionMessageFunction>("print_transaction_message");
    registry.add(print_transaction_message_func_factory);

    hexdump_transaction_message_func_factory=
      new plugin::Create_function<HexdumpTransactionMessageFunction>("hexdump_transaction_message");
    registry.add(hexdump_transaction_message_func_factory);

    /* Create and initialize the transaction log index */
    transaction_log_index= new (nothrow) TransactionLogIndex(*transaction_log);
    if (transaction_log_index == NULL)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the TransactionLogIndex instance.  Got error: %s\n"), 
                    strerror(errno));
      return 1;
    }
    else
    {
      /* Check to see if the index was not created properly */
      if (transaction_log_index->hasError())
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to initialize the Transaction Log Index.  Got error: %s\n"), 
                      transaction_log_index->getErrorMessage().c_str());
        return 1;
      }
    }

    /* 
     * Setup the background worker thread which maintains
     * summary information about the transaction log.
     */
    if (initTransactionLogBackgroundWorker())
      return 1; /* Error message output handled in function above */
  }
  return 0;
}

static int deinit(drizzled::plugin::Registry &registry)
{
  /* Cleanup the transaction log itself */
  if (transaction_log)
  {
    registry.remove(transaction_log);
    delete transaction_log;
    delete transaction_log_index;

    /* Cleanup the INFORMATION_SCHEMA views */
    registry.remove(transaction_log_view);
    registry.remove(transaction_log_entries_view);
    registry.remove(transaction_log_transactions_view);

    cleanupViewMethods();
    cleanupViewColumns();
    cleanupViews();

    /* Cleanup module UDFs */
    registry.remove(print_transaction_message_func_factory);
    delete print_transaction_message_func_factory;
    registry.remove(hexdump_transaction_message_func_factory);
    delete hexdump_transaction_message_func_factory;
  }

  return 0;
}

static void set_truncate_debug(Session *,
                               drizzle_sys_var *, 
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
                          N_("Path to the file to use for transaction log"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_LOG_FILE_PATH /* default */);

static DRIZZLE_SYSVAR_BOOL(enable_checksum,
                           sysvar_transaction_log_checksum_enabled,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable CRC32 Checksumming of each written transaction log entry"),
                           NULL, /* check func */
                           NULL, /* update func */
                           false /* default */);

static DRIZZLE_SYSVAR_UINT(sync_method,
                           sysvar_transaction_log_sync_method,
                           PLUGIN_VAR_OPCMDARG,
                           N_("0 == rely on operating system to sync log file (default), "
                              "1 == sync file at each transaction write, "
                              "2 == sync log file once per second"),
                           NULL, /* check func */
                           NULL, /* update func */
                           0, /* default */
                           0,
                           2,
                           0);

static drizzle_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(truncate_debug),
  DRIZZLE_SYSVAR(log_file),
  DRIZZLE_SYSVAR(enable_checksum),
  DRIZZLE_SYSVAR(sync_method),
  NULL
};

DRIZZLE_PLUGIN(init, deinit, NULL, system_variables);
