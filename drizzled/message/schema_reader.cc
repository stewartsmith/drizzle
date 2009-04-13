#include <iostream>
#include <fstream>
#include <string>
#include <drizzled/message/schema.pb.h>
using namespace std;

/*
  Written from Google proto example
*/

void printSchema(const drizzled::message::Schema *schema)
{
  cout << "CREATE SCHEMA `" << schema->name() << "` ";
  if (schema->has_collation())
    cout << "COLLATE `" << schema->collation() << "` ";
  cout << ";" << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  drizzled::message::Schema schema;

  {
    // Read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!schema.ParseFromIstream(&input))
    {
      cerr << "Failed to parse schema." << endl;
      return -1;
    }
  }

  printSchema(&schema);

  return 0;
}
