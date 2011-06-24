/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <bitset>
#include <boost/program_options.hpp>
#include <boost/detail/atomic_count.hpp>
#include <drizzled/common_fwd.h>
#include <drizzled/global_buffer.h>
#include <drizzled/definitions.h>

struct passwd;

namespace drizzled {

extern boost::detail::atomic_count connection_count;
extern const char *load_default_groups[];
extern bool volatile select_thread_in_use;
extern bool volatile abort_loop;
extern bool volatile ready_to_exit;
extern bool opt_help;
extern bool opt_help_extended;
extern passwd *user_info;

extern global_buffer_constraint<uint64_t> global_sort_buffer;
extern global_buffer_constraint<uint64_t> global_join_buffer;
extern global_buffer_constraint<uint64_t> global_read_rnd_buffer;
extern global_buffer_constraint<uint64_t> global_read_buffer;

extern size_t transaction_message_threshold;

extern const char * const DRIZZLE_CONFIG_NAME;

boost::program_options::variables_map &getVariablesMap();

int init_thread_environment();
void init_server_components(module::Registry&);
int init_basic_variables(int argc, char **argv);
int init_remaining_variables(module::Registry&);

passwd *check_user(const char *user);
void set_user(const char *user, passwd *user_info_arg);
void clean_up(bool print_message);

} /* namespace drizzled */

