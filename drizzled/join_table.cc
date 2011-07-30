/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <drizzled/field/blob.h>
#include <drizzled/join_table.h>
#include <drizzled/sql_lex.h>
#include <drizzled/sql_select.h>
#include <drizzled/table.h>
#include <drizzled/util/test.h>

namespace drizzled
{

int JoinTable::joinReadConstTable(optimizer::Position *pos)
{
  int error;
  Table *Table= this->table;
  Table->const_table=1;
  Table->null_row=0;
  Table->status=STATUS_NO_RECORD;

  if (this->type == AM_SYSTEM)
  {
    if ((error=this->joinReadSystem()))
    {						// Info for DESCRIBE
      this->info="const row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->setFanout(0.0);
      pos->clearRefDependMap();
      if (! Table->maybe_null || error > 0)
        return(error);
    }
  }
  else
  {
    if (! Table->key_read && 
        Table->covering_keys.test(this->ref.key) && 
        ! Table->no_keyread &&
        (int) Table->reginfo.lock_type <= (int) TL_READ_WITH_SHARED_LOCKS)
    {
      Table->key_read=1;
      Table->cursor->extra(HA_EXTRA_KEYREAD);
      this->index= this->ref.key;
    }
    error=join_read_const(this);
    if (Table->key_read)
    {
      Table->key_read=0;
      Table->cursor->extra(HA_EXTRA_NO_KEYREAD);
    }
    if (error)
    {
      this->info="unique row not found";
      /* Mark for EXPLAIN that the row was not found */
      pos->setFanout(0.0);
      pos->clearRefDependMap();
      if (!Table->maybe_null || error > 0)
        return(error);
    }
  }
  if (*this->on_expr_ref && !Table->null_row)
  {
    if ((Table->null_row= test((*this->on_expr_ref)->val_int() == 0)))
      Table->mark_as_null_row();
  }
  if (!Table->null_row)
    Table->maybe_null=0;

  /* Check appearance of new constant items in Item_equal objects */
  Join *Join= this->join;
  if (Join->conds)
    update_const_equal_items(Join->conds, this);
  TableList *tbl;
  for (tbl= Join->select_lex->leaf_tables; tbl; tbl= tbl->next_leaf)
  {
    TableList *embedded;
    TableList *embedding= tbl;
    do
    {
      embedded= embedding;
      if (embedded->on_expr)
         update_const_equal_items(embedded->on_expr, this);
      embedding= embedded->getEmbedding();
    }
    while (embedding &&
           &embedding->getNestedJoin()->join_list.front() == embedded);
  }

  return 0;
}

void JoinTable::readCachedRecord()
{
  unsigned char *pos;
  uint32_t length;
  bool last_record;
  CacheField *copy,*end_field;

  last_record= this->cache.record_nr++ == this->cache.ptr_record;
  pos= this->cache.pos;
  for (copy= this->cache.field, end_field= copy+this->cache.fields;
       copy < end_field;
       copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
        copy->blob_field->set_image(pos, copy->length+sizeof(char*),
                  copy->blob_field->charset());
        pos+=copy->length+sizeof(char*);
      }
      else
      {
        copy->blob_field->set_ptr(pos, pos+copy->length);
        pos+=copy->length+copy->blob_field->get_length();
      }
    }
    else
    {
      if (copy->strip)
      {
        length= uint2korr(pos);
        memcpy(copy->str, pos+2, length);
        memset(copy->str+length, ' ', copy->length-length);
        pos+= 2 + length;
      }
      else
      {
        memcpy(copy->str,pos,copy->length);
        pos+=copy->length;
      }
    }
  }
  this->cache.pos=pos;
}

int JoinTable::joinReadSystem()
{
  Table *Table= this->table;
  int error;
  if (Table->status & STATUS_GARBAGE)		// If first read
  {
    if ((error=Table->cursor->read_first_row(table->getInsertRecord(),
					   Table->getShare()->getPrimaryKey())))
    {
      if (error != HA_ERR_END_OF_FILE)
        return Table->report_error(error);
      this->table->mark_as_null_row();
      Table->emptyRecord();			// Make empty record
      return -1;
    }
    Table->storeRecord();
  }
  else if (!Table->status)			// Only happens with left join
    Table->restoreRecord();			// restore old record
  Table->null_row=0;
  return Table->status ? -1 : 0;
}

} /* namespace drizzled */
