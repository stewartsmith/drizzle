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

using namespace std;
using namespace drizzled::message;
using namespace google::protobuf::io;

/*
  Written from Google proto example
*/

void print_field(const ::drizzled::message::Table::Field &field)
{
  cout << "\t`" << field.name() << "`";

  Table::Field::FieldType field_type= field.type();

  if(field_type==Table::Field::VIRTUAL)
  {
    cout << " VIRTUAL"; // FIXME
    field_type= field.virtual_options().type();
  }

  switch (field_type)
  {
    case Table::Field::DOUBLE:
    cout << " DOUBLE ";
    break;
  case Table::Field::VARCHAR:
    cout << " VARCHAR(" << field.string_options().length() << ")";
    break;
  case Table::Field::BLOB:
    cout << " BLOB "; /* FIXME: or text, depends on collation */
    if(field.string_options().has_collation_id())
      cout << "COLLATION=" << field.string_options().collation_id() << " ";
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
    abort(); // handled above.
  }

  if(field.type()==Table::Field::VIRTUAL)
  {
    cout << " AS (" << field.virtual_options().expression() << ") ";
    if(field.virtual_options().physically_stored())
      cout << " STORED ";
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

  if (!( field.has_constraints()
	 && field.constraints().is_nullable()))
    cout << " NOT NULL ";

  if (field.type() == Table::Field::BLOB
      || field.type() == Table::Field::VARCHAR)
  {
    if (field.string_options().has_collation())
      cout << " COLLATE " << field.string_options().collation();
  }

  if (field.options().has_default_value())
    cout << " DEFAULT `" << field.options().default_value() << "` " ;

  if (field.options().has_default_bin_value())
  {
    string v= field.options().default_bin_value();
    cout << " DEFAULT 0x";
    for(unsigned int i=0; i< v.length(); i++)
    {
      printf("%.2x", *(v.c_str()+i));
    }
  }

  if (field.type() == Table::Field::TIMESTAMP)
    if (field.timestamp_options().has_auto_updates()
      && field.timestamp_options().auto_updates())
      cout << " ON UPDATE CURRENT_TIMESTAMP";

  if (field.has_comment())
    cout << " COMMENT `" << field.comment() << "` ";
}

void print_engine(const ::drizzled::message::Table::StorageEngine &engine)
{
  int32_t x;

  cout << " ENGINE = " << engine.name()  << endl;

  for (x= 0; x < engine.option_size(); ++x) {
    const Table::StorageEngine::EngineOption option= engine.option(x);
    cout << "\t" << option.option_name() << " = "
	 << option.option_value() << endl;
  }
}

void print_index(const ::drizzled::message::Table::Index &index)
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

void print_table_stats(const ::drizzled::message::Table::TableStats&) 
{

}

void print_table_options(const ::drizzled::message::Table::TableOptions &options)
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

  if (options.has_data_file_name())
    cout << " DATA_FILE_NAME = '" << options.data_file_name() << "'" << endl;

  if (options.has_index_file_name())
    cout << " INDEX_FILE_NAME = '" << options.index_file_name() << "'" << endl;

  if (options.has_max_rows())
    cout << " MAX_ROWS = " << options.max_rows() << endl;

  if (options.has_min_rows())
    cout << " MIN_ROWS = " << options.min_rows() << endl;

  if (options.has_auto_increment_value())
    cout << " AUTO_INCREMENT = " << options.auto_increment_value() << endl;

  if (options.has_avg_row_length())
    cout << " AVG_ROW_LENGTH = " << options.avg_row_length() << endl;

  if (options.has_key_block_size())
    cout << " KEY_BLOCK_SIZE = "  << options.key_block_size() << endl;

  if (options.has_block_size())
    cout << " BLOCK_SIZE = " << options.block_size() << endl;

  if (options.has_comment())
    cout << " COMMENT = '" << options.comment() << "'" << endl;

  if (options.has_pack_keys())
    cout << " PACK_KEYS = " << options.pack_keys() << endl;
  if (options.has_pack_record())
    cout << " PACK_RECORD = " << options.pack_record() << endl;
  if (options.has_checksum())
    cout << " CHECKSUM = " << options.checksum() << endl;
  if (options.has_page_checksum())
    cout << " PAGE_CHECKSUM = " << options.page_checksum() << endl;
  if (options.has_delay_key_write())
    cout << " DELAY_KEY_WRITE = " << options.delay_key_write() << endl;
}


void print_table(const ::drizzled::message::Table &table)
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
