/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (C) 2010 Monty Taylor
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

#ifndef DRIZZLED_DRIZZLED_H
#define DRIZZLED_DRIZZLED_H

#include <bitset>

#include "drizzled/atomics.h"

struct passwd;

namespace drizzled
{

namespace module
{
class Registry;
}

extern std::bitset<12> test_flags;
extern uint32_t max_used_connections;
extern atomic<uint32_t> connection_count;
extern bool calling_initgroups;
extern const char *load_default_groups[];
extern bool volatile select_thread_in_use;
extern bool volatile abort_loop;
extern bool volatile ready_to_exit;
extern bool opt_help;
extern bool opt_help_extended;
extern passwd *user_info;
extern char *drizzled_user;

extern const char * const DRIZZLE_CONFIG_NAME;

int init_server_components(module::Registry &modules);
int init_common_variables(const char *conf_file_name, int argc,
                          char **argv, const char **groups);

passwd *check_user(const char *user);
void set_user(const char *user, passwd *user_info_arg);
void clean_up(bool print_message);
void clean_up_mutexes(void);
bool drizzle_rm_tmp_tables();

} /* namespace drizzled */

#endif /* DRIZZLED_DRIZZLED_H */
