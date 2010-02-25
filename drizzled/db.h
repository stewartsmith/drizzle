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


#ifndef DRIZZLED_DB_H
#define DRIZZLED_DB_H

#define DRIZZLE_DATA_DICTIONARY "data_dictionary"

#define MY_DB_OPT_FILE "db.opt"

namespace drizzled {

namespace message { class Schema; }

bool mysql_create_db(Session *session, message::Schema &schema_message, bool is_if_not_exists);
bool mysql_alter_db(Session *session, const message::Schema &schema_message);
bool mysql_rm_db(Session *session, char *db, bool if_exists);
bool mysql_change_db(Session *session, const LEX_STRING *new_db_name, bool force_switch);

} /* namespace drizzled */

#endif /* DRIZZLED_DB_H */
