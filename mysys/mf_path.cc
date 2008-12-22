/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include <mystrings/m_string.h>
#include "my_static.h"
#include <drizzled/configmake.h>
#include <stdlib.h>
#include <sstream>

using namespace std;

static char *find_file_in_path(char *to,const char *name);

	/* Finds where program can find it's files.
	   pre_pathname is found by first locking at progname (argv[0]).
	   if progname contains path the path is returned.
	   else if progname is found in path, return it
	   else if progname is given and POSIX environment variable "_" is set
	   then path is taken from "_".
	   If filename doesn't contain a path append MY_BASEDIR_VERSION or
	   MY_BASEDIR if defined, else append "/my/running".
	   own_path_name_part is concatinated to result.
	   my_path puts result in to and returns to */

char * my_path(char * to, const char *progname,
               const char *own_pathname_part)
{
  char *start, *prog;
  const char *end;
  size_t to_length;

  start=to;					/* Return this */
  if (progname && (dirname_part(to, progname, &to_length) ||
		   find_file_in_path(to,progname) ||
		   ((prog=getenv("_")) != 0 &&
                    dirname_part(to, prog, &to_length))))
  {
    intern_filename(to,to);
    if (!test_if_hard_path(to))
    {
      if (!getcwd(curr_dir,FN_REFLEN))
	bchange((unsigned char*) to, 0, (unsigned char*) curr_dir, strlen(curr_dir), strlen(to)+1);
    }
  }
  else
  {
    if ((end = getenv("MY_BASEDIR_VERSION")) == 0 &&
	(end = getenv("MY_BASEDIR")) == 0)
    {
      end= PREFIX;
    }
    intern_filename(to,end);
    to= strchr(to, '\0');
    if (to != start && to[-1] != FN_LIBCHAR)
      *to++ = FN_LIBCHAR;
    strcpy(to,own_pathname_part);
  }
  return(start);
} /* my_path */


	/* test if file without filename is found in path */
	/* Returns to if found and to has dirpart if found, else NULL */

#define PATH_SEP ':'

static char *find_file_in_path(char *to, const char *name)
{
  char *path, *pos, dir[2];
  const char *ext="";
  ostringstream sstream;

  if (!(path=getenv("PATH")))
    return NULL;
  dir[0]=FN_LIBCHAR; dir[1]=0;
#ifdef PROGRAM_EXTENSION
  if (!fn_ext(name)[0])
    ext=PROGRAM_EXTENSION;
#endif

  for (pos=path ; (pos=strchr(pos,PATH_SEP)) ; path= ++pos)
  {
    if (path != pos)
    {
      sstream << path << dir << name << ext  << '\0';
      strncpy(to, sstream.str().c_str(), sstream.str().length());
      if (!access(to,F_OK))
      {
        to[(uint) (pos-path)+1]=0;	/* Return path only */
        return to;
      }
    }
  }
  return NULL;				/* File not found */
}

	/* Test if hard pathname */
	/* Returns true if dirname is a hard path */

bool test_if_hard_path(register const char *dir_name)
{
  if (dir_name[0] == FN_HOMELIB && dir_name[1] == FN_LIBCHAR)
    return (home_dir != NULL && test_if_hard_path(home_dir));
  if (dir_name[0] == FN_LIBCHAR)
    return (true);
  return false;
} /* test_if_hard_path */

