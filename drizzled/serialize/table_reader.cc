#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <drizzled/serialize/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace std;
using namespace drizzle;
using namespace google::protobuf::io;

/*
  Written from Google proto example
*/

void print_field(const ::drizzle::Table::Field &field)
{
  cout << "\t`" << field.name() << "`";
  switch (field.type())
  {
    case Table::Field::DOUBLE:
    cout << " DOUBLE ";
    break;
  case Table::Field::VARCHAR:
    cout << " VARCHAR(" << field.string_options().length() << ")";
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
      for (x= 0; x < field.set_options().field_value_size() ; x++)
      {
        const string type= field.set_options().field_value(x);

        if (x != 0)
          cout << ",";
        cout << "'" << type << "'";
      }
      cout << ") ";
      break;
    }
  case Table::Field::TINYINT:
    cout << " TINYINT ";
    break;
  case Table::Field::INTEGER:
    cout << " INT" ;
    break;
  case Table::Field::BIGINT:
    cout << " BIGINT ";
    break;
  case Table::Field::DECIMAL:
    cout << " DECIMAL(" << field.numeric_options().precision() << "," << field.numeric_options().scale() << ") ";
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
  case Table::Field::VIRTUAL:
    cout << " VIRTUAL"; // FIXME
    break;
  }

  if (field.type() == Table::Field::INTEGER
      || field.type() == Table::Field::BIGINT
      || field.type() == Table::Field::TINYINT)
  {
    if (field.has_constraints()
        && field.constraints().has_is_unsigned())
      if (field.constraints().is_unsigned())
        cout << " UNSIGNED";

    if (field.has_numeric_options() &&
      field.numeric_options().is_autoincrement())
      cout << " AUTOINCREMENT ";
  }

  if (! field.has_constraints()
      && field.constraints().is_nullable())
    cout << " NOT NULL ";

  if (field.type() == Table::Field::TEXT
      || field.type() == Table::Field::VARCHAR)
  {
    if (field.string_options().has_collation())
      cout << " COLLATE " << field.string_options().collation();
  }

  if (field.options().has_default_value())
    cout << " DEFAULT `" << field.options().default_value() << "` " ;

  if (field.type() == Table::Field::TIMESTAMP)
    if (field.timestamp_options().has_auto_updates()
      && field.timestamp_options().auto_updates())
      cout << " ON UPDATE CURRENT_TIMESTAMP";

  if (field.has_comment())
    cout << " COMMENT `" << field.comment() << "` ";
}

void print_engine(const ::drizzle::Table::StorageEngine &engine)
{
  int32_t x;

  cout << " ENGINE = " << engine.name()  << endl;

  for (x= 0; x < engine.option_size(); ++x) {
    const Table::StorageEngine::EngineOption option= engine.option(x);
    cout << "\t" << option.option_name() << " = "
	 << option.option_value() << endl;
  }
}

void print_index(const ::drizzle::Table::Index &index)
{

  if (index.is_primary())
    cout << " PRIMARY";
  else if (index.is_unique())
    cout << " UNIQUE";
  cout << " KEY `" << index.name() << "` (";
  {
    int32_t x;

    for (x= 0; x < index.index_part_size() ; x++)
    {
      const Table::Index::IndexPart part= index.index_part(x);

      if (x != 0)
        cout << ",";
      cout << "`" << part.fieldnr() << "`"; /* FIXME */
      if (part.has_compare_length())
        cout << "(" << part.compare_length() << ")";
    }
    cout << ")";
  }
  cout << "\t";
}

void print_table_stats(const ::drizzle::Table::TableStats&) 
{

}

void print_table_options(const ::drizzle::Table::TableOptions &options)
{
  if (options.has_comment())
    cout << " COMMENT = '" << options.comment() << "' " << endl;

  if (options.has_collation())
    cout << " COLLATE = '" << options.collation() << "' " << endl;

  if (options.has_auto_increment())
    cout << " AUTOINCREMENT_OFFSET = " << options.auto_increment() << endl;

  if (options.has_collation_id())
    cout << "-- collation_id = " << options.collation_id() << endl;
  
  if (options.has_connect_string())
    cout << " CONNECT_STRING = '" << options.connect_string() << "'"<<endl;

  if (options.has_row_type())
    cout << " ROW_TYPE = " << options.row_type() << endl;

/*    optional string data_file_name = 5;
    optional string index_file_name = 6;
    optional uint64 max_rows = 7;
    optional uint64 min_rows = 8;
    optional uint64 auto_increment_value = 9;
    optional uint32 avg_row_length = 11;
    optional uint32 key_block_size = 12;
    optional uint32 block_size = 13;
    optional string comment = 14;
    optional bool pack_keys = 15;
    optional bool checksum = 16;
    optional bool page_checksum = 17;
    optional bool delay_key_write = 18;
*/
}


void print_table(const ::drizzle::Table &table)
{
  int32_t x;

  cout << "CREATE ";

  if (table.type() == Table::TEMPORARY)
    cout << "TEMPORARY ";

  cout << "TABLE `" << table.name() << "` (" << endl;

  for (x= 0; x < table.field_size() ; x++)
  {
    const Table::Field field = table.field(x);

    if (x != 0)
      cout << "," << endl;

    print_field(field);
  }

  for (x= 0; x < table.indexes_size() ; x++)
  {
    const Table::Index index= table.indexes(x);

    if (x != 0)
      cout << "," << endl;;

    print_index(index);

  }
  cout << endl;

  cout << ") " << endl;

  print_engine(table.engine());

  if (table.has_options())
    print_table_options(table.options());
  /*
  if (table->has_stats())
    print_table_stats(&table->stats());
  */
}

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  Table table;

  {
    int fd= open(argv[1], O_RDONLY);

    if(fd==-1)
    {
      perror("Failed to open table definition file");
      return -1;
    }

    ZeroCopyInputStream* input = new FileInputStream(fd);

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
