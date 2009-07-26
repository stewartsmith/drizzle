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


#ifndef DRIZZLED_TABLE_PROTO_H
#define DRIZZLED_TABLE_PROTO_H

int drizzle_write_proto_file(const std::string file_name,
                             drizzled::message::Table *table_proto);
int create_table_proto_file(const char *file_name,
                            const char *db,
                            const char *table_name,
                            drizzled::message::Table *table_proto,
                            HA_CREATE_INFO *create_info,
                            List<CreateField> &create_fields,
                            uint32_t keys,
                            KEY *key_info);

int parse_table_proto(Session *session,
                      drizzled::message::Table &table,
                      TableShare *share);

int fill_table_proto(drizzled::message::Table *table_proto,
                     const char *table_name,
                     List<CreateField> &create_fields,
                     HA_CREATE_INFO *create_info,
                     uint32_t keys,
                     KEY *key_info);

#endif
