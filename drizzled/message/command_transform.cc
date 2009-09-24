/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Implementation of various routines that can be used to convert
 * Command messages to other formats, including SQL statements.
 */

#include "drizzled/message/command_transform.h"
#include "drizzled/message/replication.pb.h"

#include <string>
#include <bitset>

namespace drizzled
{
namespace message
{

void transformCommand2Sql(const Command &source,
                          std::string *destination,
                          enum CommandTransformSqlVariant sql_variant)
{
  
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  switch (source.type())
  {
  case Command::START_TRANSACTION:
    destination->assign("START TRANSACTION");
    break;
  case Command::COMMIT:
    destination->assign("COMMIT");
    break;
  case Command::ROLLBACK:
    destination->assign("ROLLBACK");
    break;
  case Command::INSERT:
    {
      destination->assign("INSERT INTO ");
      destination->push_back(quoted_identifier);
      destination->append(source.schema());
      destination->push_back(quoted_identifier);
      destination->push_back('.');
      destination->push_back(quoted_identifier);
      destination->append(source.table());
      destination->push_back(quoted_identifier);
      destination->append(" (");

      const message::InsertRecord &record= source.insert_record();

      int32_t num_fields= record.insert_field_size();

      int32_t x;
      for (x= 0; x < num_fields; x++)
      {
        if (x != 0)
          destination->push_back(',');

        const message::Table::Field f= record.insert_field(x);

        destination->push_back(quoted_identifier);
        destination->append(f.name());
        destination->push_back(quoted_identifier);
      }

      destination->append(") VALUES ");

      /* 
       * There may be an INSERT VALUES (),() type statement.  We know the
       * number of records is equal to the field_values array size divided
       * by the number of fields.
       *
       * So, we do an inner and an outer loop.  Outer loop is on the number
       * of records and the inner loop on the number of fields.  In this way, 
       * we know that record.field_values(outer_loop * num_fields) + inner_loop))
       * always gives us our correct field value.
       */
      int32_t num_records= (record.insert_value_size() / num_fields);
      int32_t y;
      for (x= 0; x < num_records; x++)
      {
        if (x != 0)
          destination->push_back(',');

        destination->push_back('(');
        for (y= 0; y < num_fields; y++)
        {
          if (y != 0)
            destination->push_back(',');

          destination->push_back('\'');
          destination->append(record.insert_value((x * num_fields) + y));
          destination->push_back('\'');
        }
        destination->push_back(')');
      }
    }
    break;
  case Command::UPDATE:
    {
      const message::UpdateRecord &record= source.update_record();
      int32_t num_update_fields= record.update_field_size();
      int32_t x;

      destination->assign("UPDATE ");
      destination->push_back(quoted_identifier);
      destination->append(source.schema());
      destination->push_back(quoted_identifier);
      destination->push_back('.');
      destination->push_back(quoted_identifier);
      destination->append(source.table());
      destination->push_back(quoted_identifier);
      destination->append(" SET ");

      for (x= 0;x < num_update_fields; x++)
      {
        Table::Field f= record.update_field(x);
        
        if (x != 0)
          destination->push_back(',');

        destination->push_back(quoted_identifier);
        destination->append(f.name());
        destination->push_back(quoted_identifier);
        destination->append("='");
        destination->append(record.after_value(x));
        destination->push_back('\'');
      }

      int32_t num_where_fields= record.where_field_size();
      /* 
       * Make sure we catch anywhere we're not aligning the fields with
       * the field_values arrays...
       */
      assert(num_where_fields == record.where_value_size());

      destination->append(" WHERE ");
      for (x= 0;x < num_where_fields; x++)
      {
        if (x != 0)
          destination->append(" AND "); /* Always AND condition with a multi-column PK */

        const Table::Field f= record.where_field(x);

        /* Always equality conditions */
        destination->push_back(quoted_identifier);
        destination->append(f.name());
        destination->push_back(quoted_identifier);
        destination->append("='");
        destination->append(record.where_value(x));
        destination->push_back('\'');
      }
    }
    break;
  case Command::DELETE:
    {
      const message::DeleteRecord &record= source.delete_record();
      int32_t num_where_fields= record.where_field_size();
      int32_t x;

      destination->assign("DELETE FROM ");
      destination->push_back(quoted_identifier);
      destination->append(source.schema());
      destination->push_back(quoted_identifier);
      destination->push_back('.');
      destination->push_back(quoted_identifier);
      destination->append(source.table());
      destination->push_back(quoted_identifier);
      
      destination->append(" WHERE ");
      for (x= 0; x < num_where_fields; x++)
      {
        if (x != 0)
          destination->append(" AND "); /* Always AND condition with a multi-column PK */

        const Table::Field f= record.where_field(x);

        /* Always equality conditions */
        destination->push_back(quoted_identifier);
        destination->append(f.name());
        destination->push_back(quoted_identifier);
        destination->append(" = '");
        destination->append(record.where_value(x));
        destination->push_back('\'');
      }
    }
    break;
  case Command::RAW_SQL:
    destination->assign(source.sql());
    break;
  }
}

} /* end namespace drizzled::message */
} /* end namespace drizzled */
