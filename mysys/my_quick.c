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

/* Quicker interface to read & write. Used with my_nosys.h */

#include "mysys_priv.h"
#include "my_nosys.h"


size_t my_quick_read(File Filedes,unsigned char *Buffer,size_t Count,myf MyFlags)
{
  size_t readbytes;

  if ((readbytes = read(Filedes, Buffer, Count)) != Count)
  {
    my_errno=errno;
    return readbytes;
  }
  return (MyFlags & (MY_NABP | MY_FNABP)) ? 0 : readbytes;
}


size_t my_quick_write(File Filedes,const unsigned char *Buffer,size_t Count)
{
  if ((
       (size_t) write(Filedes,Buffer,Count)) != Count)
  {
    my_errno=errno;
    return (size_t) -1;
  }
  return 0;
}
