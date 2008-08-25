#include <iostream>
#include <fstream>
#include <string>
#include "table.pb.h"
using namespace std;

/* 
  Written from Google proto example
*/

void print_field(const drizzle::Table::Field *field) 
{
  using namespace drizzle;
  cout << "\t`" << field->name() << "`";
  switch (field->type())
  {
    case Table::Field::DOUBLE:
    cout << " DOUBLE ";
    break;
  case Table::Field::VARCHAR:
    cout << " VARCHAR(" << field->string_options().length() << ")";
    break;
  case Table::Field::TEXT:
    cout << " TEXT ";
    break;
  case Table::Field::BLOB:
    cout << " BLOB ";
    break;
  case Table::Field::ENUM:
    {
      int x;

      cout << " ENUM(";
      for (x= 0; x < field->set_options().value_size() ; x++)
      {
        const string type= field->set_options().value(x);

        if (x != 0)
          cout << ",";
        cout << "'" << type << "'";
      }
      cout << ") ";
      break;
    }
  case Table::Field::SET:
    cout << " SET ";
    break;
  case Table::Field::TINYINT:
    cout << " TINYINT ";
    break;
  case Table::Field::SMALLINT:
    cout << " SMALLINT ";
    break;
  case Table::Field::INTEGER:
    cout << " INTEGER ";
    break;
  case Table::Field::BIGINT:
    cout << " BIGINT ";
    break;
  case Table::Field::DECIMAL:
    cout << " DECIMAL(" << field->numeric_options().precision() << "," << field->numeric_options().scale() << ") ";
    break;
  case Table::Field::VARBINARY:
    cout << " VARBINARY(" << field->string_options().length() << ") ";
    break;
  case Table::Field::DATE:
    cout << " DATE ";
    break;
  case Table::Field::TIME:
    cout << " TIME ";
    break;
  case Table::Field::TIMESTAMP:
    cout << " TIMESTAMP ";
    break;
  case Table::Field::DATETIME:
    cout << " DATETIME ";
    break;
  }

  if (field->type() == Table::Field::INTEGER
      || field->type() == Table::Field::BIGINT
      || field->type() == Table::Field::SMALLINT
      || field->type() == Table::Field::TINYINT) {
    if (field->has_constraints()
        && field->constraints().has_is_unsigned())
      if (field->constraints().is_unsigned())
        cout << " UNSIGNED";

    if (field->has_numeric_options() &&
      field->numeric_options().is_autoincrement())
      cout << " AUTOINCREMENT ";
  }

  if (! field->has_constraints()
      && field->constraints().is_nullable())
    cout << " NOT NULL ";

  if (field->type() == Table::Field::TEXT
      || field->type() == Table::Field::VARCHAR) {
    if (field->string_options().has_charset())
      cout << " CHARACTER SET " << field->string_options().charset();

    if (field->string_options().has_collation())
      cout << " COLLATE " << field->string_options().collation();
  }

  if (field->options().has_default_value())
    cout << " DEFAULT `" << field->options().default_value() << "` " ;

  if (field->type() == Table::Field::TIMESTAMP)
    if (field->timestamp_options().has_auto_updates()
      && field->timestamp_options().auto_updates())
      cout << " ON UPDATE CURRENT_TIMESTAMP";

  if (field->has_comment())
    cout << " COMMENT `" << field->comment() << "` ";
}

void print_engine(const drizzle::Table::StorageEngine *engine) {
  using namespace drizzle;
  uint32_t x;

  cout << " ENGINE = " << engine->name()  << endl;

  for (x= 0; x < engine->option_size(); ++x) {
    const Table::StorageEngine::EngineOption option= engine->option(x);
    cout << "\t" << option.name() << " = " << option.value() << endl;
  }
}

void print_index(const drizzle::Table::Index *index) {
  using namespace drizzle;
  uint32_t x;

  if (index->is_primary())
    cout << " PRIMARY"; 
  else if (index->is_unique())
    cout << " UNIQUE"; 
  cout << " KEY `" << index->name() << "` (";
  {
    int x;

    for (x= 0; x < index->index_part_size() ; x++)
    {
      const Table::Index::IndexPart part= index->index_part(x);

      if (x != 0)
        cout << ",";
      cout << "`" << part.field().name() << "`";
      if (part.has_compare_length())
        cout << "(" << part.compare_length() << ")";
    }
    cout << ")";
  }
  cout << "\t";
}

void print_table(const drizzle::Table *table) 
{
  using namespace drizzle;
  uint32_t x;

  cout << "CREATE ";

  if (table->type() == Table::TEMPORARY)
    cout << "TEMPORARY ";

  cout << "TABLE `" << table->name() << "` (" << endl;
  
  for (x= 0; x < table->field_size() ; x++)
  {
    const Table::Field field = table->field(x);

    if (x != 0)
      cout << "," << endl;

    print_field(&field);
  }

  for (x= 0; x < table->index_size() ; x++)
  {
    const Table::Index index= table->index(x);

    if (x != 0)
      cout << "," << endl;;

    print_index(&index);

  }
  cout << endl;

  cout << ") " << endl;
  
  print_engine(&table->engine());

  /*
  if (table->has_options())
    print_table_options(&table->options());
  if (table->has_stats())
    print_table_stats(&table->stats());
  */
  if (table->has_comment())
    cout << " COMMENT = `" << table->comment() << "` " << endl;
}

void print_table_stats(const drizzle::Table::TableStats *stats) {
  
}

void print_table_options(const drizzle::Table::TableOptions *options) {
  if (options->has_collation())
    cout << " COLLATE = `" << options->collation() << "` " << endl;
  if (options->has_charset())
    cout << " CHARACTER SET = `" << options->charset() << "` " << endl;
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

  print_table(&table);

  return 0;
}
