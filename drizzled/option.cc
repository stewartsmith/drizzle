/* Copyright (C) 2002-2006 MySQL AB

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

#include <config.h>
#include <drizzled/definitions.h>
#include <drizzled/charset_info.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/gettext.h>

#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/error.h>
#include <drizzled/option.h>
#include <drizzled/typelib.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <algorithm>

using namespace std;
namespace drizzled
{

  typedef void (*init_func_p)(const struct option *option, char **variable,
      int64_t value);

  void default_reporter(enum loglevel level, const char *format, ...);
  my_error_reporter my_getopt_error_reporter= &default_reporter;

  static void init_variables(const struct option *options,
      init_func_p init_one_value);
  static void fini_one_value(const struct option *option, char **variable,
      int64_t value);

  /*
     The following three variables belong to same group and the number and
     order of their arguments must correspond to each other.
   */
  static const uint32_t special_opt_prefix_lengths[]=
  { 4,      7,         6,        7,         5,      0};
  enum enum_special_opt
  { OPT_SKIP, OPT_DISABLE, OPT_ENABLE, OPT_MAXIMUM, OPT_LOOSE};

  char *disabled_my_option= (char*) "0";

  /*
     This is a flag that can be set in client programs. 1 means that
     my_getopt will skip over options it does not know how to handle.
   */

  bool my_getopt_skip_unknown= 0;

  void default_reporter(enum loglevel level, const char *format, ...)
  {
    va_list args;
    va_start(args, format);
    if (level == WARNING_LEVEL)
      fprintf(stderr, "%s", _("Warning: "));
    else if (level == INFORMATION_LEVEL)
      fprintf(stderr, "%s", _("Info: "));
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);
  }

static getopt_get_addr_func getopt_get_addr;

  /*
function: compare_strings

Works like strncmp, other than 1.) considers '-' and '_' the same.
2.) Returns -1 if strings differ, 0 if they are equal
   */

  bool getopt_compare_strings(const char *s, const char *t,
      uint32_t length)
  {
    char const *end= s + length;
    for (;s != end ; s++, t++)
    {
      if ((*s != '-' ? *s : '_') != (*t != '-' ? *t : '_'))
        return 1;
    }
    return 0;
  }

  /*
function: getopt_ll_limit_value

Applies min/max/block_size to a numeric value of an option.
Returns "fixed" value.
   */

  int64_t getopt_ll_limit_value(int64_t num, const struct option *optp,
      bool *fix)
  {
    int64_t old= num;
    bool adjusted= false;
    char buf1[255], buf2[255];
    uint64_t block_size= (optp->block_size ? (uint64_t) optp->block_size : 1L);

    if (num > 0 && ((uint64_t) num > (uint64_t) optp->max_value) &&
        optp->max_value) /* if max value is not set -> no upper limit */
    {
      num= (uint64_t) optp->max_value;
      adjusted= true;
    }

    switch ((optp->var_type & GET_TYPE_MASK)) {
      case GET_INT:
        if (num > (int64_t) INT_MAX)
        {
          num= ((int64_t) INT_MAX);
          adjusted= true;
        }
        break;
      case GET_LONG:
        if (num > (int64_t) INT32_MAX)
        {
          num= ((int64_t) INT32_MAX);
          adjusted= true;
        }
        break;
      default:
        assert((optp->var_type & GET_TYPE_MASK) == GET_LL);
        break;
    }

    num= ((num - optp->sub_size) / block_size);
    num= (int64_t) (num * block_size);

    if (num < optp->min_value)
    {
      num= optp->min_value;
      adjusted= true;
    }

    if (fix)
      *fix= adjusted;
    else if (adjusted)
      my_getopt_error_reporter(WARNING_LEVEL,
          "option '%s': signed value %s adjusted to %s",
          optp->name, internal::llstr(old, buf1), internal::llstr(num, buf2));
    return num;
  }

  /*
function: getopt_ull

This is the same as getopt_ll, but is meant for uint64_t
values.
   */

  uint64_t getopt_ull_limit_value(uint64_t num, const struct option *optp,
      bool *fix)
  {
    bool adjusted= false;
    uint64_t old= num;
    char buf1[255], buf2[255];

    if ((uint64_t) num > (uint64_t) optp->max_value &&
        optp->max_value) /* if max value is not set -> no upper limit */
    {
      num= (uint64_t) optp->max_value;
      adjusted= true;
    }

    switch ((optp->var_type & GET_TYPE_MASK)) {
      case GET_UINT:
        if (num > (uint64_t) UINT_MAX)
        {
          num= ((uint64_t) UINT_MAX);
          adjusted= true;
        }
        break;
      case GET_UINT32:
      case GET_ULONG_IS_FAIL:
        if (num > (uint64_t) UINT32_MAX)
        {
          num= ((uint64_t) UINT32_MAX);
          adjusted= true;
        }
        break;
      case GET_SIZE:
        if (num > (uint64_t) SIZE_MAX)
        {
          num= ((uint64_t) SIZE_MAX);
          adjusted= true;
        }
        break;
      default:
        assert(((optp->var_type & GET_TYPE_MASK) == GET_ULL)
            || ((optp->var_type & GET_TYPE_MASK) == GET_UINT64));
        break;
    }

    if (optp->block_size > 1)
    {
      num/= (uint64_t) optp->block_size;
      num*= (uint64_t) optp->block_size;
    }

    if (num < (uint64_t) optp->min_value)
    {
      num= (uint64_t) optp->min_value;
      adjusted= true;
    }

    if (fix)
      *fix= adjusted;
    else if (adjusted)
      my_getopt_error_reporter(WARNING_LEVEL,
          "option '%s': unsigned value %s adjusted to %s",
          optp->name, internal::ullstr(old, buf1), internal::ullstr(num, buf2));

    return num;
  }

  /*
     Init one value to it's default values

     SYNOPSIS
     init_one_value()
     option		Option to initialize
     value		Pointer to variable
   */

  static void fini_one_value(const struct option *option, char **variable,
      int64_t)
  {
    switch ((option->var_type & GET_TYPE_MASK)) {
      case GET_STR_ALLOC:
        free((*(char**) variable));
        *((char**) variable)= NULL;
        break;
      default: /* dummy default to avoid compiler warnings */
        break;
    }
    return;
  }


  void my_cleanup_options(const struct option *options)
  {
    init_variables(options, fini_one_value);
  }


  /*
     initialize all variables to their default values

     SYNOPSIS
     init_variables()
     options		Array of options

     NOTES
     We will initialize the value that is pointed to by options->value.
     If the value is of type GET_ASK_ADDR, we will also ask for the address
     for a value and initialize.
   */

  static void init_variables(const struct option *options,
      init_func_p init_one_value)
  {
    for (; options->name; options++)
    {
      char* *variable;
      /*
         We must set u_max_value first as for some variables
         options->u_max_value == options->value and in this case we want to
         set the value to default value.
       */
      if (options->u_max_value)
        init_one_value(options, options->u_max_value, options->max_value);
      if (options->value)
        init_one_value(options, options->value, options->def_value);
      if (options->var_type & GET_ASK_ADDR &&
          (variable= (*getopt_get_addr)("", 0, options)))
        init_one_value(options, variable, options->def_value);
    }
    return;
  }


  /*
function: my_print_options

Print help for all options and variables.
   */

  void my_print_help(const struct option *options)
  {
    uint32_t col, name_space= 22, comment_space= 57;
    const char *line_end;
    const struct option *optp;

    for (optp= options; optp->id; optp++)
    {
      if (optp->id < 256)
      {
        printf("  -%c%s", optp->id, strlen(optp->name) ? ", " : "  ");
        col= 6;
      }
      else
      {
        printf("  ");
        col= 2;
      }
      if (strlen(optp->name))
      {
        printf("--%s", optp->name);
        col+= 2 + (uint32_t) strlen(optp->name);
        if ((optp->var_type & GET_TYPE_MASK) == GET_STR ||
            (optp->var_type & GET_TYPE_MASK) == GET_STR_ALLOC)
        {
          printf("%s=name%s ", optp->arg_type == OPT_ARG ? "[" : "",
              optp->arg_type == OPT_ARG ? "]" : "");
          col+= (optp->arg_type == OPT_ARG) ? 8 : 6;
        }
        else if ((optp->var_type & GET_TYPE_MASK) == GET_NO_ARG ||
            (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
        {
          putchar(' ');
          col++;
        }
        else
        {
          printf("%s=#%s ", optp->arg_type == OPT_ARG ? "[" : "",
              optp->arg_type == OPT_ARG ? "]" : "");
          col+= (optp->arg_type == OPT_ARG) ? 5 : 3;
        }
        if (col > name_space && optp->comment && *optp->comment)
        {
          putchar('\n');
          col= 0;
        }
      }
      for (; col < name_space; col++)
        putchar(' ');
      if (optp->comment && *optp->comment)
      {
        const char *comment= _(optp->comment), *end= strchr(comment, '\0');

        while ((uint32_t) (end - comment) > comment_space)
        {
          for (line_end= comment + comment_space; *line_end != ' '; line_end--)
          {}
          for (; comment != line_end; comment++)
            putchar(*comment);
          comment++; /* skip the space, as a newline will take it's place now */
          putchar('\n');
          for (col= 0; col < name_space; col++)
            putchar(' ');
        }
        printf("%s", comment);
      }
      putchar('\n');
      if ((optp->var_type & GET_TYPE_MASK) == GET_NO_ARG ||
          (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
      {
        if (optp->def_value != 0)
        {
          printf(_("%*s(Defaults to on; use --skip-%s to disable.)\n"), name_space, "", optp->name);
        }
      }
    }
  }


} /* namespace drizzled */
