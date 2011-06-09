/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */



#pragma once

#ifdef __cplusplus
# include <cstdio>
#else
# include <stdio.h>
#endif

#include <errno.h>
#include <sys/types.h>

#include <drizzled/definitions.h>
#include <drizzled/internal/my_pthread.h>

#include <drizzled/charset.h>                    /* for charset_info_st */
#include <stdarg.h>

#ifndef errno				/* did we already get it? */
#ifdef HAVE_ERRNO_AS_DEFINE
#include <errno.h>			/* errno is a define */
#else
extern int errno;			/* declare errno */
#endif
#endif					/* #ifndef errno */

#ifdef HAVE_SYS_MMAN_H 
#include <sys/mman.h>
#endif

#include <drizzled/qsort_cmp.h>

#include <drizzled/visibility.h>

namespace drizzled
{
namespace internal
{

#ifndef MAP_NOSYNC
#define MAP_NOSYNC      0
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0         /* For irix and AIX */
#endif

/*
  EDQUOT is used only in 3 C files only in mysys/. If it does not exist on
  system, we set it to some value which can never happen.
*/
#ifndef EDQUOT
#define EDQUOT (-1)
#endif

/* Sun Studio does not inject this into main namespace yet */
#if defined(__cplusplus)
  using std::FILE;
#endif

#define MY_INIT(name);		{ ::drizzled::internal::my_progname= name; ::drizzled::internal::my_init(); }


	/* General bitmaps for my_func's */
#define MY_FFNF		1	/* Fatal if file not found */
#define MY_FNABP	2	/* Fatal if not all bytes read/writen */
#define MY_NABP		4	/* Error if not all bytes read/writen */
#define MY_FAE		8	/* Fatal if any error */
#define MY_WME		16	/* Write message on error */
#define MY_WAIT_IF_FULL 32	/* Wait and try again if disk full error */
#define MY_IGNORE_BADFD 32      /* my_sync: ignore 'bad descriptor' errors */
#define MY_SYNC_DIR     1024    /* my_create/delete/rename: sync directory */
#define MY_FULL_IO     512      /* For my_read - loop intil I/O is complete */
#define MY_DONT_CHECK_FILESIZE 128 /* Option to init_io_cache() */
#define MY_LINK_WARNING 32	/* my_redel() gives warning if links */
#define MY_COPYTIME	64	/* my_redel() copys time */
#define MY_DELETE_OLD	256	/* my_create_with_symlink() */
#define MY_HOLD_ORIGINAL_MODES 128  /* my_copy() holds to file modes */
#define MY_DONT_WAIT	64	/* my_lock() don't wait if can't lock */
#define MY_DONT_OVERWRITE_FILE 1024	/* my_copy: Don't overwrite file */
#define MY_THREADSAFE 2048      /* my_seek(): lock fd mutex */

#define ME_OLDWIN	2	/* Use old window */
#define ME_BELL		4	/* Ring bell then printing message */
#define ME_WAITTANG	32	/* Wait for a user action  */
#define ME_NOREFRESH	64	/* Dont refresh screen */
#define ME_NOINPUT	128	/* Dont use the input libary */

	/* Bits in last argument to fn_format */
#define MY_REPLACE_DIR		1	/* replace dir in name with 'dir' */
#define MY_REPLACE_EXT		2	/* replace extension with 'ext' */
#define MY_UNPACK_FILENAME	4	/* Unpack name (~ -> home) */
#define MY_RESOLVE_SYMLINKS	16	/* Resolve all symbolic links */
#define MY_RETURN_REAL_PATH	32	/* return full path for file */
#define MY_SAFE_PATH		64	/* Return NULL if too long path */
#define MY_RELATIVE_PATH	128	/* name is relative to 'dir' */
#define MY_APPEND_EXT           256     /* add 'ext' as additional extension*/

typedef uint64_t my_off_t;

extern char *home_dir;			/* Home directory for user */
extern const char *my_progname;		/* program-name (printed in errors) */

extern DRIZZLED_API int my_umask,		/* Default creation mask  */
	   my_umask_dir,
	   my_recived_signals,	/* Signals we have got */
	   my_safe_to_handle_signal, /* Set when allowed to SIGTSTP */
	   my_dont_interrupt;	/* call remember_intr when set */
extern bool my_use_symdir;

extern uint32_t	my_default_record_cache_size;
extern bool my_disable_async_io,
               my_disable_flush_key_blocks, my_disable_symlinks;
extern char	wild_many, wild_one, wild_prefix;
extern const char *charsets_dir;

extern bool timed_mutexes;

enum cache_type
{
  TYPE_NOT_SET= 0,
  READ_CACHE,
  WRITE_CACHE,
  READ_FIFO,
  READ_NET,
  WRITE_NET
};

typedef struct record_cache	/* Used when cacheing records */
{
public:
  int file;
  int	rc_seek,error,inited;
  uint	rc_length,read_length,reclength;
  my_off_t rc_record_pos,end_of_file;
  unsigned char *rc_buff,*rc_buff2,*rc_pos,*rc_end,*rc_request_pos;
  enum cache_type type;

  record_cache():
    file(0),
    rc_seek(0),
    error(0),
    inited(0),
    rc_length(0),
    read_length(0),
    reclength(0),
    rc_record_pos(0),
    end_of_file(0),
    rc_buff(NULL),
    rc_buff2(NULL),
    rc_pos(NULL),
    rc_end(NULL),
    rc_request_pos(NULL)
  {}

} RECORD_CACHE;


	/* defines for mf_iocache */

	/* Test if buffer is inited */
#define my_b_clear(info) (info)->buffer=0
#define my_b_inited(info) (info)->buffer
#define my_b_EOF INT_MIN

#define my_b_read(info,Buffer,Count) \
  ((info)->read_pos + (Count) <= (info)->read_end ?\
   (memcpy(Buffer,(info)->read_pos,(size_t) (Count)), \
    ((info)->read_pos+=(Count)),0) :\
   (*(info)->read_function)((info),Buffer,Count))

#define my_b_write(info,Buffer,Count) \
 ((info)->write_pos + (Count) <=(info)->write_end ?\
  (memcpy((info)->write_pos, (Buffer), (size_t)(Count)),\
   ((info)->write_pos+=(Count)),0) : \
   (*(info)->write_function)((info),(Buffer),(Count)))

#define my_b_get(info) \
  ((info)->read_pos != (info)->read_end ?\
   ((info)->read_pos++, (int) (unsigned char) (info)->read_pos[-1]) :\
   _my_b_get(info))

#define my_b_tell(info) ((info)->pos_in_file + \
			 (size_t) (*(info)->current_pos - (info)->request_pos))

#define my_b_bytes_in_cache(info) (size_t) (*(info)->current_end - \
					  *(info)->current_pos)

/* Prototypes for mysys and my_func functions */

extern int my_copy(const char *from,const char *to,myf MyFlags);
DRIZZLED_API int my_delete(const char *name,myf MyFlags);
DRIZZLED_API int my_open(const char *FileName,int Flags,myf MyFlags);
extern int my_register_filename(int fd, const char *FileName,
                                uint32_t error_message_number, myf MyFlags);
DRIZZLED_API int my_create(const char *FileName,int CreateFlags,
                           int AccessFlags, myf MyFlags);
DRIZZLED_API int my_close(int Filedes,myf MyFlags);
extern int my_mkdir(const char *dir, int Flags, myf MyFlags);
extern int my_realpath(char *to, const char *filename, myf MyFlags);
extern int my_create_with_symlink(const char *linkname, const char *filename,
                                  int createflags, int access_flags,
                                  myf MyFlags);
DRIZZLED_API int my_delete_with_symlink(const char *name, myf MyFlags);
extern int my_rename_with_symlink(const char *from,const char *to,myf MyFlags);
DRIZZLED_API size_t my_read(int Filedes,unsigned char *Buffer,size_t Count,myf MyFlags);
DRIZZLED_API int my_rename(const char *from, const char *to,myf MyFlags);
DRIZZLED_API size_t my_write(int Filedes, const unsigned char *Buffer,
                             size_t Count, myf MyFlags);

extern int check_if_legal_tablename(const char *path);

DRIZZLED_API int my_sync(int fd, myf my_flags);
extern int my_sync_dir(const char *dir_name, myf my_flags);
extern int my_sync_dir_by_file(const char *file_name, myf my_flags);
extern void my_init();
extern void my_end();
extern int my_redel(const char *from, const char *to, int MyFlags);
extern int my_copystat(const char *from, const char *to, int MyFlags);

extern void my_remember_signal(int signal_number,void (*func)(int));
extern size_t dirname_part(char * to,const char *name, size_t *to_res_length);
extern size_t dirname_length(const char *name);
#define base_name(A) (A+dirname_length(A))
bool test_if_hard_path(const char *dir_name);

extern char *convert_dirname(char *to, const char *from, const char *from_end);
extern char * fn_ext(const char *name);
extern char * fn_same(char * toname,const char *name,int flag);
DRIZZLED_API char * fn_format(char * to,const char *name,const char *dir,
                              const char *form, uint32_t flag);
extern size_t unpack_dirname(char * to,const char *from);
extern size_t unpack_filename(char * to,const char *from);
extern char * intern_filename(char * to,const char *from);
extern int pack_filename(char * to, const char *name, size_t max_length);
extern char * my_load_path(char * to, const char *path,
			      const char *own_path_prefix);
extern int wild_compare(const char *str,const char *wildstr,
                        bool str_is_pattern);

extern bool array_append_string_unique(const char *str,
                                          const char **array, size_t size);
extern int init_record_cache(RECORD_CACHE *info,size_t cachesize,int file,
			     size_t reclength,enum cache_type type,
			     bool use_async_io);
extern int read_cache_record(RECORD_CACHE *info,unsigned char *to);
extern int end_record_cache(RECORD_CACHE *info);
extern int write_cache_record(RECORD_CACHE *info,my_off_t filepos,
			      const unsigned char *record,size_t length);
extern int flush_write_cache(RECORD_CACHE *info);
extern void sigtstp_handler(int signal_number);
extern void handle_recived_signals(void);

extern void my_set_alarm_variable(int signo);
extern void my_string_ptr_sort(unsigned char *base,uint32_t items,size_t size);
extern void radixsort_for_str_ptr(unsigned char* base[], uint32_t number_of_elements,
				  size_t size_of_element,unsigned char *buffer[]);
extern void my_qsort(void *base_ptr, size_t total_elems, size_t size,
                     qsort_cmp cmp);
extern void my_qsort2(void *base_ptr, size_t total_elems, size_t size,
                      qsort2_cmp cmp, void *cmp_argument);
extern qsort2_cmp get_ptr_compare(size_t);
DRIZZLED_API void my_store_ptr(unsigned char *buff, size_t pack_length, my_off_t pos);
DRIZZLED_API my_off_t my_get_ptr(unsigned char *ptr, size_t pack_length);
int create_temp_file(char *to, const char *dir, const char *pfx, myf MyFlags);

} /* namespace internal */
} /* namespace drizzled */

