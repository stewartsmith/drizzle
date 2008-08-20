#include <iostream>
#include <fstream>
#include <string>
#include "table.pb.h"
using namespace std;

/* 
  Written from Google proto example
*/

void fill_engine(drizzle::Table::StorageEngine *engine) {
  using drizzle::Table;
  int16_t x;

  engine->set_name("InnoDB");
  StorageEngine::EngineOptions *options= engine->mutable_options();

  ::string option_names[2]= {
    "INDEX_DIRECTORY"
      , "DATA_DIRECTORY"
  }

  ::string option_values[2]= {
    "/var/drizzle/indexdir"
      . "/var/drizzle/datadir"
  }

  /* Add some engine options */
  for (x= 0; x < 2; ++x) {
    option= options->add_option();
    option->set_name(option_names[x]);
    option->set_value(option_values[x]);
    option->set_type(StorageEngine::EngineOption::STRING);
  }

}

void new_index_to_table(
    drizzle::Table *table
    , const std::string name
    , uint16_t num_index_parts
    , uint32_t field_indexes[]
    , uint32_t compare_lengths[]
    , is_primary
    , is_unique
    ) const {
  using drizzle::Table;
  uint16_t x;

  Index *index= table->mutable_index();

  index->set_name(name);
  index->set_type(Index::BTREE);
  index->set_is_primary(is_primary);
  index->set_is_unique(is_unique);

  while (x < num_index_parts) {

    IndexPart *index_part= index->mutable_index_part();

    index_part->set_field(table->fields(field_indexes[x]));
    if (compare_length > 0)
      index_part->set_compare_length(compare_lengths[x]);

    index->set_index_part(index_part);
    x++;
  }
}

void fill_table(drizzle::Table *table, const char *name) 
{
  int x;

  using drizzle::Table;

  Field *field;
  Field::FieldConstraints *field_constraints;
  Field::FieldOptions *field_options;
  Field::StringFieldOptions *string_field_options;
  Field::NumericFieldOptions *numeric_field_options;
  Field::SetFieldOptions *set_field_options;

  table->set_name(name);

  /* Write out some random varchar */
  for (x= 0; x < 3; x++)
  {
    char buffer[1024];
    field= table->add_field();

    sprintf(buffer, "sample%u", x);

    field->set_name(buffer);
    field->set_type(Field::VARCHAR);

    field_constraints= field->mutable_constraints();

    if (x % 2)
      field_constraints->set_is_nullable(true);

    string_field_options= field->mutable_string_options();
    string_field_options->set_length(rand() % 100);

    if (x % 3)
    {
      string_field_options->set_collation("utf8_swedish_ci");
      string_field_options->set_charset("utf8");
    }
    field->set_string_options(string_field_options);
    field->set_constraints(field_constraints);
  }

  /* Write out an INTEGER */
  {
    field= table->add_field();
    field->set_name("number");
    field->set_type(Field::INTEGER);
  }
  /* Write out a ENUM */
  {
    field= table->add_field();
    field->set_type(Field::ENUM);
    field->set_name("colors");

    set_field_options= field->mutable_set_options();
    set_field_options->add_values("red");
    set_field_options->add_values("blue");
    set_field_options->add_values("green");
    field->set_set_field_options(set_field_options);
  }
  /* Write out a BLOB */
  {
    field= table->add_field();
    field->set_name("some_btye_string");
    field->set_type(Field::BLOB);
  }
  /* Write out a DECIMAL */
  {
    field= table->add_field();
    field->set_name("important_number");
    field->set_type(Field::DECIMAL);

    field_constraints= field->mutable_constraints();
    field_constraints->set_is_nullable(true);
    field->set_field_constrains(field_constraints);
    
    numeric_field_options= field->mutable_numeric_field_options();
    numeric_field_options->set_precision(8);
    numeric_field_options->set_scale(3);
    field->set_numeric_options(numeric_field_options);
  }

  {
  uint32_t fields_in_index[1]= {6};
  uint32_t compare_lengths_in_index[1]= {0};
  bool is_unique= true;
  bool is_primary= false;
  /* Add a single-column index on important_number field */
  new_index_to_table(
      table
    , "idx_important_decimal"
    , 1
    , fields_in_index
    , compare_lengths_in_index
    , is_primary
    , is_unique
    );
  }

  {
  /* Add a double-column index on first two varchar fields */
  uint32_t fields_in_index[2]= {0,1};
  uint32_t compare_lengths_in_index[2]= {20,35};
  bool is_unique= true;
  bool is_primary= true;
  new_index_to_table(
      table
    , "idx_varchar1_2"
    , 2
    , fields_in_index
    , compare_lengths_in_index
    , is_primary
    , is_unique
    );
  }

  /* Do engine-specific stuff */
  StorageEngine *engine= table->mutable_engine();
  fill_engine(engine);
  table->set_engine(engine);

}

int main(int argc, char* argv[]) 
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " SCHEMA" << endl;
    return -1;
  }

  drizzle::Table table;

  update_table(&table, "example_table");

  fstream output(argv[1], ios::out | ios::trunc | ios::binary);
  if (!table.SerializeToOstream(&output)) 
  {
    cerr << "Failed to write schema." << endl;
    return -1;
  }

  return 0;
}
