/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <config.h>

#include <haildb.h>

#include "haildb_engine.h"

#include <drizzled/charset.h>
#include <drizzled/message/table.pb.h>

using namespace drizzled;

int get_haildb_system_table_message(const char* table_name, drizzled::message::Table *table_message)
{
  if (strcmp(table_name, "SYS_TABLES") == 0)
  {
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_tables");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("NAME");
    field->set_type(message::Table::Field::VARCHAR);
    message::Table::Field::StringFieldOptions *stropt= field->mutable_string_options();
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);

    field= table_message->add_field();
    field->set_name("ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("N_COLS");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("TYPE");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("MIX_ID");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("MIX_LEN");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("CLUSTER_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("SPACE");
    field->set_type(message::Table::Field::INTEGER);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(IB_MAX_TABLE_NAME_LEN);
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(IB_MAX_TABLE_NAME_LEN);

    return 0;
  }
  else if (strcmp(table_name, "SYS_COLUMNS") == 0)
  {
    message::Table::Field::StringFieldOptions *stropt;
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_columns");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("TABLE_ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("POS");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("MTYPE");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("PRTYPE");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("LEN");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("PREC");
    field->set_type(message::Table::Field::INTEGER);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(sizeof(uint64_t) + sizeof(uint32_t));
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(sizeof(uint64_t));
    part= index->add_index_part();
    part->set_fieldnr(1);
    part->set_compare_length(sizeof(uint32_t));

    return 0;
  }
  else if (strcmp(table_name, "SYS_INDEXES") == 0)
  {
    message::Table::Field::StringFieldOptions *stropt;
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_indexes");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("TABLE_ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("N_FIELDS");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("TYPE");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("SPACE");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("PAGE_NO");
    field->set_type(message::Table::Field::INTEGER);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(sizeof(uint64_t) + sizeof(uint32_t));
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(sizeof(uint64_t));
    part= index->add_index_part();
    part->set_fieldnr(1);
    part->set_compare_length(sizeof(uint32_t));

    return 0;
  }
  else if (strcmp(table_name, "SYS_FIELDS") == 0)
  {
    message::Table::Field::StringFieldOptions *stropt;
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_fields");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("INDEX_ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("POS");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("COL_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(sizeof(uint64_t) + sizeof(uint32_t));
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(sizeof(uint64_t));
    part= index->add_index_part();
    part->set_fieldnr(1);
    part->set_compare_length(sizeof(uint32_t));

    return 0;
  }
  else if (strcmp(table_name, "SYS_FOREIGN") == 0)
  {
    message::Table::Field::StringFieldOptions *stropt;
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_foreign");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("FOR_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("REF_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("N_COLS");
    field->set_type(message::Table::Field::INTEGER);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(sizeof(uint64_t));
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(sizeof(uint64_t));

    index= table_message->add_indexes();
    index->set_name("FOR_IND");
    index->set_is_primary(false);
    index->set_is_unique(false);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(IB_MAX_TABLE_NAME_LEN);
    part= index->add_index_part();
    part->set_fieldnr(1);
    part->set_compare_length(IB_MAX_TABLE_NAME_LEN);

    index= table_message->add_indexes();
    index->set_name("REF_IND");
    index->set_is_primary(false);
    index->set_is_unique(false);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(IB_MAX_TABLE_NAME_LEN);
    part= index->add_index_part();
    part->set_fieldnr(2);
    part->set_compare_length(IB_MAX_TABLE_NAME_LEN);
    return 0;
  }
  else if (strcmp(table_name, "SYS_FOREIGN_COLS") == 0)
  {
    message::Table::Field::StringFieldOptions *stropt;
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_sys_foreign_cols");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("ID");
    field->set_type(message::Table::Field::BIGINT);

    field= table_message->add_field();
    field->set_name("POS");
    field->set_type(message::Table::Field::INTEGER);

    field= table_message->add_field();
    field->set_name("FOR_COL_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("REF_COL_NAME");
    field->set_type(message::Table::Field::VARCHAR);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(sizeof(uint64_t) + sizeof(uint32_t));
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(sizeof(uint64_t));
    part= index->add_index_part();
    part->set_fieldnr(1);
    part->set_compare_length(sizeof(uint32_t));

    return 0;
  }

  return -1;
}
