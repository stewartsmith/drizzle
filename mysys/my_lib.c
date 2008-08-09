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

/* TODO: check for overun of memory for names. */
/*	 Convert MSDOS-TIME to standar time_t (still needed?) */

#include	"mysys_priv.h"
#include	<mystrings/m_string.h>
#include	<my_dir.h>	/* Structs used by my_dir,includes sys/types */
#include	"mysys_err.h"
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if defined(HAVE_SYS_NDIR_H)
#  include <sys/ndir.h>
# endif
# if defined(HAVE_SYS_DIR_H)
#  include <sys/dir.h>
# endif
# if defined(HAVE_NDIR_H)
#  include <ndir.h>
# endif
#endif

#if defined(HAVE_READDIR_R)
#define READDIR(A,B,C) ((errno=readdir_r(A,B,&C)) != 0 || !C)
#else
#define READDIR(A,B,C) (!(C=readdir(A)))
#endif

/*
  We are assuming that directory we are reading is either has less than 
  100 files and so can be read in one initial chunk or has more than 1000
  files and so big increment are suitable.
*/
#define ENTRIES_START_SIZE (8192/sizeof(FILEINFO))
#define ENTRIES_INCREMENT  (65536/sizeof(FILEINFO))
#define NAMES_START_SIZE   32768


static int	comp_names(struct fileinfo *a,struct fileinfo *b);


	/* We need this because program don't know with malloc we used */

void my_dirend(MY_DIR *buffer)
{
  if (buffer)
  {
    delete_dynamic((DYNAMIC_ARRAY*)((char*)buffer + 
                                    ALIGN_SIZE(sizeof(MY_DIR))));
    free_root((MEM_ROOT*)((char*)buffer + ALIGN_SIZE(sizeof(MY_DIR)) + 
                          ALIGN_SIZE(sizeof(DYNAMIC_ARRAY))), MYF(0));
    my_free((uchar*) buffer,MYF(0));
  }
  return;
} /* my_dirend */


	/* Compare in sort of filenames */

static int comp_names(struct fileinfo *a, struct fileinfo *b)
{
  return (strcmp(a->name,b->name));
} /* comp_names */


MY_DIR	*my_dir(const char *path, myf MyFlags)
{
  char          *buffer;
  MY_DIR        *result= 0;
  FILEINFO      finfo;
  DYNAMIC_ARRAY *dir_entries_storage;
  MEM_ROOT      *names_storage;
  DIR		*dirp;
  struct dirent *dp;
  char		tmp_path[FN_REFLEN+1],*tmp_file;
  char	dirent_tmp[sizeof(struct dirent)+_POSIX_PATH_MAX+1];

#if !defined(HAVE_READDIR_R)
  pthread_mutex_lock(&THR_LOCK_open);
#endif

  dirp = opendir(directory_file_name(tmp_path,(char *) path));
  if (dirp == NULL || 
      ! (buffer= my_malloc(ALIGN_SIZE(sizeof(MY_DIR)) + 
                           ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)) +
                           sizeof(MEM_ROOT), MyFlags)))
    goto error;

  dir_entries_storage= (DYNAMIC_ARRAY*)(buffer + ALIGN_SIZE(sizeof(MY_DIR))); 
  names_storage= (MEM_ROOT*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)) +
                             ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)));
  
  if (my_init_dynamic_array(dir_entries_storage, sizeof(FILEINFO),
                            ENTRIES_START_SIZE, ENTRIES_INCREMENT))
  {
    my_free((uchar*) buffer,MYF(0));
    goto error;
  }
  init_alloc_root(names_storage, NAMES_START_SIZE, NAMES_START_SIZE);
  
  /* MY_DIR structure is allocated and completly initialized at this point */
  result= (MY_DIR*)buffer;

  tmp_file=strend(tmp_path);

  dp= (struct dirent*) dirent_tmp;
  
  while (!(READDIR(dirp,(struct dirent*) dirent_tmp,dp)))
  {
    if (!(finfo.name= strdup_root(names_storage, dp->d_name)))
      goto error;
    
    if (MyFlags & MY_WANT_STAT)
    {
      if (!(finfo.mystat= (struct stat*)alloc_root(names_storage, 
                                               sizeof(struct stat))))
        goto error;
      
      memset(finfo.mystat, 0, sizeof(struct stat));
      VOID(stpcpy(tmp_file,dp->d_name));
      VOID(stat(tmp_path, finfo.mystat));
      if (!(finfo.mystat->st_mode & S_IREAD))
        continue;
    }
    else
      finfo.mystat= NULL;

    if (push_dynamic(dir_entries_storage, (uchar*)&finfo))
      goto error;
  }

  (void) closedir(dirp);
#if !defined(HAVE_READDIR_R)
  pthread_mutex_unlock(&THR_LOCK_open);
#endif
  result->dir_entry= (FILEINFO *)dir_entries_storage->buffer;
  result->number_off_files= dir_entries_storage->elements;
  
  if (!(MyFlags & MY_DONT_SORT))
    my_qsort((void *) result->dir_entry, result->number_off_files,
          sizeof(FILEINFO), (qsort_cmp) comp_names);
  return(result);

 error:
#if !defined(HAVE_READDIR_R)
  pthread_mutex_unlock(&THR_LOCK_open);
#endif
  my_errno=errno;
  if (dirp)
    (void) closedir(dirp);
  my_dirend(result);
  if (MyFlags & (MY_FAE | MY_WME))
    my_error(EE_DIR,MYF(ME_BELL+ME_WAITTANG),path,my_errno);
  return((MY_DIR *) NULL);
} /* my_dir */


/*
 * Convert from directory name to filename.
 * On VMS:
 *	 xyzzy:[mukesh.emacs] => xyzzy:[mukesh]emacs.dir.1
 *	 xyzzy:[mukesh] => xyzzy:[000000]mukesh.dir.1
 * On UNIX, it's simple: just make sure there is a terminating /

 * Returns pointer to dst;
 */

char * directory_file_name (char * dst, const char *src)
{
  /* Process as Unix format: just remove test the final slash. */

  char * end;

  if (src[0] == 0)
    src= (char*) ".";				/* Use empty as current */
  end=stpcpy(dst, src);
  if (end[-1] != FN_LIBCHAR)
  {
    end[0]=FN_LIBCHAR;				/* Add last '/' */
    end[1]='\0';
  }
  return dst;
} /* directory_file_name */

