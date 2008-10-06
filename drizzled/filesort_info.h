/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_FILESORT_INFO_ST_H
#define DRIZZLED_FILESORT_INFO_ST_H

/* Information on state of filesort */
struct filesort_info_st
{
  IO_CACHE *io_cache;           /* If sorted through filesort */
  unsigned char     **sort_keys;        /* Buffer for sorting keys */
  unsigned char     *buffpek;           /* Buffer for buffpek structures */
  uint      buffpek_len;        /* Max number of buffpeks in the buffer */
  unsigned char     *addon_buf;         /* Pointer to a buffer if sorted with fields */
  size_t    addon_length;       /* Length of the buffer */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, unsigned char *); /* To unpack back */
  unsigned char     *record_pointers;    /* If sorted in memory */
  ha_rows   found_records;      /* How many records in sort */
};

#endif /* DRIZZLED_FILESORT_INFO_ST_H */
