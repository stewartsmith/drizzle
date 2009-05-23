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

#include "mysys/mysys_priv.h"
#include "my_static.h"
#include "mysys/mysys_err.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static void make_ftype(char * to,int flag);

/*
  Open a file as stream

  SYNOPSIS
    my_fopen()
    FileName	Path-name of file
    Flags	Read | write | append | trunc (like for open())
    MyFlags	Flags for handling errors

  RETURN
    0	Error
    #	File handler
*/

FILE *my_fopen(const char *filename, int flags, myf MyFlags)
{
  FILE *fd;
  char type[5];
  /*
    if we are not creating, then we need to use my_access to make sure
    the file exists since Windows doesn't handle files like "com1.sym"
    very  well
  */
  {
    make_ftype(type,flags);
    fd = fopen(filename, type);
  }

  if (fd != NULL)
  {
    my_stream_opened++;
    return fd;				/* safeguard */
  }
  else
    my_errno=errno;
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
    my_error((flags & O_RDONLY) || (flags == O_RDONLY ) ? EE_FILENOTFOUND :
	     EE_CANTCREATEFILE,
	     MYF(ME_BELL+ME_WAITTANG), filename, my_errno);
  return NULL;
} /* my_fopen */


	/* Close a stream */

int my_fclose(FILE *fd, myf MyFlags)
{
  int err,file;

  file= fileno(fd);
  if ((err = fclose(fd)) < 0)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_BADCLOSE, MYF(ME_BELL+ME_WAITTANG),
	       my_filename(file),errno);
  }
  else
    my_stream_opened--;

  return(err);
} /* my_fclose */



/*
  Make a fopen() typestring from a open() type bitmap

  SYNOPSIS
    make_ftype()
    to		String for fopen() is stored here
    flag	Flag used by open()

  IMPLEMENTATION
    This routine attempts to find the best possible match
    between  a numeric option and a string option that could be
    fed to fopen.  There is not a 1 to 1 mapping between the two.

  NOTE
    On Unix, O_RDONLY is usually 0

  MAPPING
    r  == O_RDONLY
    w  == O_WRONLY|O_TRUNC|O_CREAT
    a  == O_WRONLY|O_APPEND|O_CREAT
    r+ == O_RDWR
    w+ == O_RDWR|O_TRUNC|O_CREAT
    a+ == O_RDWR|O_APPEND|O_CREAT
*/

static void make_ftype(register char * to, register int flag)
{
  /* check some possible invalid combinations */
  assert((flag & (O_TRUNC | O_APPEND)) != (O_TRUNC | O_APPEND));
  assert((flag & (O_WRONLY | O_RDWR)) != (O_WRONLY | O_RDWR));

  if ((flag & (O_RDONLY|O_WRONLY)) == O_WRONLY)
    *to++= (flag & O_APPEND) ? 'a' : 'w';
  else if (flag & O_RDWR)
  {
    /* Add '+' after theese */
    if (flag & (O_TRUNC | O_CREAT))
      *to++= 'w';
    else if (flag & O_APPEND)
      *to++= 'a';
    else
      *to++= 'r';
    *to++= '+';
  }
  else
    *to++= 'r';

  *to='\0';
} /* make_ftype */
