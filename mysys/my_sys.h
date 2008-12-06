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

#ifndef _my_sys_h
#define _my_sys_h

#include <errno.h>
#define my_errno (errno)

#include <mysys/my_pthread.h>

#include <mystrings/m_ctype.h>                    /* for CHARSET_INFO */
#include <stdarg.h>
#include <mysys/typelib.h>
#include <mysys/aio_result.h>

#include <mysys/my_alloc.h>

/* Sun Studio does not inject this into main namespace yet */
#if defined(__cplusplus)
  using std::FILE;
#endif 

#define MY_INIT(name);		{ my_progname= name; my_init(); }

#define ERRMSGSIZE	(SC_MAXWIDTH)	/* Max length of a error message */
#define NRERRBUFFS	(2)	/* Buffers for parameters */
#define MY_FILE_ERROR	((size_t) -1)

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
#define MY_RESOLVE_LINK 128	/* my_realpath(); Only resolve links */
#define MY_HOLD_ORIGINAL_MODES 128  /* my_copy() holds to file modes */
#define MY_REDEL_MAKE_BACKUP 256
#define MY_SEEK_NOT_DONE 32	/* my_lock may have to do a seek */
#define MY_DONT_WAIT	64	/* my_lock() don't wait if can't lock */
#define MY_ZEROFILL	32	/* my_malloc(), fill array with zero */
#define MY_ALLOW_ZERO_PTR 64	/* my_realloc() ; zero ptr -> malloc */
#define MY_FREE_ON_ERROR 128	/* my_realloc() ; Free old ptr on error */
#define MY_HOLD_ON_ERROR 256	/* my_realloc() ; Return old ptr on error */
#define MY_DONT_OVERWRITE_FILE 1024	/* my_copy: Don't overwrite file */
#define MY_THREADSAFE 2048      /* my_seek(): lock fd mutex */

#define MY_CHECK_ERROR	1	/* Params to my_end; Check open-close */
#define MY_GIVE_INFO	2	/* Give time info about process*/

#define MY_REMOVE_NONE    0     /* Params for modify_defaults_file */
#define MY_REMOVE_OPTION  1
#define MY_REMOVE_SECTION 2

#define ME_HIGHBYTE	8	/* Shift for colours */
#define ME_NOCUR	1	/* Don't use curses message */
#define ME_OLDWIN	2	/* Use old window */
#define ME_BELL		4	/* Ring bell then printing message */
#define ME_HOLDTANG	8	/* Don't delete last keys */
#define ME_WAITTOT	16	/* Wait for errtime secs of for a action */
#define ME_WAITTANG	32	/* Wait for a user action  */
#define ME_NOREFRESH	64	/* Dont refresh screen */
#define ME_NOINPUT	128	/* Dont use the input libary */
#define ME_COLOUR1	((1 << ME_HIGHBYTE))	/* Possibly error-colours */
#define ME_COLOUR2	((2 << ME_HIGHBYTE))
#define ME_COLOUR3	((3 << ME_HIGHBYTE))
#define ME_FATALERROR   1024    /* Fatal statement error */

	/* Bits in last argument to fn_format */
#define MY_REPLACE_DIR		1	/* replace dir in name with 'dir' */
#define MY_REPLACE_EXT		2	/* replace extension with 'ext' */
#define MY_UNPACK_FILENAME	4	/* Unpack name (~ -> home) */
#define MY_PACK_FILENAME	8	/* Pack name (home -> ~) */
#define MY_RESOLVE_SYMLINKS	16	/* Resolve all symbolic links */
#define MY_RETURN_REAL_PATH	32	/* return full path for file */
#define MY_SAFE_PATH		64	/* Return NULL if too long path */
#define MY_RELATIVE_PATH	128	/* name is relative to 'dir' */
#define MY_APPEND_EXT           256     /* add 'ext' as additional extension*/


	/* My seek flags */
#define MY_SEEK_SET	0
#define MY_SEEK_CUR	1
#define MY_SEEK_END	2

	/* Some constants */
#define MY_WAIT_FOR_USER_TO_FIX_PANIC	60	/* in seconds */
#define MY_WAIT_GIVE_USER_A_MESSAGE	10	/* Every 10 times of prev */
#define DFLT_INIT_HITS  3

	/* root_alloc flags */
#define MY_KEEP_PREALLOC	1
#define MY_MARK_BLOCKS_FREE     2  /* move used to free list and reuse them */

	/* Internal error numbers (for assembler functions) */
#define MY_ERRNO_EDOM		33
#define MY_ERRNO_ERANGE		34

	/* Bits for get_date timeflag */
#define GETDATE_DATE_TIME	1
#define GETDATE_SHORT_DATE	2
#define GETDATE_HHMMSSTIME	4
#define GETDATE_GMT		8
#define GETDATE_FIXEDLENGTH	16

	/* defines when allocating data */
#define my_checkmalloc()
#undef TERMINATE
#define TERMINATE(A,B) {}

#ifdef __cplusplus
extern "C" {
#endif

typedef int  (*qsort_cmp)(const void *,const void *);
typedef int  (*qsort_cmp2)(void*, const void *,const void *);

extern void *my_malloc(size_t Size,myf MyFlags);
#define my_malloc_ci(SZ,FLAG) my_malloc( SZ, FLAG )
extern void *my_realloc(void *oldpoint, size_t Size, myf MyFlags);
extern void my_no_flags_free(void *ptr);
extern void *my_memdup(const void *from,size_t length,myf MyFlags);
extern char *my_strdup(const char *from,myf MyFlags);
extern char *my_strndup(const char *from, size_t length,
				   myf MyFlags);
#define TRASH(A,B) /* nothing */

#ifndef errno				/* did we already get it? */
#ifdef HAVE_ERRNO_AS_DEFINE
#include <errno.h>			/* errno is a define */
#else
extern int errno;			/* declare errno */
#endif
#endif					/* #ifndef errno */
extern char errbuff[NRERRBUFFS][ERRMSGSIZE];
extern char *home_dir;			/* Home directory for user */
extern const char *my_progname;		/* program-name (printed in errors) */
typedef void (*error_handler_func)(uint32_t my_err, const char *str,myf MyFlags);
extern error_handler_func error_handler_hook;
extern error_handler_func fatal_error_handler_hook;
extern uint32_t my_file_limit;

/* charsets */
extern const CHARSET_INFO *default_charset_info;
extern CHARSET_INFO *all_charsets[256];
extern CHARSET_INFO compiled_charsets[];

/* statistics */
extern uint32_t	my_file_opened,my_stream_opened, my_tmp_file_created;
extern uint32_t    my_file_total_opened;
extern uint	mysys_usage_id;
extern bool	my_init_done;

typedef void (*void_ptr_func)(void);
typedef void (*void_ptr_int_func)(int);

					/* Point to current my_message() */
extern void_ptr_func my_sigtstp_cleanup,
					/* Executed before jump to shell */
	    my_sigtstp_restart;
extern void_ptr_int_func my_abort_hook;
					/* Executed when comming from shell */
extern int my_umask,		/* Default creation mask  */
	   my_umask_dir,
	   my_recived_signals,	/* Signals we have got */
	   my_safe_to_handle_signal, /* Set when allowed to SIGTSTP */
	   my_dont_interrupt;	/* call remember_intr when set */
extern bool mysys_uses_curses, my_use_symdir;
extern uint32_t sf_malloc_cur_memory, sf_malloc_max_memory;

extern uint32_t	my_default_record_cache_size;
extern bool my_disable_async_io,
               my_disable_flush_key_blocks, my_disable_symlinks;
extern char	wild_many, wild_one, wild_prefix;
extern const char *charsets_dir;
/* from default.c */
extern char *my_defaults_extra_file;
extern const char *my_defaults_group_suffix;
extern const char *my_defaults_file;

extern bool timed_mutexes;

typedef struct wild_file_pack	/* Struct to hold info when selecting files */
{
  uint		wilds;		/* How many wildcards */
  uint		not_pos;	/* Start of not-theese-files */
  char *	*wild;		/* Pointer to wildcards */
} WF_PACK;

enum loglevel {
   ERROR_LEVEL,
   WARNING_LEVEL,
   INFORMATION_LEVEL
};

enum cache_type
{
  TYPE_NOT_SET= 0, READ_CACHE, WRITE_CACHE,
  SEQ_READ_APPEND		/* sequential read or append */,
  READ_FIFO, READ_NET,WRITE_NET};

typedef struct st_record_cache	/* Used when cacheing records */
{
  File file;
  int	rc_seek,error,inited;
  uint	rc_length,read_length,reclength;
  my_off_t rc_record_pos,end_of_file;
  unsigned char *rc_buff,*rc_buff2,*rc_pos,*rc_end,*rc_request_pos;
#ifdef HAVE_AIOWAIT
  int	use_async_io;
  my_aio_result aio_result;
#endif
  enum cache_type type;
} RECORD_CACHE;

enum file_type
{
  UNOPEN = 0, FILE_BY_OPEN, FILE_BY_CREATE, STREAM_BY_FOPEN, STREAM_BY_FDOPEN,
  FILE_BY_MKSTEMP, FILE_BY_DUP
};

struct st_my_file_info
{
  char *		name;
  enum file_type	type;
#if !defined(HAVE_PREAD)
  pthread_mutex_t	mutex;
#endif
};

extern struct st_my_file_info *my_file_info;

typedef struct st_dynamic_array
{
  unsigned char *buffer;
  size_t elements,max_element;
  uint32_t alloc_increment;
  uint32_t size_of_element;
} DYNAMIC_ARRAY;

typedef struct st_my_tmpdir
{
  DYNAMIC_ARRAY full_list;
  char **list;
  uint32_t cur, max;
  pthread_mutex_t mutex;
} MY_TMPDIR;


typedef int (*qsort2_cmp)(const void *, const void *, const void *);

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

	/* my_b_write_byte dosn't have any err-check */
#define my_b_write_byte(info,chr) \
  (((info)->write_pos < (info)->write_end) ?\
   ((*(info)->write_pos++)=(chr)) :\
   (_my_b_write(info,0,0) , ((*(info)->write_pos++)=(chr))))

#define my_b_fill_cache(info) \
  (((info)->read_end=(info)->read_pos),(*(info)->read_function)(info,0,0))

#define my_b_tell(info) ((info)->pos_in_file + \
			 (size_t) (*(info)->current_pos - (info)->request_pos))

#define my_b_get_buffer_start(info) (info)->request_pos 
#define my_b_get_bytes_in_buffer(info) (char*) (info)->read_end -   \
  (char*) my_b_get_buffer_start(info)
#define my_b_get_pos_in_file(info) (info)->pos_in_file


#define my_b_bytes_in_cache(info) (size_t) (*(info)->current_end - \
					  *(info)->current_pos)

typedef uint32_t ha_checksum;

/* Define the type of function to be passed to process_default_option_files */
typedef int (*Process_option_func)(void *ctx, const char *group_name,
                                   const char *option);

int handle_default_option(void *in_ctx, const char *group_name,
                          const char *option);

#include <mysys/my_alloc.h>


	/* Prototypes for mysys and my_func functions */

extern int my_copy(const char *from,const char *to,myf MyFlags);
extern int my_delete(const char *name,myf MyFlags);
extern void *my_once_alloc(size_t Size,myf MyFlags);
extern void my_once_free(void);
extern char *my_once_strdup(const char *src,myf myflags);
extern void *my_once_memdup(const void *src, size_t len, myf myflags);
extern File my_open(const char *FileName,int Flags,myf MyFlags);
extern File my_register_filename(File fd, const char *FileName,
				 enum file_type type_of_file,
				 uint32_t error_message_number, myf MyFlags);
extern File my_create(const char *FileName,int CreateFlags,
		      int AccessFlags, myf MyFlags);
extern int my_close(File Filedes,myf MyFlags);
extern File my_dup(File file, myf MyFlags);
extern int my_mkdir(const char *dir, int Flags, myf MyFlags);
extern int my_readlink(char *to, const char *filename, myf MyFlags);
extern int my_realpath(char *to, const char *filename, myf MyFlags);
extern File my_create_with_symlink(const char *linkname, const char *filename,
				   int createflags, int access_flags,
				   myf MyFlags);
extern int my_delete_with_symlink(const char *name, myf MyFlags);
extern int my_rename_with_symlink(const char *from,const char *to,myf MyFlags);
extern int my_symlink(const char *content, const char *linkname, myf MyFlags);
extern size_t my_read(File Filedes,unsigned char *Buffer,size_t Count,myf MyFlags);
extern int my_rename(const char *from,const char *to,myf MyFlags);
extern my_off_t my_seek(File fd,my_off_t pos,int whence,myf MyFlags);
extern my_off_t my_tell(File fd);
extern size_t my_write(File Filedes,const unsigned char *Buffer,size_t Count,
		     myf MyFlags);
extern size_t my_fwrite(FILE *stream,const unsigned char *Buffer,size_t Count,
		      myf MyFlags);
extern my_off_t my_fseek(FILE *stream,my_off_t pos,int whence,myf MyFlags);
extern void *_mymalloc(size_t uSize,const char *sFile,
                       uint32_t uLine, myf MyFlag);
extern void *_myrealloc(void *pPtr,size_t uSize,const char *sFile,
		       uint32_t uLine, myf MyFlag);
extern void * my_multi_malloc (myf MyFlags, ...);
extern void _myfree(void *pPtr, const char *sFile, uint32_t uLine, myf MyFlag);
extern int _sanity(const char *sFile, uint32_t uLine);
extern void *_my_memdup(const void *from, size_t length,
                        const char *sFile, uint32_t uLine,myf MyFlag);
extern char * _my_strdup(const char *from, const char *sFile, uint32_t uLine,
                         myf MyFlag);
extern char *_my_strndup(const char *from, size_t length,
                         const char *sFile, uint32_t uLine,
                         myf MyFlag);

#define my_access access
extern int check_if_legal_filename(const char *path);
extern int check_if_legal_tablename(const char *path);

#define my_delete_allow_opened(fname,flags)  my_delete((fname),(flags))

#ifndef TERMINATE
extern void TERMINATE(FILE *file, uint32_t flag);
#endif
extern void init_glob_errs(void);
extern FILE *my_fopen(const char *FileName,int Flags,myf MyFlags);
extern int my_fclose(FILE *fd,myf MyFlags);
extern int my_sync(File fd, myf my_flags);
extern int my_sync_dir(const char *dir_name, myf my_flags);
extern int my_sync_dir_by_file(const char *file_name, myf my_flags);
extern void my_error(int nr,myf MyFlags, ...);
extern void my_printf_error(uint32_t my_err, const char *format,
                            myf MyFlags, ...)
  __attribute__((format(printf, 2, 4)));
extern int my_error_register(const char **errmsgs, int first, int last);
extern const char **my_error_unregister(int first, int last);
extern void my_message(uint32_t my_err, const char *str,myf MyFlags);
extern void my_message_no_curses(uint32_t my_err, const char *str,myf MyFlags);
extern bool my_init(void);
extern void my_end(int infoflag);
extern int my_redel(const char *from, const char *to, int MyFlags);
extern int my_copystat(const char *from, const char *to, int MyFlags);
extern char * my_filename(File fd);

#ifdef EXTRA_DEBUG
void my_print_open_files(void);
#else
#define my_print_open_files()
#endif

extern bool init_tmpdir(MY_TMPDIR *tmpdir, const char *pathlist);
extern char *my_tmpdir(MY_TMPDIR *tmpdir);
extern void free_tmpdir(MY_TMPDIR *tmpdir);

extern void my_remember_signal(int signal_number,RETSIGTYPE (*func)(int));
extern size_t dirname_part(char * to,const char *name, size_t *to_res_length);
extern size_t dirname_length(const char *name);
#define base_name(A) (A+dirname_length(A))
bool test_if_hard_path(const char *dir_name);
extern char *convert_dirname(char *to, const char *from, const char *from_end);
extern void to_unix_path(char * name);
extern char * fn_ext(const char *name);
extern char * fn_same(char * toname,const char *name,int flag);
extern char * fn_format(char * to,const char *name,const char *dir,
			   const char *form, uint32_t flag);
extern size_t strlength(const char *str);
extern void pack_dirname(char * to,const char *from);
extern size_t unpack_dirname(char * to,const char *from);
extern size_t cleanup_dirname(char * to,const char *from);
extern size_t system_filename(char * to,const char *from);
extern size_t unpack_filename(char * to,const char *from);
extern char * intern_filename(char * to,const char *from);
extern char * directory_file_name(char * dst, const char *src);
extern int pack_filename(char * to, const char *name, size_t max_length);
extern char * my_path(char * to,const char *progname,
			 const char *own_pathname_part);
extern char * my_load_path(char * to, const char *path,
			      const char *own_path_prefix);
extern int wild_compare(const char *str,const char *wildstr,
                        bool str_is_pattern);
extern WF_PACK *wf_comp(char * str);
extern int wf_test(struct wild_file_pack *wf_pack,const char *name);
extern void wf_end(struct wild_file_pack *buffer);
extern bool array_append_string_unique(const char *str,
                                          const char **array, size_t size);
extern void get_date(char * to,int timeflag,time_t use_time);
extern void soundex(CHARSET_INFO *, char * out_pntr, char * in_pntr,
                    bool remove_garbage);
extern int init_record_cache(RECORD_CACHE *info,size_t cachesize,File file,
			     size_t reclength,enum cache_type type,
			     bool use_async_io);
extern int read_cache_record(RECORD_CACHE *info,unsigned char *to);
extern int end_record_cache(RECORD_CACHE *info);
extern int write_cache_record(RECORD_CACHE *info,my_off_t filepos,
			      const unsigned char *record,size_t length);
extern int flush_write_cache(RECORD_CACHE *info);
extern RETSIGTYPE sigtstp_handler(int signal_number);
extern void handle_recived_signals(void);

extern RETSIGTYPE my_set_alarm_variable(int signo);
extern void my_string_ptr_sort(unsigned char *base,uint32_t items,size_t size);
extern void radixsort_for_str_ptr(unsigned char* base[], uint32_t number_of_elements,
				  size_t size_of_element,unsigned char *buffer[]);
extern RETQSORTTYPE my_qsort(void *base_ptr, size_t total_elems, size_t size,
                             qsort_cmp cmp);
extern RETQSORTTYPE my_qsort2(void *base_ptr, size_t total_elems, size_t size,
                              qsort2_cmp cmp, void *cmp_argument);
extern qsort2_cmp get_ptr_compare(size_t);
void my_store_ptr(unsigned char *buff, size_t pack_length, my_off_t pos);
my_off_t my_get_ptr(unsigned char *ptr, size_t pack_length);
File create_temp_file(char *to, const char *dir, const char *pfx,
		      int mode, myf MyFlags);
#define my_init_dynamic_array(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)
#define my_init_dynamic_array_ci(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)
#define my_init_dynamic_array2(A,B,C,D,E) init_dynamic_array2(A,B,C,D,E)
#define my_init_dynamic_array2_ci(A,B,C,D,E) init_dynamic_array2(A,B,C,D,E)
extern bool init_dynamic_array2(DYNAMIC_ARRAY *array,uint32_t element_size,
                                   void *init_buffer, uint32_t init_alloc, 
                                   uint32_t alloc_increment);
/* init_dynamic_array() function is deprecated */
extern bool init_dynamic_array(DYNAMIC_ARRAY *array,uint32_t element_size,
                                  uint32_t init_alloc,uint32_t alloc_increment);
extern bool insert_dynamic(DYNAMIC_ARRAY *array,unsigned char * element);
extern unsigned char *alloc_dynamic(DYNAMIC_ARRAY *array);
extern unsigned char *pop_dynamic(DYNAMIC_ARRAY*);
extern bool set_dynamic(DYNAMIC_ARRAY *array,unsigned char * element,uint32_t array_index);
extern bool allocate_dynamic(DYNAMIC_ARRAY *array, uint32_t max_elements);
extern void get_dynamic(DYNAMIC_ARRAY *array,unsigned char * element,uint32_t array_index);
extern void delete_dynamic(DYNAMIC_ARRAY *array);
extern void delete_dynamic_element(DYNAMIC_ARRAY *array, uint32_t array_index);
extern void freeze_size(DYNAMIC_ARRAY *array);
extern int  get_index_dynamic(DYNAMIC_ARRAY *array, unsigned char * element);
#define dynamic_array_ptr(array,array_index) ((array)->buffer+(array_index)*(array)->size_of_element)
#define dynamic_element(array,array_index,type) ((type)((array)->buffer) +(array_index))
#define push_dynamic(A,B) insert_dynamic((A),(B))
#define reset_dynamic(array) ((array)->elements= 0)
#define sort_dynamic(A,cmp) my_qsort((A)->buffer, (A)->elements, (A)->size_of_element, (cmp))

#define alloc_root_inited(A) ((A)->min_malloc != 0)
#define ALLOC_ROOT_MIN_BLOCK_SIZE (MALLOC_OVERHEAD + sizeof(USED_MEM) + 8)
#define clear_alloc_root(A) do { (A)->free= (A)->used= (A)->pre_alloc= 0; (A)->min_malloc=0;} while(0)
extern int get_defaults_options(int argc, char **argv,
                                char **defaults, char **extra_defaults,
                                char **group_suffix);
extern int load_defaults(const char *conf_file, const char **groups,
			 int *argc, char ***argv);
extern int modify_defaults_file(const char *file_location, const char *option,
                                const char *option_value,
                                const char *section_name, int remove_option);
extern int my_search_option_files(const char *conf_file, int *argc,
                                  char ***argv, uint32_t *args_used,
                                  Process_option_func func, void *func_ctx);
extern void free_defaults(char **argv);
extern void my_print_default_files(const char *conf_file);
extern void print_defaults(const char *conf_file, const char **groups);
extern ha_checksum my_checksum(ha_checksum crc, const unsigned char *mem,
                               size_t count);
extern void my_sleep(uint32_t m_seconds);
extern uint32_t my_set_max_open_files(uint32_t files);
void my_free_open_file_info(void);

extern time_t my_time(myf flags);
extern uint64_t my_getsystime(void);
extern uint64_t my_micro_time(void);
extern uint64_t my_micro_time_and_time(time_t *time_arg);
time_t my_time_possible_from_micro(uint64_t microtime);
extern bool my_gethwaddr(unsigned char *to);

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>

#ifndef MAP_NOSYNC
#define MAP_NOSYNC      0
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0         /* For irix and AIX */
#endif

#ifdef HAVE_MMAP64
#define my_mmap(a,b,c,d,e,f)    mmap64(a,b,c,d,e,f)
#else
#define my_mmap(a,b,c,d,e,f)    mmap(a,b,c,d,e,f)
#endif
#define my_munmap(a,b)          munmap((a),(b))

#else
/* not a complete set of mmap() flags, but only those that nesessary */
#define PROT_READ        1
#define PROT_WRITE       2
#define MAP_NORESERVE    0
#define MAP_SHARED       0x0001
#define MAP_PRIVATE      0x0002
#define MAP_NOSYNC       0x0800
#define MAP_FAILED       ((void *)-1)
#define MS_SYNC          0x0000
#define HAVE_MMAP

void *my_mmap(void *, size_t, int, int, int, my_off_t);
int my_munmap(void *, size_t);
#endif

/* character sets */
void *cs_alloc(size_t size);

extern uint32_t get_charset_number(const char *cs_name, uint32_t cs_flags);
extern uint32_t get_collation_number(const char *name);
extern const char *get_charset_name(uint32_t cs_number);

extern const CHARSET_INFO *get_charset(uint32_t cs_number, myf flags);
extern const CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags);
extern const CHARSET_INFO *get_charset_by_csname(const char *cs_name, uint32_t cs_flags, myf my_flags);

extern bool resolve_charset(const char *cs_name,
                            const CHARSET_INFO *default_cs,
                            const CHARSET_INFO **cs);
extern bool resolve_collation(const char *cl_name,
                             const CHARSET_INFO *default_cl,
                             const CHARSET_INFO **cl);

extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern bool my_charset_same(const CHARSET_INFO *cs1, const CHARSET_INFO *cs2);
extern bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(CHARSET_INFO *cs);
extern size_t escape_string_for_drizzle(const CHARSET_INFO *charset_info,
                                        char *to, size_t to_length,
                                        const char *from, size_t length);
extern size_t escape_quotes_for_drizzle(const CHARSET_INFO *charset_info,
                                        char *to, size_t to_length,
                                        const char *from, size_t length);

extern void thd_increment_bytes_sent(uint32_t length);
extern void thd_increment_bytes_received(uint32_t length);
extern void thd_increment_net_big_packet_count(uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* _my_sys_h */
