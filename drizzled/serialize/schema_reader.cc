#include <iostream>
#include <fstream>
#include <string>
#include "schema.pb.h"
using namespace std;

/*
  Written from Google proto example
*/

void printSchema(const drizzle::Schema *schema)
{
  cout << "CREATE SCHEMA `" << schema->name() << "` ";
  if (schema->has_collation())
    cout << "COLLATE `" << schema->collation() << "` ";
  if (schema->has_characterset())
    cout << "CHARACTER SET `" << schema->characterset() <<"` ";
  cout << ";" << endl;
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  drizzle::Schema schema;

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
