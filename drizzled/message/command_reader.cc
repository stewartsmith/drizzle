#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <drizzled/message/replication.pb.h>

#include "drizzled/message/command_transform.h"

#include "drizzled/korr.h"

using namespace std;
using namespace drizzled;

static void printCommand(const message::Command &command)
{
  cout << "/* Timestamp: " << command.timestamp() << " */"<< endl;

  message::TransactionContext trx= command.transaction_context();

  cout << "/* SERVER ID: " << trx.server_id() << " TRX ID: " << trx.transaction_id();
  
  if (command.has_session_id())
    cout << " SESSION ID: " << command.session_id();

  cout << " */ ";

  string sql("");

  message::transformCommand2Sql(command, &sql, message::DRIZZLE);

  /* 
   * Replace \n with spaces so that SQL statements 
   * are always on a single line 
   */
  const std::string newline= "\n";
  while (sql.find(newline) != std::string::npos)
    sql.replace(sql.find(newline), 1, " ");

  cout << sql << ';' << endl;
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
