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
 * Transaction log module initialization and plugin
 * registration.
 */

#include "config.h"

#include "transaction_log.h"
#include "transaction_log_applier.h"
#include "transaction_log_index.h"
#include "data_dictionary_schema.h"
#include "print_transaction_message.h"
#include "hexdump_transaction_message.h"
#include "background_worker.h"

#include <errno.h>

#include <drizzled/plugin/plugin.h>
#include <drizzled/session.h>
#include <drizzled/gettext.h>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

/**
 * The name of the main transaction log file on disk.  With no prefix,
 * this goes into Drizzle's $datadir.
 */
static const char DEFAULT_LOG_FILE_PATH[]= "transaction.log"; /* In datadir... */
/** 
 * Transaction Log plugin system variable - Is the log enabled? Only used on init().  
 */
static bool sysvar_transaction_log_enabled= false;

/** Transaction Log plugin system variable - The path to the log file used */
static string sysvar_transaction_log_file;

/** 
 * Transaction Log plugin system variable - A debugging variable to assist 
 * in truncating the log file. 
 */
static bool sysvar_transaction_log_truncate_debug= false;
/** 
 * Transaction Log plugin system variable - Should we write a CRC32 checksum for 
 * each written Transaction message?
 */
static bool sysvar_transaction_log_checksum_enabled= false;
/**
 * Numeric option controlling the sync/flush behaviour of the transaction
 * log.  Options are:
 *
 * TransactionLog::FLUSH_FREQUENCY_OS == 0            ... let OS do sync'ing
 * TransactionLog::FLUSH_FREQUENCY_EVERY_WRITE == 1   ... sync on every write
 * TransactionLog::FLUSH_FREQUENCY_EVERY_SECOND == 2  ... sync at most once a second
 */
typedef constrained_check<int, 2, 0> flush_constraint;
static flush_constraint sysvar_transaction_log_flush_frequency;
/**
 * Transaction Log plugin system variable - Number of slots to create
 * for managing write buffers
 */
typedef constrained_check<uint32_t, 8192, 4> write_buffers_constraint;
static write_buffers_constraint sysvar_transaction_log_num_write_buffers;
/**
 * Transaction Log plugin system variable - The name of the replicator plugin
 * to pair the transaction log's applier with.  Defaults to "default"
 */
static const char DEFAULT_USE_REPLICATOR[]= "default";
static string sysvar_transaction_log_use_replicator;

/** DATA_DICTIONARY views */
static TransactionLogTool *transaction_log_tool;
static TransactionLogEntriesTool *transaction_log_entries_tool;
static TransactionLogTransactionsTool *transaction_log_transactions_tool;

/** Index defined in transaction_log_index.cc */
extern TransactionLogIndex *transaction_log_index;
/** Transaction Log descriptor defined in transaction_log.cc */
extern TransactionLog *transaction_log;
/** Transaction Log descriptor defined in transaction_log.cc */
extern TransactionLogApplier *transaction_log_applier;

/** Defined in print_transaction_message.cc */
extern plugin::Create_function<PrintTransactionMessageFunction> *print_transaction_message_func_factory;
extern plugin::Create_function<HexdumpTransactionMessageFunction> *hexdump_transaction_message_func_factory;

TransactionLog::~TransactionLog()
{
  /* Clear up any resources we've consumed */
  if (log_file != -1)
  {
    (void) close(log_file);
  }
}

static void set_truncate_debug(Session *, sql_var_t)
{
  if (transaction_log)
  {
    if (sysvar_transaction_log_truncate_debug)
    {
      transaction_log->truncate();
      transaction_log_index->clear();
      sysvar_transaction_log_truncate_debug= false;
    }
  }
}

static int init(drizzled::module::Context &context)
{
  context.registerVariable(new sys_var_bool_ptr_readonly("enable",
                                                         &sysvar_transaction_log_enabled));
  context.registerVariable(new sys_var_bool_ptr("truncate-debug",
                                                &sysvar_transaction_log_truncate_debug,
                                                set_truncate_debug));

  context.registerVariable(new sys_var_const_string("file",
                                                    sysvar_transaction_log_file));
  context.registerVariable(new sys_var_const_string("use-replicator",
                                                    sysvar_transaction_log_use_replicator));
  context.registerVariable(new sys_var_bool_ptr_readonly("enable-checksum",
                                                         &sysvar_transaction_log_checksum_enabled));
  context.registerVariable(new sys_var_constrained_value_readonly<int>("flush-frequency", sysvar_transaction_log_flush_frequency));

  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("num-write-buffers",
                                                                            sysvar_transaction_log_num_write_buffers));


  /* Create and initialize the transaction log itself */
  if (sysvar_transaction_log_enabled)
  {
  
    transaction_log= new (nothrow) TransactionLog(sysvar_transaction_log_file,
                                                  static_cast<int>(sysvar_transaction_log_flush_frequency),
                                                  sysvar_transaction_log_checksum_enabled);

    if (transaction_log == NULL)
    {
      char errmsg[STRERROR_MAX];
      strerror_r(errno, errmsg, sizeof(errmsg));
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the TransactionLog instance.  Got error: %s\n"), 
                    errmsg);
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

    /* Create and initialize the transaction log index */
    transaction_log_index= new (nothrow) TransactionLogIndex(*transaction_log);
    if (transaction_log_index == NULL)
    {
      char errmsg[STRERROR_MAX];
      strerror_r(errno, errmsg, sizeof(errmsg));
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the TransactionLogIndex instance.  Got error: %s\n"), 
                    errmsg);
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

    /* Create the applier plugin and register it */
    transaction_log_applier= new (nothrow) TransactionLogApplier("transaction_log_applier",
                                                                 transaction_log, 
                                                                 transaction_log_index, 
                                                                 static_cast<uint32_t>(sysvar_transaction_log_num_write_buffers));
    if (transaction_log_applier == NULL)
    {
      char errmsg[STRERROR_MAX];
      strerror_r(errno, errmsg, sizeof(errmsg));
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the TransactionLogApplier instance.  Got error: %s\n"), 
                    errmsg);
      return 1;
    }
    context.add(transaction_log_applier);
    ReplicationServices &replication_services= ReplicationServices::singleton();
    replication_services.attachApplier(transaction_log_applier,
                                       sysvar_transaction_log_use_replicator);

    /* Setup DATA_DICTIONARY views */

    transaction_log_tool= new (nothrow) TransactionLogTool;
    context.add(transaction_log_tool);
    transaction_log_entries_tool= new (nothrow) TransactionLogEntriesTool;
    context.add(transaction_log_entries_tool);
    transaction_log_transactions_tool= new (nothrow) TransactionLogTransactionsTool;
    context.add(transaction_log_transactions_tool);

    /* Setup the module's UDFs */
    print_transaction_message_func_factory=
      new plugin::Create_function<PrintTransactionMessageFunction>("print_transaction_message");
    context.add(print_transaction_message_func_factory);

    hexdump_transaction_message_func_factory=
      new plugin::Create_function<HexdumpTransactionMessageFunction>("hexdump_transaction_message");
    context.add(hexdump_transaction_message_func_factory);

    /* 
     * Setup the background worker thread which maintains
     * summary information about the transaction log.
     */
    if (initTransactionLogBackgroundWorker())
      return 1; /* Error message output handled in function above */
  }
  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("truncate-debug",
          po::value<bool>(&sysvar_transaction_log_truncate_debug)->default_value(false)->zero_tokens(),
          N_("DEBUGGING - Truncate transaction log"));
  context("enable-checksum",
          po::value<bool>(&sysvar_transaction_log_checksum_enabled)->default_value(false)->zero_tokens(),
          N_("Enable CRC32 Checksumming of each written transaction log entry"));  
  context("enable",
          po::value<bool>(&sysvar_transaction_log_enabled)->default_value(false)->zero_tokens(),
          N_("Enable transaction log"));
  context("file",
          po::value<string>(&sysvar_transaction_log_file)->default_value(DEFAULT_LOG_FILE_PATH),
          N_("Path to the file to use for transaction log"));
  context("use-replicator",
          po::value<string>(&sysvar_transaction_log_use_replicator)->default_value(DEFAULT_USE_REPLICATOR),
          N_("Name of the replicator plugin to use (default='default_replicator')")); 
  context("flush-frequency",
          po::value<flush_constraint>(&sysvar_transaction_log_flush_frequency)->default_value(0),
          N_("0 == rely on operating system to sync log file (default), 1 == sync file at each transaction write, 2 == sync log file once per second"));
  context("num-write-buffers",
          po::value<write_buffers_constraint>(&sysvar_transaction_log_num_write_buffers)->default_value(8),
          N_("Number of slots for in-memory write buffers (default=8)."));
}

DRIZZLE_PLUGIN(init, NULL, init_options);
