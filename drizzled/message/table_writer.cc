/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <iostream>
#include <fstream>
#include <string>
#include <drizzled/message/table.pb.h>

#include <boost/program_options.hpp>

using namespace std;
using namespace drizzled;

namespace po=boost::program_options;

/*
  Written from Google proto example
*/

static void fill_engine(message::Engine *engine)
{
  engine->set_name("InnoDB");
  message::Engine::Option *option;

  string option_names[2]= {
    "INDEX_DIRECTORY"
    , "DATA_DIRECTORY"
  };

  string option_values[2]= {
    "/var/drizzle/indexdir"
    , "/var/drizzle/datadir"
  };

  /* Add some engine options */
  for (int16_t x= 0; x < 2; x++)
  {
    option= engine->add_options();
    option->set_name(option_names[x]);
    option->set_state(option_values[x]);
  }
}

static void new_index_to_table(message::Table *table,
                               const string name,
                               uint16_t num_index_parts,
                               uint32_t field_indexes[],
                               uint32_t compare_lengths[],
                               bool is_primary,
                               bool is_unique)
{
  uint16_t x= 0;

  message::Table::Index *index;
  message::Table::Index::IndexPart *index_part;

  index= table->add_indexes();

  index->set_name(name);
  index->set_type(message::Table::Index::BTREE);
  index->set_is_primary(is_primary);
  index->set_is_unique(is_unique);

  int key_length= 0;

  for(int i=0; i< num_index_parts; i++)
    key_length+= compare_lengths[i];

  index->set_key_length(key_length);

  while (x < num_index_parts)
  {
    index_part= index->add_index_part();

    index_part->set_fieldnr(field_indexes[x]);

    if (compare_lengths[x] > 0)
      index_part->set_compare_length(compare_lengths[x]);

    x++;
  }
}

static void fill_table(message::Table *table, const char *name)
{
  uint16_t x;

  message::Table::Field *field;
  message::Table::Field::FieldConstraints *field_constraints;
  message::Table::Field::StringFieldOptions *string_field_options;
  message::Table::Field::NumericFieldOptions *numeric_field_options;
  message::Table::Field::EnumerationValues *enumeration_options;

  table->set_name(name);
  table->set_type(message::Table::STANDARD);

  /* Write out some random varchar */
  for (x= 0; x < 3; x++)
  {
    char buffer[1024];
    field= table->add_field();
    field_constraints= field->mutable_constraints();
    string_field_options= field->mutable_string_options();

    snprintf(buffer, sizeof(buffer), "sample%u", x);

    field->set_name(buffer);
    field->set_type(message::Table::Field::VARCHAR);

    field_constraints->set_is_notnull((x % 2));

    string_field_options->set_length(rand() % 100);

    if (x % 3)
    {
      string_field_options->set_collation("utf8_swedish_ci");
    }
  }

  /* Write out an INTEGER */
  {
    field= table->add_field();
    field->set_name("number");
    field->set_type(message::Table::Field::INTEGER);
  }
  /* Write out a ENUM */
  {
    field= table->add_field();
    field->set_type(message::Table::Field::ENUM);
    field->set_name("colors");

    enumeration_options= field->mutable_enumeration_values();
    enumeration_options->add_field_value("red");
    enumeration_options->add_field_value("blue");
    enumeration_options->add_field_value("green");
  }
  /* Write out a BLOB */
  {
    field= table->add_field();
    field->set_name("some_btye_string");
    field->set_type(message::Table::Field::BLOB);
  }

  /* Write out a DECIMAL */
  {
    field= table->add_field();
    field->set_name("important_number");
    field->set_type(message::Table::Field::DECIMAL);

    field_constraints= field->mutable_constraints();
    field_constraints->set_is_notnull(false);

    numeric_field_options= field->mutable_numeric_options();
    numeric_field_options->set_precision(8);
    numeric_field_options->set_scale(3);
  }

  {
    uint32_t fields_in_index[1]= {6};
    uint32_t compare_lengths_in_index[1]= {0};
    bool is_unique= true;
    bool is_primary= false;
    /* Add a single-column index on important_number field */
    new_index_to_table(table, "idx_important_decimal", 1, fields_in_index, compare_lengths_in_index, is_primary, is_unique);
  }

  {
    /* Add a double-column index on first two varchar fields */
    uint32_t fields_in_index[2]= {0,1};
    uint32_t compare_lengths_in_index[2]= {20,35};
    bool is_unique= true;
    bool is_primary= true;
    new_index_to_table(table, "idx_varchar1_2", 2, fields_in_index, compare_lengths_in_index, is_primary, is_unique);
  }

  /* Do engine-specific stuff */
  message::Engine *engine= table->mutable_engine();
  fill_engine(engine);

}

static void fill_table1(message::Table *table)
{
  message::Table::Field *field;
  message::Table::TableOptions *tableopts;

  table->set_name("t1");
  table->set_catalog("LOCAL");
  table->set_type(message::Table::INTERNAL);

  tableopts= table->mutable_options();
  tableopts->set_comment("Table without a StorageEngine message");

  {
    field= table->add_field();
    field->set_name("number");
    field->set_type(message::Table::Field::INTEGER);
  }

}


int main(int argc, char* argv[])
{
  int table_number= 0;

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("table-number,t", po::value<int>(&table_number)->default_value(0), "Table Number");

  po::variables_map vm;
  po::positional_options_description p;
  p.add("table-name", 1);

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(argc, argv).options(desc).style(style).
            positional(p).run(), vm);

  if (not vm.count("table-name"))
  {
    fprintf(stderr, "Expected Table name argument\n\n");
    cerr << desc << endl;
    exit(EXIT_FAILURE);
  }

  message::Table table;

  switch (table_number)
  {
  case 0:
    fill_table(&table, "example_table");
    break;
  case 1:
    fill_table1(&table);
    break;
  default:
    fprintf(stderr, "Invalid table number.\n\n");
    cerr << desc << endl;
    exit(EXIT_FAILURE);
  }

  fstream output(vm["table-name"].as<string>().c_str(),
                 ios::out | ios::trunc | ios::binary);
  if (not table.SerializeToOstream(&output))
  {
    cerr << "Failed to write schema." << endl;
    return -1;
  }

  return 0;
}
