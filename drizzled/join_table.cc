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

#include "config.h"
#include <drizzled/join_table.h>
#include <drizzled/field/blob.h>

namespace drizzled
{

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

} /* namespace drizzled */
