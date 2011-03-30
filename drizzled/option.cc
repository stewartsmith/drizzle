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

  static int findopt(char *optpat, uint32_t length,
      const struct option **opt_res,
      char **ffname);
  static int64_t getopt_ll(char *arg, const struct option *optp, int *err);
  static uint64_t getopt_ull(char *arg, const struct option *optp,
      int *err);
  static size_t getopt_size(char *arg, const struct option *optp, int *err);
  static double getopt_double(char *arg, const struct option *optp, int *err);
  static void init_variables(const struct option *options,
      init_func_p init_one_value);
  static void init_one_value(const struct option *option, char **variable,
      int64_t value);
  static void fini_one_value(const struct option *option, char **variable,
      int64_t value);
  static int setval(const struct option *opts, char* *value, char *argument,
      bool set_maximum_value);
  static char *check_struct_option(char *cur_arg, char *key_name);

  /*
     The following three variables belong to same group and the number and
     order of their arguments must correspond to each other.
   */
  static const char *special_opt_prefix[]=
  {"skip", "disable", "enable", "maximum", "loose", 0};
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

  /*
function: handle_options

Sort options; put options first, until special end of options (--), or
until end of argv. Parse options; check that the given option matches with
one of the options in struct 'option', return error in case of ambiguous
or unknown option. Check that option was given an argument if it requires
one. Call function 'get_one_option()' once for each option.
   */

  static getopt_get_addr_func getopt_get_addr;

  int handle_options(int *argc, char ***argv,
      const struct option *longopts,
      my_get_one_option get_one_option)
  {
    uint32_t opt_found, argvpos= 0, length;
    bool end_of_options= 0, must_be_var, set_maximum_value=false,
         option_is_loose;
    char **pos, **pos_end, *optend, *prev_found=NULL,
         *opt_str, key_name[FN_REFLEN];
    const struct option *optp;
    char* *value;
    int error, i;

    /* handle_options() assumes arg0 (program name) always exists */
    assert(argc && *argc >= 1);
    assert(argv && *argv);
    (*argc)--; /* Skip the program name */
    (*argv)++; /*      --- || ----      */
    init_variables(longopts, init_one_value);

    for (pos= *argv, pos_end=pos+ *argc; pos != pos_end ; pos++)
    {
      char **first= pos;
      char *cur_arg= *pos;
      if (cur_arg[0] == '-' && cur_arg[1] && !end_of_options) /* must be opt */
      {
        char *argument=    0;
        must_be_var=       0;
        set_maximum_value= 0;
        option_is_loose=   0;

        cur_arg++;  /* skip '-' */
        if (*cur_arg == '-' || *cur_arg == 'O') /* check for long option, */
        {                                       /* --set-variable, or -O  */
          if (*cur_arg == 'O')
          {
            must_be_var= 1;

            if (!(*++cur_arg))	/* If not -Ovar=# */
            {
              /* the argument must be in next argv */
              if (!*++pos)
              {
                my_getopt_error_reporter(ERROR_LEVEL,
                    "%s: Option '-O' requires an argument",
                    internal::my_progname);
                return EXIT_ARGUMENT_REQUIRED;
              }
              cur_arg= *pos;
              (*argc)--;
            }
          }
          else if (!getopt_compare_strings(cur_arg, "-set-variable", 13))
          {
            must_be_var= 1;
            if (cur_arg[13] == '=')
            {
              cur_arg+= 14;
              if (!*cur_arg)
              {
                my_getopt_error_reporter(ERROR_LEVEL,
                    "%s: Option '--set-variable' requires an argument",
                    internal::my_progname);
                return EXIT_ARGUMENT_REQUIRED;
              }
            }
            else if (cur_arg[14]) /* garbage, or another option. break out */
              must_be_var= 0;
            else
            {
              /* the argument must be in next argv */
              if (!*++pos)
              {
                my_getopt_error_reporter(ERROR_LEVEL,
                    "%s: Option '--set-variable' requires an argument",
                    internal::my_progname);
                return EXIT_ARGUMENT_REQUIRED;
              }
              cur_arg= *pos;
              (*argc)--;
            }
          }
          else if (!must_be_var)
          {
            if (!*++cur_arg)	/* skip the double dash */
            {
              /* '--' means end of options, look no further */
              end_of_options= 1;
              (*argc)--;
              continue;
            }
          }
          opt_str= check_struct_option(cur_arg, key_name);
          optend= strchr(opt_str, '=');
          if (optend != NULL)
          {
            length= (uint32_t) (optend - opt_str);
            optend++;
          }
          else
          {
            length= static_cast<uint32_t>(strlen(opt_str));
            optend= 0;
          }

          /*
             Find first the right option. Return error in case of an ambiguous,
             or unknown option
           */
          optp= longopts;
          if (!(opt_found= findopt(opt_str, length, &optp, &prev_found)))
          {
            /*
               Didn't find any matching option. Let's see if someone called
               option with a special option prefix
             */
            if (!must_be_var)
            {
              if (optend)
                must_be_var= 1; /* option is followed by an argument */
              for (i= 0; special_opt_prefix[i]; i++)
              {
                if (!getopt_compare_strings(special_opt_prefix[i], opt_str,
                      special_opt_prefix_lengths[i]) &&
                    (opt_str[special_opt_prefix_lengths[i]] == '-' ||
                     opt_str[special_opt_prefix_lengths[i]] == '_'))
                {
                  /*
                     We were called with a special prefix, we can reuse opt_found
                   */
                  opt_str+= special_opt_prefix_lengths[i] + 1;
                  length-= special_opt_prefix_lengths[i] + 1;
                  if (i == OPT_LOOSE)
                    option_is_loose= 1;
                  if ((opt_found= findopt(opt_str, length, &optp, &prev_found)))
                  {
                    if (opt_found > 1)
                    {
                      my_getopt_error_reporter(ERROR_LEVEL,
                          "%s: ambiguous option '--%s-%s' (--%s-%s)",
                          internal::my_progname,
                          special_opt_prefix[i],
                          cur_arg, special_opt_prefix[i],
                          prev_found);
                      return EXIT_AMBIGUOUS_OPTION;
                    }
                    switch (i) {
                      case OPT_SKIP:
                      case OPT_DISABLE: /* fall through */
                        /*
                           double negation is actually enable again,
                           for example: --skip-option=0 -> option = true
                         */
                        optend= (optend && *optend == '0' && !(*(optend + 1))) ?
                          (char*) "1" : disabled_my_option;
                        break;
                      case OPT_ENABLE:
                        optend= (optend && *optend == '0' && !(*(optend + 1))) ?
                          disabled_my_option : (char*) "1";
                        break;
                      case OPT_MAXIMUM:
                        set_maximum_value= true;
                        must_be_var= true;
                        break;
                    }
                    break; /* break from the inner loop, main loop continues */
                  }
                  i= -1; /* restart the loop */
                }
              }
            }
            if (!opt_found)
            {
              if (my_getopt_skip_unknown)
              {
                /*
                   preserve all the components of this unknown option, this may
                   occurr when the user provides options like: "-O foo" or
                   "--set-variable foo" (note that theres a space in there)
                   Generally, these kind of options are to be avoided
                 */
                do {
                  (*argv)[argvpos++]= *first++;
                } while (first <= pos);
                continue;
              }
              if (must_be_var)
              {
                my_getopt_error_reporter(option_is_loose ?
                    WARNING_LEVEL : ERROR_LEVEL,
                    "%s: unknown variable '%s'",
                    internal::my_progname, cur_arg);
                if (!option_is_loose)
                  return EXIT_UNKNOWN_VARIABLE;
              }
              else
              {
                my_getopt_error_reporter(option_is_loose ?
                    WARNING_LEVEL : ERROR_LEVEL,
                    "%s: unknown option '--%s'",
                    internal::my_progname, cur_arg);
                if (!option_is_loose)
                  return EXIT_UNKNOWN_OPTION;
              }
              if (option_is_loose)
              {
                (*argc)--;
                continue;
              }
            }
          }
          if (opt_found > 1)
          {
            if (must_be_var)
            {
              my_getopt_error_reporter(ERROR_LEVEL,
                  "%s: variable prefix '%s' is not unique",
                  internal::my_progname, opt_str);
              return EXIT_VAR_PREFIX_NOT_UNIQUE;
            }
            else
            {
              my_getopt_error_reporter(ERROR_LEVEL,
                  "%s: ambiguous option '--%s' (%s, %s)",
                  internal::my_progname, opt_str, prev_found,
                  optp->name);
              return EXIT_AMBIGUOUS_OPTION;
            }
          }
          if ((optp->var_type & GET_TYPE_MASK) == GET_DISABLED)
          {
            fprintf(stderr,
                _("%s: %s: Option '%s' used, but is disabled\n"),
                internal::my_progname,
                option_is_loose ? _("WARNING") : _("ERROR"), opt_str);
            if (option_is_loose)
            {
              (*argc)--;
              continue;
            }
            return EXIT_OPTION_DISABLED;
          }
          if (must_be_var && (optp->var_type & GET_TYPE_MASK) == GET_NO_ARG)
          {
            my_getopt_error_reporter(ERROR_LEVEL,
                "%s: option '%s' cannot take an argument",
                internal::my_progname, optp->name);
            return EXIT_NO_ARGUMENT_ALLOWED;
          }
          value= optp->var_type & GET_ASK_ADDR ?
            (*getopt_get_addr)(key_name, (uint32_t) strlen(key_name), optp) : optp->value;

          if (optp->arg_type == NO_ARG)
          {
            if (optend && (optp->var_type & GET_TYPE_MASK) != GET_BOOL)
            {
              my_getopt_error_reporter(ERROR_LEVEL,
                  "%s: option '--%s' cannot take an argument",
                  internal::my_progname, optp->name);
              return EXIT_NO_ARGUMENT_ALLOWED;
            }
            if ((optp->var_type & GET_TYPE_MASK) == GET_BOOL)
            {
              /*
                 Set bool to 1 if no argument or if the user has used
                 --enable-'option-name'.
               *optend was set to '0' if one used --disable-option
               */
              (*argc)--;
              if (!optend || *optend == '1' ||
                  !my_strcasecmp(&my_charset_utf8_general_ci, optend, "true"))
                *((bool*) value)= (bool) 1;
              else if (*optend == '0' ||
                  !my_strcasecmp(&my_charset_utf8_general_ci, optend, "false"))
                *((bool*) value)= (bool) 0;
              else
              {
                my_getopt_error_reporter(WARNING_LEVEL,
                    "%s: ignoring option '--%s' due to "
                    "invalid value '%s'",
                    internal::my_progname,
                    optp->name, optend);
                continue;
              }
              error= get_one_option(optp->id, optp, *((bool*) value) ?
                                    (char*) "1" : disabled_my_option);
              if (error != 0)
                return error;
              else
                continue;
            }
            argument= optend;
          }
          else if (optp->arg_type == OPT_ARG &&
              (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
          {
            if (optend == disabled_my_option)
              *((bool*) value)= (bool) 0;
            else
            {
              if (!optend) /* No argument -> enable option */
                *((bool*) value)= (bool) 1;
              else
                argument= optend;
            }
          }
          else if (optp->arg_type == REQUIRED_ARG && !optend)
          {
            /* Check if there are more arguments after this one */
            if (!*++pos)
            {
              my_getopt_error_reporter(ERROR_LEVEL,
                  "%s: option '--%s' requires an argument",
                  internal::my_progname, optp->name);
              return EXIT_ARGUMENT_REQUIRED;
            }
            argument= *pos;
            (*argc)--;
          }
          else
            argument= optend;
        }
        else  /* must be short option */
        {
          for (optend= cur_arg; *optend; optend++)
          {
            opt_found= 0;
            for (optp= longopts; optp->id; optp++)
            {
              if (optp->id == (int) (unsigned char) *optend)
              {
                /* Option recognized. Find next what to do with it */
                opt_found= 1;
                if ((optp->var_type & GET_TYPE_MASK) == GET_DISABLED)
                {
                  fprintf(stderr,
                      _("%s: ERROR: Option '-%c' used, but is disabled\n"),
                      internal::my_progname, optp->id);
                  return EXIT_OPTION_DISABLED;
                }
                if ((optp->var_type & GET_TYPE_MASK) == GET_BOOL &&
                    optp->arg_type == NO_ARG)
                {
                  *((bool*) optp->value)= (bool) 1;
                  error= get_one_option(optp->id, optp, argument);
                  if (error != 0)
                    return error;
                  else
                    continue;

                }
                else if (optp->arg_type == REQUIRED_ARG ||
                    optp->arg_type == OPT_ARG)
                {
                  if (*(optend + 1))
                  {
                    /* The rest of the option is option argument */
                    argument= optend + 1;
                    /* This is in effect a jump out of the outer loop */
                    optend= (char*) " ";
                  }
                  else
                  {
                    if (optp->arg_type == OPT_ARG)
                    {
                      if (optp->var_type == GET_BOOL)
                        *((bool*) optp->value)= (bool) 1;
                      error= get_one_option(optp->id, optp, argument);
                      if (error != 0)
                        return error;
                      else
                        continue;
                    }
                    /* Check if there are more arguments after this one */
                    if (!pos[1])
                    {
                      my_getopt_error_reporter(ERROR_LEVEL,
                          "%s: option '-%c' requires "
                          "an argument",
                          internal::my_progname, optp->id);
                      return EXIT_ARGUMENT_REQUIRED;
                    }
                    argument= *++pos;
                    (*argc)--;
                    /* the other loop will break, because *optend + 1 == 0 */
                  }
                }
                if ((error= setval(optp, optp->value, argument,
                        set_maximum_value)))
                {
                  my_getopt_error_reporter(ERROR_LEVEL,
                      "%s: Error while setting value '%s' "
                      "to '%s'",
                      internal::my_progname,
                      argument, optp->name);
                  return error;
                }
                error= get_one_option(optp->id, optp, argument);
                if (error != 0)
                  return error;
                else
                  break;
              }
            }
            if (!opt_found)
            {
              my_getopt_error_reporter(ERROR_LEVEL,
                  "%s: unknown option '-%c'",
                  internal::my_progname, *optend);
              return EXIT_UNKNOWN_OPTION;
            }
          }
          (*argc)--; /* option handled (short), decrease argument count */
          continue;
        }
        if ((error= setval(optp, value, argument, set_maximum_value)))
        {
          my_getopt_error_reporter(ERROR_LEVEL,
              "%s: Error while setting value '%s' to '%s'",
              internal::my_progname, argument, optp->name);
          return error;
        }
        error= get_one_option(optp->id, optp, argument);
        if (error != 0)
          return error;

        (*argc)--; /* option handled (short or long), decrease argument count */
      }
      else /* non-option found */
        (*argv)[argvpos++]= cur_arg;
    }
    /*
       Destroy the first, already handled option, so that programs that look
       for arguments in 'argv', without checking 'argc', know when to stop.
       Items in argv, before the destroyed one, are all non-option -arguments
       to the program, yet to be (possibly) handled.
     */
    (*argv)[argvpos]= 0;
    return 0;
  }


  /*
function: check_struct_option

Arguments: Current argument under processing from argv and a variable
where to store the possible key name.

Return value: In case option is a struct option, returns a pointer to
the current argument at the position where the struct option (key_name)
ends, the next character after the dot. In case argument is not a struct
option, returns a pointer to the argument.

key_name will hold the name of the key, or 0 if not found.
   */

  static char *check_struct_option(char *cur_arg, char *key_name)
  {
    char *ptr, *end;

    ptr= NULL; //Options with '.' are now supported.
    end= strrchr(cur_arg, '=');

    /*
       If the first dot is after an equal sign, then it is part
       of a variable value and the option is not a struct option.
       Also, if the last character in the string before the ending
       NULL, or the character right before equal sign is the first
       dot found, the option is not a struct option.
     */
    if ((ptr != NULL) && (end != NULL) && (end - ptr > 1))
    {
      uint32_t len= (uint32_t) (ptr - cur_arg);
      set_if_smaller(len, (uint32_t)FN_REFLEN-1);
      strncpy(key_name, cur_arg, len);
      return ++ptr;
    }
    key_name[0]= 0;
    return cur_arg;
  }

  /*
function: setval

Arguments: opts, argument
Will set the option value to given value
   */

  static int setval(const struct option *opts, char **value, char *argument,
      bool set_maximum_value)
  {
    int err= 0;

    if (value && argument)
    {
      char* *result_pos= ((set_maximum_value) ?
          opts->u_max_value : value);

      if (!result_pos)
        return EXIT_NO_PTR_TO_VARIABLE;

      switch ((opts->var_type & GET_TYPE_MASK)) {
        case GET_BOOL: /* If argument differs from 0, enable option, else disable */
          *((bool*) result_pos)= (bool) atoi(argument) != 0;
          break;
        case GET_INT:
          *((int32_t*) result_pos)= (int) getopt_ll(argument, opts, &err);
          break;
        case GET_UINT:
        case GET_UINT32:
          *((uint32_t*) result_pos)= (uint32_t) getopt_ull(argument, opts, &err);
          break;
        case GET_ULONG_IS_FAIL:
          *((ulong*) result_pos)= (ulong) getopt_ull(argument, opts, &err);
          break;
        case GET_LONG:
          *((long*) result_pos)= (long) getopt_ll(argument, opts, &err);
          break;
        case GET_LL:
          *((int64_t*) result_pos)= getopt_ll(argument, opts, &err);
          break;
        case GET_ULL:
        case GET_UINT64:
          *((uint64_t*) result_pos)= getopt_ull(argument, opts, &err);
          break;
        case GET_SIZE:
          *((size_t*) result_pos)= getopt_size(argument, opts, &err);
          break;
        case GET_DOUBLE:
          *((double*) result_pos)= getopt_double(argument, opts, &err);
          break;
        case GET_STR:
          *((char**) result_pos)= argument;
          break;
        case GET_STR_ALLOC:
          if ((*((char**) result_pos)))
            free((*(char**) result_pos));
          if (!(*((char**) result_pos)= strdup(argument)))
            return EXIT_OUT_OF_MEMORY;
          break;
        case GET_ENUM:
          if (((*(int*)result_pos)= opts->typelib->find_type(argument, 2) - 1) < 0)
            return EXIT_ARGUMENT_INVALID;
          break;
        case GET_SET:
          *((uint64_t*)result_pos)= opts->typelib->find_typeset(argument, &err);
          if (err)
            return EXIT_ARGUMENT_INVALID;
          break;
        default:    /* dummy default to avoid compiler warnings */
          break;
      }
      if (err)
        return EXIT_UNKNOWN_SUFFIX;
    }
    return 0;
  }


  /*
     Find option

     SYNOPSIS
     findopt()
     optpat	Prefix of option to find (with - or _)
     length	Length of optpat
     opt_res	Options
     ffname	Place for pointer to first found name

     IMPLEMENTATION
     Go through all options in the option struct. Return number
     of options found that match the pattern and in the argument
     list the option found, if any. In case of ambiguous option, store
     the name in ffname argument

     RETURN
     0    No matching options
#   Number of matching options
ffname points to first matching option
   */

  static int findopt(char *optpat, uint32_t length,
      const struct option **opt_res,
      char **ffname)
  {
    uint32_t count;
    struct option *opt= (struct option *) *opt_res;

    for (count= 0; opt->name; opt++)
    {
      if (!getopt_compare_strings(opt->name, optpat, length)) /* match found */
      {
        (*opt_res)= opt;
        if (!opt->name[length])		/* Exact match */
          return 1;
        if (!count)
        {
          count= 1;
          *ffname= (char *) opt->name;	/* We only need to know one prev */
        }
        else if (strcmp(*ffname, opt->name))
        {
          /*
             The above test is to not count same option twice
             (see mysql.cc, option "help")
           */
          count++;
        }
      }
    }
    return count;
  }


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
function: eval_num_suffix

Transforms a number with a suffix to real number. Suffix can
be k|K for kilo, m|M for mega or g|G for giga.
   */

  static int64_t eval_num_suffix(char *argument, int *error, char *option_name)
  {
    char *endchar;
    int64_t num;

    *error= 0;
    errno= 0;
    num= strtoll(argument, &endchar, 10);
    if (errno == ERANGE)
    {
      my_getopt_error_reporter(ERROR_LEVEL,
          "Incorrect integer value: '%s'", argument);
      *error= 1;
      return 0;
    }
    if (*endchar == 'k' || *endchar == 'K')
      num*= 1024L;
    else if (*endchar == 'm' || *endchar == 'M')
      num*= 1024L * 1024L;
    else if (*endchar == 'g' || *endchar == 'G')
      num*= 1024L * 1024L * 1024L;
    else if (*endchar)
    {
      fprintf(stderr,
          _("Unknown suffix '%c' used for variable '%s' (value '%s')\n"),
          *endchar, option_name, argument);
      *error= 1;
      return 0;
    }
    return num;
  }

  /*
function: getopt_ll

Evaluates and returns the value that user gave as an argument
to a variable. Recognizes (case insensitive) K as KILO, M as MEGA
and G as GIGA bytes. Some values must be in certain blocks, as
defined in the given option struct, this function will check
that those values are honored.
In case of an error, set error value in *err.
   */

  static int64_t getopt_ll(char *arg, const struct option *optp, int *err)
  {
    int64_t num=eval_num_suffix(arg, err, (char*) optp->name);
    return getopt_ll_limit_value(num, optp, NULL);
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

  static uint64_t getopt_ull(char *arg, const struct option *optp, int *err)
  {
    uint64_t num= eval_num_suffix(arg, err, (char*) optp->name);
    return getopt_ull_limit_value(num, optp, NULL);
  }


  static size_t getopt_size(char *arg, const struct option *optp, int *err)
  {
    return (size_t)getopt_ull(arg, optp, err);
  }



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
     Get double value withing ranges

     Evaluates and returns the value that user gave as an argument to a variable.

     RETURN
     decimal value of arg

     In case of an error, prints an error message and sets *err to
     EXIT_ARGUMENT_INVALID.  Otherwise err is not touched
   */

  static double getopt_double(char *arg, const struct option *optp, int *err)
  {
    double num;
    int error;
    char *end= arg + 1000;                     /* Big enough as *arg is \0 terminated */
    num= internal::my_strtod(arg, &end, &error);
    if (end[0] != 0 || error)
    {
      fprintf(stderr,
          _("%s: ERROR: Invalid decimal value for option '%s'\n"),
          internal::my_progname, optp->name);
      *err= EXIT_ARGUMENT_INVALID;
      return 0.0;
    }
    if (optp->max_value && num > (double) optp->max_value)
      num= (double) optp->max_value;
    return max(num, (double) optp->min_value);
  }

  /*
     Init one value to it's default values

     SYNOPSIS
     init_one_value()
     option		Option to initialize
     value		Pointer to variable
   */

  static void init_one_value(const struct option *option, char** variable,
      int64_t value)
  {
    switch ((option->var_type & GET_TYPE_MASK)) {
      case GET_BOOL:
        *((bool*) variable)= (bool) value;
        break;
      case GET_INT:
        *((int*) variable)= (int) value;
        break;
      case GET_UINT:
      case GET_ENUM:
        *((uint*) variable)= (uint32_t) value;
        break;
      case GET_LONG:
        *((long*) variable)= (long) value;
        break;
      case GET_UINT32:
        *((uint32_t*) variable)= (uint32_t) value;
        break;
      case GET_ULONG_IS_FAIL:
        *((ulong*) variable)= (ulong) value;
        break;
      case GET_LL:
        *((int64_t*) variable)= (int64_t) value;
        break;
      case GET_SIZE:
        *((size_t*) variable)= (size_t) value;
        break;
      case GET_ULL:
      case GET_SET:
      case GET_UINT64:
        *((uint64_t*) variable)=  (uint64_t) value;
        break;
      case GET_DOUBLE:
        *((double*) variable)=  (double) value;
        break;
      case GET_STR:
        /*
           Do not clear variable value if it has no default value.
           The default value may already be set.
NOTE: To avoid compiler warnings, we first cast int64_t to intptr_t,
so that the value has the same size as a pointer.
         */
        if ((char*) (intptr_t) value)
          *((char**) variable)= (char*) (intptr_t) value;
        break;
      case GET_STR_ALLOC:
        /*
           Do not clear variable value if it has no default value.
           The default value may already be set.
NOTE: To avoid compiler warnings, we first cast int64_t to intptr_t,
so that the value has the same size as a pointer.
         */
        if ((char*) (intptr_t) value)
        {
          free((*(char**) variable));
          char *tmpptr= strdup((char *) (intptr_t) value);
          if (tmpptr != NULL)
            *((char**) variable)= tmpptr;
        }
        break;
      default: /* dummy default to avoid compiler warnings */
        break;
    }
    return;
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


  /*
function: my_print_options

Print variables.
   */

  void my_print_variables(const struct option *options)
  {
    uint32_t name_space= 34, length, nr;
    uint64_t bit, llvalue;
    char buff[255];
    const struct option *optp;

    printf(_("\nVariables (--variable-name=value)\n"
          "and boolean options {false|true}  Value (after reading options)\n"
          "--------------------------------- -----------------------------\n"));
    for (optp= options; optp->id; optp++)
    {
      char* *value= (optp->var_type & GET_ASK_ADDR ?
          (*getopt_get_addr)("", 0, optp) : optp->value);
      if (value)
      {
        printf("%s ", optp->name);
        length= (uint32_t) strlen(optp->name)+1;
        for (; length < name_space; length++)
          putchar(' ');
        switch ((optp->var_type & GET_TYPE_MASK)) {
          case GET_SET:
            if (!(llvalue= *(uint64_t*) value))
              printf("%s\n", _("(No default value)"));
            else
              for (nr= 0, bit= 1; llvalue && nr < optp->typelib->count; nr++, bit<<=1)
              {
                if (!(bit & llvalue))
                  continue;
                llvalue&= ~bit;
                printf( llvalue ? "%s," : "%s\n", optp->typelib->get_type(nr));
              }
            break;
          case GET_ENUM:
            printf("%s\n", optp->typelib->get_type(*(uint*) value));
            break;
          case GET_STR:
          case GET_STR_ALLOC:                    /* fall through */
            printf("%s\n", *((char**) value) ? *((char**) value) :
                _("(No default value)"));
            break;
          case GET_BOOL:
            printf("%s\n", *((bool*) value) ? _("true") : _("false"));
            break;
          case GET_INT:
            printf("%d\n", *((int*) value));
            break;
          case GET_UINT:
            printf("%d\n", *((uint*) value));
            break;
          case GET_LONG:
            printf("%ld\n", *((long*) value));
            break;
          case GET_UINT32:
            printf("%u\n", *((uint32_t*) value));
            break;
          case GET_ULONG_IS_FAIL:
            printf("%lu\n", *((ulong*) value));
            break;
          case GET_SIZE:
            internal::int64_t2str((uint64_t)(*(size_t*)value), buff, 10);
            printf("%s\n", buff);
            break;
          case GET_LL:
            printf("%s\n", internal::llstr(*((int64_t*) value), buff));
            break;
          case GET_ULL:
          case GET_UINT64:
            internal::int64_t2str(*((uint64_t*) value), buff, 10);
            printf("%s\n", buff);
            break;
          case GET_DOUBLE:
            printf("%g\n", *(double*) value);
            break;
          default:
            printf(_("(Disabled)\n"));
            break;
        }
      }
    }
  }

} /* namespace drizzled */
