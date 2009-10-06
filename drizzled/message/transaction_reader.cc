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

#include <drizzled/global.h>
#include <drizzled/gettext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/message/statement_transform.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace std;
using namespace drizzled;
using namespace google;

static void printStatement(const message::Statement &statement)
{
  cout << "/* Start Timestamp: " << statement.start_timestamp() << " ";
  cout << " End Timestamp: " << statement.end_timestamp() << " */" << endl;

  vector<string> sql_strings;

  message::transformStatementToSql(statement, sql_strings, message::DRIZZLE);

  vector<string>::iterator sql_string_iter= sql_strings.begin();
  const std::string newline= "\n";
  while (sql_string_iter != sql_strings.end())
  {
    string &sql= *sql_string_iter;
    /* 
     * Replace \n with spaces so that SQL statements 
     * are always on a single line 
     */
    while (sql.find(newline) != std::string::npos)
      sql.replace(sql.find(newline), 1, " ");

    cout << sql << ';' << endl;
    ++sql_string_iter;
  }
}

static void printTransaction(const message::Transaction &transaction)
{
  const message::TransactionContext trx= transaction.transaction_context();

  cout << "/* SERVER ID: " << trx.server_id() << " TRX ID: " << trx.transaction_id() << " */ " << endl;

  size_t num_statements= transaction.statement_size();
  size_t x;

  for (x= 0; x < num_statements; ++x)
  {
    const message::Statement &statement= transaction.statement(x);
    printStatement(statement);
  }
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  int file;

  if (argc != 2)
  {
    fprintf(stderr, _("Usage: %s TRANSACTION_LOG\n"), argv[0]);
    return -1;
  }

  message::Transaction transaction;

  file= open(argv[1], O_RDONLY);
  if (file == -1)
  {
    fprintf(stderr, _("Cannot open file: %s\n"), argv[1]);
    return -1;
  }

  protobuf::io::ZeroCopyInputStream *raw_input= new protobuf::io::FileInputStream(file);
  protobuf::io::CodedInputStream *coded_input= new protobuf::io::CodedInputStream(raw_input);

  char *buffer= NULL;
  char *temp_buffer= NULL;
  uint64_t length= 0;
  uint64_t previous_length= 0;
  bool result= true;

  /* Read in the length of the command */
  while (result == true && coded_input->ReadLittleEndian64(&length) == true)
  {
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
      fprintf(stderr, _("Could not read transaction message.\n"));
      fprintf(stderr, _("GPB ERROR: %s.\n"), strerror(errno));
      fprintf(stderr, _("Raw buffer read: %s.\n"), buffer);
      break;
    }

    result= transaction.ParseFromArray(buffer, static_cast<size_t>(length));
    if (result == false)
    {
      fprintf(stderr, _("Unable to parse command. Got error: %s.\n"), transaction.InitializationErrorString().c_str());
      if (buffer != NULL)
        fprintf(stderr, _("BUFFER: %s\n"), buffer);
      break;
    }

    /* Print the transaction */
    printTransaction(transaction);

    previous_length= length;
  }
  if (buffer)
    free(buffer);
  
  delete coded_input;
  delete raw_input;

  return (result == true ? 0 : 1);
}
