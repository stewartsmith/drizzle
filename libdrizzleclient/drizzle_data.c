/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/global.h>

#include "libdrizzle_priv.h"

#include "drizzle_data.h"
#include "drizzle_rows.h"
#include "drizzle_field.h"

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************************
  Change field rows to field structs
***************************************************************************/

DRIZZLE_FIELD *
unpack_fields(DRIZZLE_DATA *data, unsigned int fields,
              bool default_value)
{
  DRIZZLE_ROWS  *row;
  DRIZZLE_FIELD  *field,*result;
  uint32_t lengths[9];        /* Max of fields */

  field= result= (DRIZZLE_FIELD*) malloc((unsigned int) sizeof(*field)*fields);
  if (!result)
  {
    free_rows(data);        /* Free old data */
    return(0);
  }
  memset((char*) field, 0, (unsigned int) sizeof(DRIZZLE_FIELD)*fields);

  for (row= data->data; row ; row = row->next,field++)
  {
    unsigned char *pos;
    /* fields count may be wrong */
    assert((unsigned int) (field - result) < fields);
    cli_fetch_lengths(&lengths[0], row->data, default_value ? 8 : 7);
    field->catalog=   strdup((char*) row->data[0]);
    field->db=        strdup((char*) row->data[1]);
    field->table=     strdup((char*) row->data[2]);
    field->org_table= strdup((char*) row->data[3]);
    field->name=      strdup((char*) row->data[4]);
    field->org_name=  strdup((char*) row->data[5]);

    field->catalog_length=  lengths[0];
    field->db_length=    lengths[1];
    field->table_length=  lengths[2];
    field->org_table_length=  lengths[3];
    field->name_length=  lengths[4];
    field->org_name_length=  lengths[5];

    /* Unpack fixed length parts */
    pos= (unsigned char*) row->data[6];
    field->charsetnr= uint2korr(pos);
    field->length=  (unsigned int) uint4korr(pos+2);
    field->type=  (enum enum_field_types) pos[6];
    field->flags=  uint2korr(pos+7);
    field->decimals=  (unsigned int) pos[9];

    /* Test if field is Internal Number Format */
    if (((field->type <= DRIZZLE_TYPE_LONGLONG) &&
         (field->type != DRIZZLE_TYPE_TIMESTAMP)) ||
        (field->length == 14) ||
        (field->length == 8))
      field->flags|= NUM_FLAG;
    if (default_value && row->data[7])
    {
      field->def= (char *)malloc(lengths[7]);
      memcpy(field->def, row->data[7], lengths[7]);
      field->def_length= lengths[7];
    }
    else
      field->def=0;
    field->max_length= 0;
  }

  free_rows(data);        /* Free old data */
  return(result);
}

void free_rows(DRIZZLE_DATA *cur)
{
  if (cur)
  {
    if (cur->data != NULL)
    {
      struct st_drizzle_rows * row= cur->data;
      uint64_t x;
      for (x= 0; x< cur->rows; x++)
      {
        struct st_drizzle_rows * next_row= row->next;
        free(row);
        row= next_row;
      }
    }
    free((unsigned char*) cur);
  }
}
