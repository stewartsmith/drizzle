#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <drizzled/message/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;
using namespace google;


static void print_table(const message::Table &table)
{
  string proto_as_text("");

  protobuf::TextFormat::PrintToString(table, &proto_as_text);

  cout << proto_as_text;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  message::Table table;

  {
    int fd= open(argv[1], O_RDONLY);

    if(fd==-1)
    {
      perror("Failed to open table definition file");
      return -1;
    }

    protobuf::io::ZeroCopyInputStream* input=
      new protobuf::io::FileInputStream(fd);

    if (!table.ParseFromZeroCopyStream(input))
    {
      cerr << "Failed to parse table." << endl;
      close(fd);
      return -1;
    }

    close(fd);
  }

  print_table(table);

  return 0;
}
