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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <drizzled/internal/my_sys.h>

#include <pwd.h>

#include <drizzled/internal/m_string.h>
#include "my_static.h"

namespace drizzled
{
namespace internal
{

static char * expand_tilde(char * *path);
static size_t system_filename(char * to, const char *from);

/*
  remove unwanted chars from dirname

  SYNOPSIS
     cleanup_dirname()
     to		Store result here
     from	Dirname to fix.  May be same as to

  IMPLEMENTATION
  "/../" removes prev dir
  "/~/" removes all before ~
  //" is same as "/", except on Win32 at start of a file
  "/./" is removed
  Unpacks home_dir if "~/.." used
  Unpacks current dir if if "./.." used

  RETURN
    #  length of new name
*/

static size_t cleanup_dirname(char *to, const char *from)
{
  size_t length;
  char * pos;
  const char * from_ptr;
  char * start;
  char parent[5],				/* for "FN_PARENTDIR" */
       buff[FN_REFLEN+1],*end_parentdir;

  start=buff;
  from_ptr= from;
#ifdef FN_DEVCHAR
  if ((pos=strrchr(from_ptr,FN_DEVCHAR)) != 0)
  {						/* Skip device part */
    length=(size_t) (pos-from_ptr)+1;
    start= strncpy(buff,from_ptr,length);
    start+= strlen(from_ptr);
    from_ptr+=length;
  }
#endif

  parent[0]=FN_LIBCHAR;
  length= (size_t)((strcpy(parent+1,FN_PARENTDIR)+strlen(FN_PARENTDIR))-parent);
  for (pos=start ; (*pos= *from_ptr++) != 0 ; pos++)
  {
#ifdef BACKSLASH_MBTAIL
    uint32_t l;
    if (use_mb(fs) && (l= my_ismbchar(fs, from_ptr - 1, from_ptr + 2)))
    {
      for (l-- ; l ; *++pos= *from_ptr++, l--);
      start= pos + 1; /* Don't look inside multi-byte char */
      continue;
    }
#endif
    if (*pos == '/')
      *pos = FN_LIBCHAR;
    if (*pos == FN_LIBCHAR)
    {
      if ((size_t) (pos-start) > length &&
          memcmp(pos-length,parent,length) == 0)
      {						/* If .../../; skip prev */
	pos-=length;
	if (pos != start)
	{					 /* not /../ */
	  pos--;
	  if (*pos == FN_HOMELIB && (pos == start || pos[-1] == FN_LIBCHAR))
	  {
	    if (!home_dir)
	    {
	      pos+=length+1;			/* Don't unpack ~/.. */
	      continue;
	    }
	    pos= strcpy(buff,home_dir)+strlen(home_dir)-1;	/* Unpacks ~/.. */
	    if (*pos == FN_LIBCHAR)
	      pos--;				/* home ended with '/' */
	  }
	  if (*pos == FN_CURLIB && (pos == start || pos[-1] == FN_LIBCHAR))
	  {
	    if (getcwd(curr_dir,FN_REFLEN))
	    {
	      pos+=length+1;			/* Don't unpack ./.. */
	      continue;
	    }
	    pos= strcpy(buff,curr_dir)+strlen(curr_dir)-1;	/* Unpacks ./.. */
	    if (*pos == FN_LIBCHAR)
	      pos--;				/* home ended with '/' */
	  }
	  end_parentdir=pos;
	  while (pos >= start && *pos != FN_LIBCHAR)	/* remove prev dir */
	    pos--;
	  if (pos[1] == FN_HOMELIB || memcmp(pos,parent,length) == 0)
	  {					/* Don't remove ~user/ */
	    pos= strcpy(end_parentdir+1,parent)+strlen(parent);
	    *pos=FN_LIBCHAR;
	    continue;
	  }
	}
      }
      else if ((size_t) (pos-start) == length-1 &&
	       !memcmp(start,parent+1,length-1))
	start=pos;				/* Starts with "../" */
      else if (pos-start > 0 && pos[-1] == FN_LIBCHAR)
      {
#ifdef FN_NETWORK_DRIVES
	if (pos-start != 1)
#endif
	  pos--;			/* Remove dupplicate '/' */
      }
      else if (pos-start > 1 && pos[-1] == FN_CURLIB && pos[-2] == FN_LIBCHAR)
	pos-=2;					/* Skip /./ */
      else if (pos > buff+1 && pos[-1] == FN_HOMELIB && pos[-2] == FN_LIBCHAR)
      {					/* Found ..../~/  */
	buff[0]=FN_HOMELIB;
	buff[1]=FN_LIBCHAR;
	start=buff; pos=buff+1;
      }
    }
  }
  (void) strcpy(to,buff);
  return((size_t) (pos-buff));
} /* cleanup_dirname */


/*
  On system where you don't have symbolic links, the following
  code will allow you to create a file:
  directory-name.sym that should contain the real path
  to the directory.  This will be used if the directory name
  doesn't exists
*/


bool my_use_symdir=0;	/* Set this if you want to use symdirs */


/*
  Fixes a directroy name so that can be used by open()

  SYNOPSIS
    unpack_dirname()
    to			result-buffer, FN_REFLEN characters. may be == from
    from		'Packed' directory name (may contain ~)

 IMPLEMENTATION
  Make that last char of to is '/' if from not empty and
  from doesn't end in FN_DEVCHAR
  Uses cleanup_dirname and changes ~/.. to home_dir/..

  Changes a UNIX filename to system filename (replaces / with \ on windows)

  RETURN
   Length of new directory name (= length of to)
*/

size_t unpack_dirname(char * to, const char *from)
{
  size_t length, h_length;
  char buff[FN_REFLEN+1+4],*suffix,*tilde_expansion;

  (void) intern_filename(buff,from);	    /* Change to intern name */
  length= strlen(buff);                     /* Fix that '/' is last */
  if (length &&
#ifdef FN_DEVCHAR
      buff[length-1] != FN_DEVCHAR &&
#endif
      buff[length-1] != FN_LIBCHAR && buff[length-1] != '/')
  {
    buff[length]=FN_LIBCHAR;
    buff[length+1]= '\0';
  }

  length=cleanup_dirname(buff,buff);
  if (buff[0] == FN_HOMELIB)
  {
    suffix=buff+1; tilde_expansion=expand_tilde(&suffix);
    if (tilde_expansion)
    {
      length-= (size_t) (suffix-buff)-1;
      if (length+(h_length= strlen(tilde_expansion)) <= FN_REFLEN)
      {
	if (tilde_expansion[h_length-1] == FN_LIBCHAR)
	  h_length--;
	if (buff+h_length < suffix)
          memmove(buff+h_length, suffix, length);
	else
	  bmove_upp((unsigned char*) buff+h_length+length, (unsigned char*) suffix+length, length);
        memmove(buff, tilde_expansion, h_length);
      }
    }
  }
  return(system_filename(to,buff));	/* Fix for open */
} /* unpack_dirname */


	/* Expand tilde to home or user-directory */
	/* Path is reset to point at FN_LIBCHAR after ~xxx */

static char * expand_tilde(char * *path)
{
  if (path[0][0] == FN_LIBCHAR)
    return home_dir;			/* ~/ expanded to home */
  char *str,save;
  struct passwd *user_entry;

  if (!(str=strchr(*path,FN_LIBCHAR)))
    str= strchr(*path, '\0');
  save= *str; *str= '\0';
  user_entry=getpwnam(*path);
  *str=save;
  endpwent();
  if (user_entry)
  {
    *path=str;
    return user_entry->pw_dir;
  }
  return NULL;
}


/*
  Fix filename so it can be used by open, create

  SYNOPSIS
    unpack_filename()
    to		Store result here. Must be at least of size FN_REFLEN.
    from	Filename in unix format (with ~)

  RETURN
    # length of to

  NOTES
    to may be == from
    ~ will only be expanded if total length < FN_REFLEN
*/


size_t unpack_filename(char * to, const char *from)
{
  size_t length, n_length, buff_length;
  char buff[FN_REFLEN];

  length=dirname_part(buff, from, &buff_length);/* copy & convert dirname */
  n_length=unpack_dirname(buff,buff);
  if (n_length+strlen(from+length) < FN_REFLEN)
  {
    (void) strcpy(buff+n_length,from+length);
    length= system_filename(to,buff);		/* Fix to usably filename */
  }
  else
    length= system_filename(to,from);		/* Fix to usably filename */
  return(length);
} /* unpack_filename */


	/* Convert filename (unix standard) to system standard */
	/* Used before system command's like open(), create() .. */
	/* Returns used length of to; total length should be FN_REFLEN */

static size_t system_filename(char * to, const char *from)
{
  return strlen(strncpy(to,from,FN_REFLEN-1));
} /* system_filename */


	/* Fix a filename to intern (UNIX format) */

char *intern_filename(char *to, const char *from)
{
  size_t length, to_length;
  char buff[FN_REFLEN];
  if (from == to)
  {						/* Dirname may destroy from */
    strcpy(buff,from);
    from=buff;
  }
  length= dirname_part(to, from, &to_length);	/* Copy dirname & fix chars */
  (void) strcpy(to + to_length,from+length);
  return (to);
} /* intern_filename */

} /* namespace internal */
} /* namespace drizzled */
