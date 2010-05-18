/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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

#include "config.h"
#include "drizzled/sql_select.h" /* include join.h */
#include "drizzled/field/blob.h"

#include <algorithm>

using namespace std;

namespace drizzled
{

static uint32_t used_blob_length(CacheField **ptr);

static uint32_t used_blob_length(CacheField **ptr)
{
  uint32_t length,blob_length;
  for (length=0 ; *ptr ; ptr++)
  {
    (*ptr)->blob_length=blob_length=(*ptr)->blob_field->get_length();
    length+=blob_length;
    (*ptr)->blob_field->get_ptr(&(*ptr)->str);
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
    free((unsigned char*) cache->buff);
    cache->buff=0;
    return(1);
  }
  copy=cache->field;
  blob_ptr=cache->blob_ptr=(CacheField**)
    (cache->field+cache->fields+table_count*2);

  length=0;
  for (unsigned int i= 0 ; i < table_count ; i++)
  {
    uint32_t null_fields=0, used_fields;
    Field **f_ptr,*field;
    for (f_ptr= tables[i].table->field,used_fields= tables[i].used_fields; used_fields; f_ptr++)
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
      copy->length= tables[i].table->s->null_bytes;
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
  if (!(cache->buff= (unsigned char*) malloc(size)))
    return 1;
  cache->end= cache->buff+size;
  reset_cache_write(cache);
  return 0;
}

bool store_record_in_cache(JoinCache *cache)
{
  uint32_t length;
  unsigned char *pos;
  CacheField *copy,*end_field;
  bool last_record;

  pos= cache->pos;
  end_field= cache->field+cache->fields;

  length= cache->length;
  if (cache->blobs)
    length+= used_blob_length(cache->blob_ptr);
  if ((last_record= (length + cache->length > (size_t) (cache->end - pos))))
    cache->ptr_record= cache->records;
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
        copy->blob_field->get_image(pos, copy->length+sizeof(char*), copy->blob_field->charset());
        pos+= copy->length+sizeof(char*);
      }
      else
      {
        copy->blob_field->get_image(pos, copy->length, // blob length
				    copy->blob_field->charset());
        memcpy(pos+copy->length,copy->str,copy->blob_length);  // Blob data
        pos+= copy->length+copy->blob_length;
      }
    }
    else
    {
      // SemiJoinDuplicateElimination: Get the rowid into table->ref:
      if (copy->get_rowid)
        copy->get_rowid->cursor->position(copy->get_rowid->record[0]);

      if (copy->strip)
      {
        unsigned char *str,*end;
        for (str= copy->str,end= str+copy->length; end > str && end[-1] == ' '; end--)
        {}
        length= (uint32_t) (end-str);
        memcpy(pos+2, str, length);
        int2store(pos, length);
        pos+= length+2;
      }
      else
      {
        memcpy(pos,copy->str,copy->length);
        pos+= copy->length;
      }
    }
  }
  cache->pos= pos;
  return last_record || (size_t) (cache->end - pos) < cache->length;
}

void reset_cache_read(JoinCache *cache)
{
  cache->record_nr= 0;
  cache->pos= cache->buff;
}

void reset_cache_write(JoinCache *cache)
{
  reset_cache_read(cache);
  cache->records= 0;
  cache->ptr_record= UINT32_MAX;
}

/**
  @} (end of group Query_Optimizer)
*/

} /* namespace drizzled */
