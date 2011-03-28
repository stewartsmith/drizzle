/* Copyright (C) 2000-2003 MySQL AB

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

/****************************************************************************
 Add all options from files named "group".cnf from the default_directories
 before the command line arguments.
 On Windows defaults will also search in the Windows directory for a file
 called 'group'.ini
 As long as the program uses the last argument for conflicting
 options one only have to add a call to "load_defaults" to enable
 use of default values.
 pre- and end 'blank space' are removed from options and values. The
 following escape sequences are recognized in values:  \b \t \n \r \\

 The following arguments are handled automaticly;  If used, they must be
 first argument on the command line!
 --no-defaults	; no options are read.
 --defaults-file=full-path-to-default-file	; Only this file will be read.
 --defaults-extra-file=full-path-to-default-file ; Read this file before ~/
 --defaults-group-suffix  ; Also read groups with concat(group, suffix)
 --print-defaults	  ; Print the modified command line and exit
****************************************************************************/

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/charset_info.h>
#include <drizzled/typelib.h>
#include <drizzled/configmake.h>
#include <drizzled/gettext.h>
#include <drizzled/dynamic_array.h>
#include <drizzled/cached_directory.h>
#include <drizzled/memory/root.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <cstdio>
#include <algorithm>

using namespace std;

namespace drizzled
{
namespace internal
{

/* Define the type of function to be passed to process_default_option_files */
typedef int (*Process_option_func)(void *ctx, const char *group_name,
                                   const char *option);

/* Which directories are searched for options (and in which order) */

#define MAX_DEFAULT_DIRS 6
const char *default_directories[MAX_DEFAULT_DIRS + 1];

/*
   This structure defines the context that we pass to callback
   function 'handle_default_option' used in search_default_file
   to process each option. This context is used if search_default_file
   was called from load_defaults.
*/

struct handle_option_ctx
{
   memory::Root *alloc;
   DYNAMIC_ARRAY *args;
   TYPELIB *group;
};



} /* namespace internal */
} /* namespace drizzled */
