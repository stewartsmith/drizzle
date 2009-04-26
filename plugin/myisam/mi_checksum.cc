/* Copyright (C) 2000-2001, 2003-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Calculate a checksum for a row */

#include "myisamdef.h"

ha_checksum mi_checksum(MI_INFO *info, const unsigned char *buf)
{
  uint32_t i;
  ha_checksum crc=0;
  MI_COLUMNDEF *rec=info->s->rec;

  for (i=info->s->base.fields ; i-- ; buf+=(rec++)->length)
  {
    const unsigned char *pos;
    ulong length;
    switch (rec->type) {
    case FIELD_BLOB:
    {
      length=_mi_calc_blob_length(rec->length-
					portable_sizeof_char_ptr,
					buf);
      memcpy(&pos, buf+rec->length - portable_sizeof_char_ptr, sizeof(char*));
      break;
    }
    case FIELD_VARCHAR:
    {
      uint32_t pack_length= HA_VARCHAR_PACKLENGTH(rec->length-1);
      if (pack_length == 1)
        length= (ulong) *(unsigned char*) buf;
      else
        length= uint2korr(buf);
      pos= buf+pack_length;
      break;
    }
    default:
      length=rec->length;
      pos=buf;
      break;
    }
    crc=my_checksum(crc, pos ? pos : (unsigned char*) "", length);
  }
  return crc;
}


ha_checksum mi_static_checksum(MI_INFO *info, const unsigned char *pos)
{
  return my_checksum(0, pos, info->s->base.reclength);
}
