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

#include <signal.h>

#include <cstdlib>
#include <cassert>
static bool volatile signal_thread_in_use= false;
extern "C" void drizzled_print_signal_warning(int sig);
extern "C" void drizzled_handle_segfault(int sig);
extern "C" void drizzled_end_thread_signal(int sig);

/*
  posix sigaction() based signal handler implementation
  On some systems, such as Mac OS X, sigset() results in flags
  such as SA_RESTART being set, and we want to make sure that no such
  flags are set.
*/
static inline void ignore_signal(int sig)
{
  /* Wow. There is a function sigaction which takes a pointer to a
    struct sigaction. */
  struct sigaction l_s;
  sigset_t l_set;
  sigemptyset(&l_set);

  assert(sig != 0);
  l_s.sa_handler= SIG_IGN;
  l_s.sa_mask= l_set;
  l_s.sa_flags= 0;
  int l_rc= sigaction(sig, &l_s, NULL);
  assert(l_rc == 0);
}

