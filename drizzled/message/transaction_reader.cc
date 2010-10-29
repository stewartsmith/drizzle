/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <cerrno>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include "drizzled/definitions.h"
#include "drizzled/gettext.h"
#include "drizzled/replication_services.h"
#include "drizzled/algorithm/crc32.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/statement_transform.h"
#include "drizzled/message/transaction_manager.h"
#include "drizzled/util/convert.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <boost/program_options.hpp>

using namespace std;
using namespace google;
using namespace drizzled;

namespace po= boost::program_options;

static const char *replace_with_spaces= "\n\r";

static void printStatement(const message::Statement &statement)
{
  vector<string> sql_strings;

  message::transformStatementToSql(statement,
                                   sql_strings,
                                   message::DRIZZLE,
                                   true /* already in transaction */);

  for (vector<string>::iterator sql_string_iter= sql_strings.begin();
       sql_string_iter != sql_strings.end();
       ++sql_string_iter)
  {
    string &sql= *sql_string_iter;

    /* 
     * Replace \n and \r with spaces so that SQL statements 
     * are always on a single line 
     */
    {
      string::size_type found= sql.find_first_of(replace_with_spaces);
      while (found != string::npos)
      {
        sql[found]= ' ';
        found= sql.find_first_of(replace_with_spaces, found);
      }
    }

    /*
     * Embedded NUL characters are a pain in the ass.
     */
    {
      string::size_type found= sql.find_first_of('\0');
      while (found != string::npos)
      {
        sql[found]= '\\';
        sql.insert(found + 1, 1, '0');
        found= sql.find_first_of('\0', found);
      }
    }

    cout << sql << ';' << endl;
  }
}

static bool isEndStatement(const message::Statement &statement)
{
  switch (statement.type())
  {
    case (message::Statement::INSERT):
    {
      const message::InsertData &data= statement.insert_data();
      if (not data.end_segment())
        return false;
      break;
    }
    case (message::Statement::UPDATE):
    {
      const message::UpdateData &data= statement.update_data();
      if (not data.end_segment())
        return false;
      break;
    }
    case (message::Statement::DELETE):
    {
      const message::DeleteData &data= statement.delete_data();
      if (not data.end_segment())
        return false;
      break;
    }
    default:
      return true;
  }
  return true;
}

static bool isEndTransaction(const message::Transaction &transaction)
{
  const message::TransactionContext trx= transaction.transaction_context();

  size_t num_statements= transaction.statement_size();

  /*
   * If any Statement is partial, then we can expect another Transaction
   * message.
   */
  for (size_t x= 0; x < num_statements; ++x)
  {
    const message::Statement &statement= transaction.statement(x);

    if (not isEndStatement(statement))
      return false;
  }

  return true;
}

static void printEvent(const message::Event &event)
{
  switch (event.type())
  {
    case message::Event::STARTUP:
    {
      cout << "-- EVENT: Server startup\n";
      break;
    }
    case message::Event::SHUTDOWN:
    {
      cout << "-- EVENT: Server shutdown\n";
      break;
    }
    default:
    {
      cout << "-- EVENT: Unknown event\n";
      break;
    }
  }
}

static void printTransaction(const message::Transaction &transaction,
                             bool ignore_events,
                             bool print_as_raw)
{
  static uint64_t last_trx_id= 0;
  bool should_commit= true;
  const message::TransactionContext trx= transaction.transaction_context();

  /*
   * First check to see if this is an event message.
   */
  if (transaction.has_event())
  {
    last_trx_id= trx.transaction_id();
    if (not ignore_events)
    {
      if (print_as_raw)
        transaction.PrintDebugString();
      else
        printEvent(transaction.event());
    }
    return;
  }

  if (print_as_raw)
  {
    transaction.PrintDebugString();
    return;
  }

  size_t num_statements= transaction.statement_size();
  size_t x;

  /*
   * One way to determine when a new transaction begins is when the
   * transaction id changes (if all transactions have their GPB messages
   * grouped together, which this program will). We check that here.
   */
  if (trx.transaction_id() != last_trx_id)
    cout << "START TRANSACTION;" << endl;

  last_trx_id= trx.transaction_id();

  for (x= 0; x < num_statements; ++x)
  {
    const message::Statement &statement= transaction.statement(x);

    if (should_commit)
      should_commit= isEndStatement(statement);

    /* A ROLLBACK would be the only Statement within the Transaction
     * since all other Statements will have been deleted from the
     * Transaction message, so we should fall out of this loop immediately.
     * We don't want to issue an unnecessary COMMIT, so we change
     * should_commit to false here.
     */
    if (statement.type() == message::Statement::ROLLBACK)
      should_commit= false;

    printStatement(statement);
  }

  /*
   * If ALL Statements are end segments, we can commit this Transaction.
   * We can also check to see if the transaction_id changed, but this
   * wouldn't work for the last Transaction in the transaction log since
   * we don't have another Transaction to compare to. Checking for all
   * end segments (like we do above) covers this case.
   */
  if (should_commit)
    cout << "COMMIT;" << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int opt_start_pos= 0;
  uint64_t opt_transaction_id= 0;

  /*
   * Setup program options
   */
  po::options_description desc("Program options");
  desc.add_options()
    ("help", N_("Display help and exit"))
    ("checksum", N_("Perform checksum"))
    ("ignore-events", N_("Ignore event messages"))
    ("input-file", po::value< vector<string> >(), N_("Transaction log file"))
    ("raw", N_("Print raw Protobuf messages instead of SQL"))
    ("start-pos",
      po::value<int>(&opt_start_pos),
      N_("Start reading from the given file position"))
    ("transaction-id",
      po::value<uint64_t>(&opt_transaction_id),
      N_("Only output for the given transaction ID"));

  /*
   * We allow one positional argument that will be transaction file name
   */
  po::positional_options_description pos;
  pos.add("input-file", 1);

  /*
   * Parse the program options
   */
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(pos).run(), vm);
  po::notify(vm);

  /*
   * If the help option was given, or not input file was supplied,
   * print out usage information.
   */
  if (vm.count("help") || not vm.count("input-file"))
  {
    cerr << desc << endl;
    return -1;
  }

  /*
   * Specifying both a transaction ID and a start position
   * is not logical.
   */
  if (vm.count("start-pos") && vm.count("transaction-id"))
  {
    cerr << _("Cannot use --start-pos and --transaction-id together\n");
    return -1;
  }

  bool do_checksum= vm.count("checksum") ? true : false;
  bool ignore_events= vm.count("ignore-events") ? true : false;
  bool print_as_raw= vm.count("raw") ? true : false;

  string filename= vm["input-file"].as< vector<string> >()[0];
  int file= open(filename.c_str(), O_RDONLY);
  if (file == -1)
  {
    cerr << _("Cannot open file: ") << filename << endl;
    return -1;
  }

  message::Transaction transaction;
  message::TransactionManager trx_mgr;

  protobuf::io::ZeroCopyInputStream *raw_input= new protobuf::io::FileInputStream(file);
  protobuf::io::CodedInputStream *coded_input= new protobuf::io::CodedInputStream(raw_input);

  /* Skip ahead to user supplied position */
  if (opt_start_pos)
  {
    if (not coded_input->Skip(opt_start_pos))
    {
      cerr << _("Could not skip to position ") << opt_start_pos
           << _(" in file ") << filename << endl;
      exit(-1);
    }
  }

  char *buffer= NULL;
  char *temp_buffer= NULL;
  uint32_t length= 0;
  uint32_t previous_length= 0;
  uint32_t checksum= 0;
  bool result= true;
  uint32_t message_type= 0;

  /* Read in the length of the command */
  while (result == true && 
         coded_input->ReadLittleEndian32(&message_type) == true &&
         coded_input->ReadLittleEndian32(&length) == true)
  {
    if (message_type != ReplicationServices::TRANSACTION)
    {
      cerr << _("Found a non-transaction message in log.  Currently, not supported.\n");
      exit(-1);
    }

    if (length > INT_MAX)
    {
      cerr << _("Attempted to read record bigger than INT_MAX\n");
      exit(-1);
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
      cerr << _("Memory allocation failure trying to allocate ") << length << _(" bytes\n");
      break;
    }
    else
      buffer= temp_buffer;

    /* Read the Command */
    result= coded_input->ReadRaw(buffer, (int) length);
    if (result == false)
    {
      char errmsg[STRERROR_MAX];
      strerror_r(errno, errmsg, sizeof(errmsg));
      cerr << _("Could not read transaction message.\n");
      cerr << _("GPB ERROR: ") << errmsg << endl;;
      string hexdump;
      hexdump.reserve(length * 4);
      bytesToHexdumpFormat(hexdump, reinterpret_cast<const unsigned char *>(buffer), length);
      cerr << _("HEXDUMP:\n\n") << hexdump << endl;
      break;
    }

    result= transaction.ParseFromArray(buffer, static_cast<int32_t>(length));
    if (result == false)
    {
      cerr << _("Unable to parse command. Got error: ")
           << transaction.InitializationErrorString() << endl;
      if (buffer != NULL)
      {
        string hexdump;
        hexdump.reserve(length * 4);
        bytesToHexdumpFormat(hexdump, reinterpret_cast<const unsigned char *>(buffer), length);
        cerr <<  _("HEXDUMP:\n\n") << hexdump << endl;
      }
      break;
    }

    const message::TransactionContext trx= transaction.transaction_context();
    uint64_t transaction_id= trx.transaction_id();

    /*
     * If we are given a transaction ID, we only look for that one and
     * print it out.
     */
    if (vm.count("transaction-id"))
    {
      if (opt_transaction_id == transaction_id)
      {
        printTransaction(transaction, ignore_events, print_as_raw);
      }
      else
      {
        /* Need to get the checksum bytes out of stream */
        coded_input->ReadLittleEndian32(&checksum);
        previous_length = length;
        continue;
      }
    }

    /*
     * No transaction ID given, so process all messages.
     */
    else
    {
      if (not isEndTransaction(transaction))
      {
        trx_mgr.store(transaction);
      }
      else
      {
        /*
         * If there are any previous Transaction messages for this transaction,
         * store this one, then output all of them together.
         */
        if (trx_mgr.contains(transaction_id))
        {
          trx_mgr.store(transaction);

          uint32_t size= trx_mgr.getTransactionBufferSize(transaction_id);
          uint32_t idx= 0;

          while (idx != size)
          {
            message::Transaction new_trx;
            trx_mgr.getTransactionMessage(new_trx, transaction_id, idx);
            printTransaction(new_trx, ignore_events, print_as_raw);
            idx++;
          }

          /* No longer need this transaction */
          trx_mgr.remove(transaction_id);
        }
        else
        {
          printTransaction(transaction, ignore_events, print_as_raw);
        }
      }
    } /* end ! vm.count("transaction-id") */

    /* Skip 4 byte checksum */
    coded_input->ReadLittleEndian32(&checksum);

    if (do_checksum)
    {
      if (checksum != drizzled::algorithm::crc32(buffer, static_cast<size_t>(length)))
      {
        cerr << _("Checksum failed. Wanted ")
             << checksum
             << _(" got ")
             << drizzled::algorithm::crc32(buffer, static_cast<size_t>(length))
             << endl;
      }
    }

    previous_length= length;
  } /* end while */

  if (buffer)
    free(buffer);

  delete coded_input;
  delete raw_input;

  return (result == true ? 0 : 1);
}

