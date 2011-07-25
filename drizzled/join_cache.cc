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

/**
 * @file
 *
 * Implementation of the JOIN cache
 * 
 * @defgroup Query_Optimizer  Query Optimizer
 * @{
 */

#include <config.h>

#include <drizzled/sql_select.h> /* include join.h */
#include <drizzled/field/blob.h>
#include <drizzled/drizzled.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

#include <algorithm>

using namespace std;

namespace drizzled {

static uint32_t used_blob_length(CacheField **ptr)
{
  uint32_t length,blob_length;
  for (length=0 ; *ptr ; ptr++)
  {
    (*ptr)->blob_length=blob_length=(*ptr)->blob_field->get_length();
    length+=blob_length;
    (*ptr)->str= (*ptr)->blob_field->get_ptr();
  }
  return length;
}

/*****************************************************************************
  Fill join cache with packed records
  Records are stored in tab->cache.buffer and last record in
  last record is stored with pointers to blobs to support very big
  records
******************************************************************************/
int join_init_cache(Session *session, JoinTable *tables, uint32_t table_count)
{
  unsigned int length, blobs;
  size_t size;
  CacheField *copy,**blob_ptr;
  JoinCache  *cache;
  JoinTable *join_tab;

  cache= &tables[table_count].cache;
  cache->fields=blobs=0;

  join_tab= tables;
  for (unsigned int i= 0; i < table_count ; i++, join_tab++)
  {
    if (!join_tab->used_fieldlength)		/* Not calced yet */
      calc_used_field_length(session, join_tab);
    cache->fields+=join_tab->used_fields;
    blobs+=join_tab->used_blobs;

    /* SemiJoinDuplicateElimination: reserve space for rowid */
    if (join_tab->rowid_keep_flags & JoinTable::KEEP_ROWID)
    {
      cache->fields++;
      join_tab->used_fieldlength += join_tab->table->cursor->ref_length;
    }
  }
  if (!(cache->field=(CacheField*)
        memory::sql_alloc(sizeof(CacheField)*(cache->fields+table_count*2)+(blobs+1)* sizeof(CacheField*))))
  {
    size= cache->end - cache->buff;
    global_join_buffer.sub(size);
    free((unsigned char*) cache->buff);
    cache->buff=0;
    return 1;
  }
  copy=cache->field;
  blob_ptr=cache->blob_ptr=(CacheField**)
    (cache->field+cache->fields+table_count*2);

  length=0;
  for (unsigned int i= 0 ; i < table_count ; i++)
  {
    uint32_t null_fields=0, used_fields;
    Field **f_ptr,*field;
    for (f_ptr= tables[i].table->getFields(), used_fields= tables[i].used_fields; used_fields; f_ptr++)
    {
      field= *f_ptr;
      if (field->isReadSet())
      {
        used_fields--;
        length+=field->fill_cache_field(copy);
        if (copy->blob_field)
          (*blob_ptr++)=copy;
        if (field->maybe_null())
          null_fields++;
        copy->get_rowid= NULL;
        copy++;
      }
    }
    /* Copy null bits from table */
    if (null_fields && tables[i].table->getNullFields())
    {						/* must copy null bits */
      copy->str= tables[i].table->null_flags;
      copy->length= tables[i].table->getShare()->null_bytes;
      copy->strip=0;
      copy->blob_field=0;
      copy->get_rowid= NULL;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
    /* If outer join table, copy null_row flag */
    if (tables[i].table->maybe_null)
    {
      copy->str= (unsigned char*) &tables[i].table->null_row;
      copy->length=sizeof(tables[i].table->null_row);
      copy->strip=0;
      copy->blob_field=0;
      copy->get_rowid= NULL;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
    /* SemiJoinDuplicateElimination: Allocate space for rowid if needed */
    if (tables[i].rowid_keep_flags & JoinTable::KEEP_ROWID)
    {
      copy->str= tables[i].table->cursor->ref;
      copy->length= tables[i].table->cursor->ref_length;
      copy->strip=0;
      copy->blob_field=0;
      copy->get_rowid= NULL;
      if (tables[i].rowid_keep_flags & JoinTable::CALL_POSITION)
      {
        /* We will need to call h->position(): */
        copy->get_rowid= tables[i].table;
        /* And those after us won't have to: */
        tables[i].rowid_keep_flags&=  ~((int)JoinTable::CALL_POSITION);
      }
      copy++;
    }
  }

  cache->length= length+blobs*sizeof(char*);
  cache->blobs= blobs;
  *blob_ptr= NULL;					/* End sequentel */
  size= max((size_t) session->variables.join_buff_size, (size_t)cache->length);
  if (not global_join_buffer.add(size))
  {
    my_error(ER_OUT_OF_GLOBAL_JOINMEMORY, MYF(ME_ERROR+ME_WAITTANG));
    return 1;
  }
  cache->buff= (unsigned char*) malloc(size);
  cache->end= cache->buff+size;
  cache->reset_cache_write();

  return 0;
}

bool JoinCache::store_record_in_cache()
{
  JoinCache *cache= this;
  unsigned char *local_pos;
  CacheField *copy,*end_field;
  bool last_record;

  local_pos= cache->pos;
  end_field= cache->field+cache->fields;

  {
    uint32_t local_length;

    local_length= cache->length;
    if (cache->blobs)
    {
      local_length+= used_blob_length(cache->blob_ptr);
    }

    if ((last_record= (local_length + cache->length > (size_t) (cache->end - local_pos))))
    {
      cache->ptr_record= cache->records;
    }
  }

  /*
    There is room in cache. Put record there
  */
  cache->records++;
  for (copy= cache->field; copy < end_field; copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
        copy->blob_field->get_image(local_pos, copy->length+sizeof(char*), copy->blob_field->charset());
        local_pos+= copy->length+sizeof(char*);
      }
      else
      {
        copy->blob_field->get_image(local_pos, copy->length, // blob length
				    copy->blob_field->charset());
        memcpy(local_pos + copy->length,copy->str,copy->blob_length);  // Blob data
        local_pos+= copy->length+copy->blob_length;
      }
    }
    else
    {
      // SemiJoinDuplicateElimination: Get the rowid into table->ref:
      if (copy->get_rowid)
        copy->get_rowid->cursor->position(copy->get_rowid->getInsertRecord());

      if (copy->strip)
      {
        unsigned char *str, *local_end;
        for (str= copy->str,local_end= str+copy->length; local_end > str && local_end[-1] == ' '; local_end--) {}

        uint32_t local_length= (uint32_t) (local_end - str);
        memcpy(local_pos+2, str, local_length);
        int2store(local_pos, local_length);
        local_pos+= local_length+2;
      }
      else
      {
        memcpy(local_pos, copy->str, copy->length);
        local_pos+= copy->length;
      }
    }
  }
  cache->pos= local_pos;
  return last_record || (size_t) (cache->end - local_pos) < cache->length;
}

void JoinCache::reset_cache_read()
{
  record_nr= 0;
  pos= buff;
}

void JoinCache::reset_cache_write()
{
  reset_cache_read();
  records= 0;
  ptr_record= UINT32_MAX;
}

/**
  @} (end of group Query_Optimizer)
*/

} /* namespace drizzled */
