#include <iostream>
#include <fstream>
#include <string>
#include "table.pb.h"
using namespace std;

/* 
  Written from Google proto example
*/

void updateTable(drizzle::Table *table, const char *name) 
{
  int x;

  table->set_name(name);
  table->set_engine("Innodb");

  /* Write out some random varchar */
  for (x= 0; x < 3; x++)
  {
    char buffer[1024];
    drizzle::Table::Field *field = table->add_field();

    sprintf(buffer, "sample%u", x);

    field->set_name(buffer);
    field->set_type(drizzle::Table::VARCHAR);
    if (x % 2)
      field->set_is_notnull(true);

    field->set_length(rand() % 100);

    if (x % 3)
    {
      field->set_collation("utf8_swedish_ci");
      field->set_characterset("utf8");
    }
  }

  /* Write out an INTEGER */
  {
    drizzle::Table::Field *field = table->add_field();
    field->set_name("number");
    field->set_type(drizzle::Table::INTEGER);
  }
  /* Write out a ENUM */
  {
    drizzle::Table::Field *field = table->add_field();
    field->set_type(drizzle::Table::ENUM);
    field->set_name("colors");
    field->add_values("red");
    field->add_values("blue");
    field->add_values("green");
  }
  /* Write out a BLOB */
  {
    drizzle::Table::Field *field = table->add_field();
    field->set_name("some_btye_string");
    field->set_type(drizzle::Table::BLOB);
  }
  /* Write out a DECIMAL */
  {
    drizzle::Table::Field *field = table->add_field();
    field->set_name("important_number");
    field->set_type(drizzle::Table::DECIMAL);
    field->set_length(8);
    field->set_scale(3);
  }
  /* Write out a VARCHAR with index */
  {
    drizzle::Table::Field *field = table->add_field();
    field->set_name("important_string");
    field->set_type(drizzle::Table::VARCHAR);
    field->set_length(20);
    field->set_unique(true);
  }

  {
    drizzle::Table::Index *index = table->add_index();
    index->set_name("number");
    index->set_primary(true);
  }

  for (x= 0; x < 2; x++)
  {
    char buffer[1024];
    drizzle::Table::Index *index = table->add_index();
    drizzle::Table::KeyPart *keypart = index->add_values();

    sprintf(buffer, "sample%u", x);
    index->set_name(buffer);

    keypart->set_name(buffer);
  }

}

int main(int argc, char* argv[]) 
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  drizzle::Table table;

  updateTable(&table, "example_table");

  fstream output(argv[1], ios::out | ios::trunc | ios::binary);
  if (!table.SerializeToOstream(&output)) 
  {
    cerr << "Failed to write schema." << endl;
    return -1;
  }

  return 0;
}
