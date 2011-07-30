/* Copyright (C) 2002-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

#include <drizzled/common_fwd.h>

namespace drizzled {

#define GET_NO_ARG     1
#define GET_BOOL       2
#define GET_INT        3
#define GET_UINT       4
#define GET_LONG       5
#define GET_UINT32     6
#define GET_LL         7
#define GET_ULL        8
#define GET_STR        9
#define GET_STR_ALLOC 10
#define GET_SIZE      15
#define GET_UINT64    16
#define GET_ULONG_IS_FAIL 17

#define GET_TYPE_MASK	 127

enum loglevel {
   ERROR_LEVEL,
   WARNING_LEVEL,
   INFORMATION_LEVEL
};

enum get_opt_arg_type { NO_ARG, OPT_ARG, REQUIRED_ARG };

struct option
{
  const char *name;                     /* Name of the option */
  int        id;                        /* unique id or short option */
  const char *comment;                  /* option comment, for autom. --help */
  char      **value;                   /* The variable value */
  void      **u_max_value_unused;             /* The user def. max variable value */
  TYPELIB *typelib;           /* Pointer to possible values */
  uint32_t     var_type;
  get_opt_arg_type arg_type;
  int64_t   def_value;                 /* Default value */
  int64_t   min_value;                 /* Min allowed value */
  int64_t   max_value;                 /* Max allowed value */
  int64_t   sub_size;                  /* Subtract this from given value */
  int       block_size;                /* Value should be a mult. of this */
  void*     app_type_unused;
};


typedef int (* my_get_one_option) (int, const struct option *, char * );
typedef void (* my_error_reporter) (enum loglevel level, const char *format, ... );
typedef char ** (*getopt_get_addr_func)(const char *, uint32_t, const struct option *);

extern int handle_options (int *argc, char ***argv, const option*, my_get_one_option);
extern void my_cleanup_options(const struct option *options);
extern void my_print_help(const struct option *options);
extern void my_print_variables(const struct option *options);

uint64_t getopt_ull_limit_value(uint64_t num, const option&, bool* fix);
int64_t getopt_ll_limit_value(int64_t, const option&, bool* fix);
bool getopt_compare_strings(const char *s, const char *t, uint32_t length);

} /* namespace drizzled */


