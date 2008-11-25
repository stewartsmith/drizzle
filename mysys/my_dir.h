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

#ifndef _my_dir_h
#define _my_dir_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef MY_DIR_H
#define MY_DIR_H

#include <drizzled/definitions.h>
#include <sys/stat.h>

#define MY_DONT_SORT	512	/* my_lib; Don't sort files */
#define MY_WANT_STAT	1024	/* my_lib; stat files */

/* Struct describing one file returned from my_dir */
typedef struct fileinfo
{
  char			*name;
  struct stat		*mystat;
} FILEINFO;

typedef struct st_my_dir	/* Struct returned from my_dir */
{
  /*
    These members are just copies of parts of DYNAMIC_ARRAY structure, 
    which is allocated right after the end of MY_DIR structure (MEM_ROOT
    for storing names is also resides there). We've left them here because
    we don't want to change code that uses my_dir.
  */
  struct fileinfo	*dir_entry;
  uint			number_off_files;
} MY_DIR;

extern MY_DIR *my_dir(const char *path, myf MyFlags);
extern void my_dirend(MY_DIR *buffer);

#endif /* MY_DIR_H */

#ifdef	__cplusplus
}
#endif
#endif
