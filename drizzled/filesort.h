/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

/* 
  This is a "work in progress". The concept needs to be replicated throughout
  the code, but we will start with baby steps for the moment. To not incur
  cost until we are complete, for the moment it will do no allocation.

  This is mainly here so that it can be used in the SE interface for
  the time being.

  This will replace Table_ident.
  */

#pragma once

namespace drizzled {

class FileSort 
{
  Session &_session;

  uint32_t sortlength(SortField *sortorder, uint32_t s_length, bool *multi_byte_charset);
  sort_addon_field *get_addon_fields(Field **ptabfield, uint32_t sortlength, uint32_t *plength);
  ha_rows find_all_keys(SortParam *param, 
                        optimizer::SqlSelect *select,
                        unsigned char **sort_keys,
                        internal::io_cache_st *buffpek_pointers,
                        internal::io_cache_st *tempfile, internal::io_cache_st *indexfile);

  int merge_buffers(SortParam *param,internal::io_cache_st *from_file,
                    internal::io_cache_st *to_file, unsigned char *sort_buffer,
                    buffpek *lastbuff,
                    buffpek *Fb,
                    buffpek *Tb,int flag);

  int merge_index(SortParam *param,
                  unsigned char *sort_buffer,
                  buffpek *buffpek,
                  uint32_t maxbuffer,
                  internal::io_cache_st *tempfile,
                  internal::io_cache_st *outfile);

  int merge_many_buff(SortParam *param, unsigned char *sort_buffer,
                      buffpek *buffpek,
                      uint32_t *maxbuffer, internal::io_cache_st *t_file);

  uint32_t read_to_buffer(internal::io_cache_st *fromfile, buffpek *buffpek,
                          uint32_t sort_length);



public:

  FileSort(Session &arg);

  Session &getSession()
  {
    return _session;
  }

  ha_rows run(Table *table, SortField *sortorder, uint32_t s_length,
              optimizer::SqlSelect *select, ha_rows max_rows,
              bool sort_positions, ha_rows &examined_rows);

};

} /* namespace drizzled */

