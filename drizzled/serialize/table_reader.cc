#include <iostream>
#include <fstream>
#include <string>
#include "table.pb.h"
using namespace std;

/* 
  Written from Google proto example
*/

void printType(const drizzle::Table::Field *field) 
{
  switch (field->type())
  {
  case drizzle::Table::DOUBLE:
    cout << " DOUBLE ";
    break;
  case drizzle::Table::VARCHAR:
    cout << " VARCHAR(" << field->length() << ")";
    break;
  case drizzle::Table::TEXT:
    cout << " TEXT ";
    break;
  case drizzle::Table::BLOB:
    cout << " BLOB ";
    break;
  case drizzle::Table::ENUM:
    {
      int x;

      cout << " ENUM(";
      for (x= 0; x < field->values_size() ; x++)
      {
        const string type= field->values(x);

        if (x != 0)
          cout << ",";
        cout << "'" << type << "'";
      }
      cout << ") ";
      break;
    }
  case drizzle::Table::SET:
    cout << " SET ";
    break;
  case drizzle::Table::TINYINT:
    cout << " TINYINNT ";
    break;
  case drizzle::Table::SMALLINT:
    cout << " SMALLINT ";
    break;
  case drizzle::Table::INTEGER:
    cout << " INTEGER ";
    break;
  case drizzle::Table::BIGINT:
    cout << " BIGINT ";
    break;
  case drizzle::Table::DECIMAL:
    cout << " DECIMAL(" << field->length() << "," << field->scale() << ") ";
    break;
  case drizzle::Table::VARBINARY:
    cout << " VARBINARY(" << field->length() << ") ";
    break;
  case drizzle::Table::DATE:
    cout << " DATE ";
    break;
  case drizzle::Table::TIME:
    cout << " TIME ";
    break;
  case drizzle::Table::TIMESTAMP:
    cout << " TIMESTAMP ";
    break;
  case drizzle::Table::DATETIME:
    cout << " DATETIME ";
    break;
  }

  if (field->has_characterset())
    cout << " CHARACTER SET " << field->characterset();

  if (field->has_collation())
    cout << " COLLATE " << field->collation();

  if (field->is_notnull())
    cout << " NOT NULL ";

  if (field->has_default_value())
    cout << " DEFAULT `" << field->default_value() << "` " ;

  if (field->on_update())
    cout << " ON UPDATE CURRENT_TIMESTAMP";

  if (field->autoincrement())
    cout << " AUTOINCREMENT ";

  if (field->has_comment())
    cout << " COMMENT `" << field->comment() << "` ";
}

void printTable(const drizzle::Table *table) 
{
  uint32_t x;

  cout << "CREATE TABLE";

  if (table->temp())
    cout << " TEMPORARY";

  cout << " `" << table->name() << "` (" << endl;
  
  for (x= 0; x < table->field_size() ; x++)
  {
    const drizzle::Table::Field field = table->field(x);

    if (x != 0)
      cout << "," << endl;;

    cout << "\t`" << field.name() << "`";
    printType(&field);
  }

  for (x= 0; x < table->index_size() ; x++)
  {
    const drizzle::Table::Index index = table->index(x);

    cout << "," << endl;;

    cout << "\t";

    if (index.primary())
      cout << " PRIMARY KEY (`" << index.name() << "`)";
    else
    {
      int x;

      cout << " UNIQUE KEY `" << index.name() << "` (";
      for (x= 0; x < index.values_size() ; x++)
      {
        const drizzle::Table::KeyPart key= index.values(x);

        if (x != 0)
          cout << ",";
        cout << "`" << key.name() << "`";
      }
      cout << ")";
    }
  }
  cout << endl;

  cout << ") " << endl;
  if (table->has_collation())
    cout << " COLLATE = `" << table->collation() << "` " << endl;;
  if (table->has_characterset())
    cout << " CHARACTER SET = `" << table->characterset() << "` " << endl;;
  if (table->has_comment())
    cout << " COMMENT = `" << table->comment() << "` " << endl;;
  if (table->has_engine())
  if (table->has_data_directory())
    cout << " DATA DIRECTORY = `" << table->data_directory() << "` " << endl;;
  if (table->has_index_directory())
    cout << " INDEX DIRECTORY = `" << table->index_directory() << "`" << endl;
  cout << " ENGINE = " << table->engine() << ";"  << endl;
}

int main(int argc, char* argv[]) 
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  drizzle::Table table;

  {
    // Read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!table.ParseFromIstream(&input)) 
    {
      cerr << "Failed to parse table." << endl;
      return -1;
    }
  }

  printTable(&table);

  return 0;
}
