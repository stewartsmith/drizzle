/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef mysys_iocache_h
#define mysys_iocache_h

#if defined(__cplusplus)
extern "C" {
#endif

struct st_io_cache;
typedef int (*IO_CACHE_CALLBACK)(struct st_io_cache*);

typedef struct st_io_cache_share
{
  pthread_mutex_t       mutex;           /* To sync on reads into buffer. */
  pthread_cond_t        cond;            /* To wait for signals. */
  pthread_cond_t        cond_writer;     /* For a synchronized writer. */
  /* Offset in file corresponding to the first byte of buffer. */
  my_off_t              pos_in_file;
  /* If a synchronized write cache is the source of the data. */
  struct st_io_cache    *source_cache;
  unsigned char                 *buffer;         /* The read buffer. */
  unsigned char                 *read_end;       /* Behind last valid byte of buffer. */
  int                   running_threads; /* threads not in lock. */
  int                   total_threads;   /* threads sharing the cache. */
  int                   error;           /* Last error. */
#ifdef NOT_YET_IMPLEMENTED
  /* whether the structure should be free'd */
  bool alloced;
#endif
} IO_CACHE_SHARE;

typedef struct st_io_cache    /* Used when cacheing files */
{
  /* Offset in file corresponding to the first byte of unsigned char* buffer. */
  my_off_t pos_in_file;
  /*
    The offset of end of file for READ_CACHE and WRITE_CACHE.
    For SEQ_READ_APPEND it the maximum of the actual end of file and
    the position represented by read_end.
  */
  my_off_t end_of_file;
  /* Points to current read position in the buffer */
  unsigned char  *read_pos;
  /* the non-inclusive boundary in the buffer for the currently valid read */
  unsigned char  *read_end;
  unsigned char  *buffer;        /* The read buffer */
  /* Used in ASYNC_IO */
  unsigned char  *request_pos;

  /* Only used in WRITE caches and in SEQ_READ_APPEND to buffer writes */
  unsigned char  *write_buffer;
  /*
    Only used in SEQ_READ_APPEND, and points to the current read position
    in the write buffer. Note that reads in SEQ_READ_APPEND caches can
    happen from both read buffer (unsigned char* buffer) and write buffer
    (unsigned char* write_buffer).
  */
  unsigned char *append_read_pos;
  /* Points to current write position in the write buffer */
  unsigned char *write_pos;
  /* The non-inclusive boundary of the valid write area */
  unsigned char *write_end;

  /*
    Current_pos and current_end are convenience variables used by
    my_b_tell() and other routines that need to know the current offset
    current_pos points to &write_pos, and current_end to &write_end in a
    WRITE_CACHE, and &read_pos and &read_end respectively otherwise
  */
  unsigned char  **current_pos, **current_end;
  /*
    The lock is for append buffer used in SEQ_READ_APPEND cache
    need mutex copying from append buffer to read buffer.
  */
  pthread_mutex_t append_buffer_lock;
  /*
    The following is used when several threads are reading the
    same file in parallel. They are synchronized on disk
    accesses reading the cached part of the file asynchronously.
    It should be set to NULL to disable the feature.  Only
    READ_CACHE mode is supported.
  */
  IO_CACHE_SHARE *share;
  /*
    A caller will use my_b_read() macro to read from the cache
    if the data is already in cache, it will be simply copied with
    memcpy() and internal variables will be accordinging updated with
    no functions invoked. However, if the data is not fully in the cache,
    my_b_read() will call read_function to fetch the data. read_function
    must never be invoked directly.
  */
  int (*read_function)(struct st_io_cache *,unsigned char *,size_t);
  /*
    Same idea as in the case of read_function, except my_b_write() needs to
    be replaced with my_b_append() for a SEQ_READ_APPEND cache
  */
  int (*write_function)(struct st_io_cache *,const unsigned char *,size_t);
  /*
    Specifies the type of the cache. Depending on the type of the cache
    certain operations might not be available and yield unpredicatable
    results. Details to be documented later
  */
  enum cache_type type;
  /*
    Callbacks when the actual read I/O happens. These were added and
    are currently used for binary logging of LOAD DATA INFILE - when a
    block is read from the file, we create a block create/append event, and
    when IO_CACHE is closed, we create an end event. These functions could,
    of course be used for other things
  */
  IO_CACHE_CALLBACK pre_read;
  IO_CACHE_CALLBACK post_read;
  IO_CACHE_CALLBACK pre_close;
  /*
    Counts the number of times, when we were forced to use disk. We use it to
    increase the binlog_cache_disk_use status variable.
  */
  uint32_t disk_writes;
  void* arg;        /* for use by pre/post_read */
  char *file_name;      /* if used with 'open_cached_file' */
  char *dir,*prefix;
  File file; /* file descriptor */
  /*
    seek_not_done is set by my_b_seek() to inform the upcoming read/write
    operation that a seek needs to be preformed prior to the actual I/O
    error is 0 if the cache operation was successful, -1 if there was a
    "hard" error, and the actual number of I/O-ed bytes if the read/write was
    partial.
  */
  int  seek_not_done,error;
  /* buffer_length is memory size allocated for buffer or write_buffer */
  size_t  buffer_length;
  /* read_length is the same as buffer_length except when we use async io */
  size_t  read_length;
  myf  myflags;      /* Flags used to my_read/my_write */
  /*
    alloced_buffer is 1 if the buffer was allocated by init_io_cache() and
    0 if it was supplied by the user.
    Currently READ_NET is the only one that will use a buffer allocated
    somewhere else
  */
  bool alloced_buffer;
#ifdef HAVE_AIOWAIT
  /*
    As inidicated by ifdef, this is for async I/O, which is not currently
    used (because it's not reliable on all systems)
  */
  uint32_t inited;
  my_off_t aio_read_pos;
  my_aio_result aio_result;
#endif
} IO_CACHE;

/* tell write offset in the SEQ_APPEND cache */
int      my_b_copy_to_file(IO_CACHE *cache, FILE *file);
my_off_t my_b_append_tell(IO_CACHE* info);
my_off_t my_b_safe_tell(IO_CACHE* info); /* picks the correct tell() */

extern int init_io_cache(IO_CACHE *info,File file,size_t cachesize,
                         enum cache_type type,my_off_t seek_offset,
                         bool use_async_io, myf cache_myflags);
extern bool reinit_io_cache(IO_CACHE *info,enum cache_type type,
                            my_off_t seek_offset,bool use_async_io,
                            bool clear_cache);
extern void setup_io_cache(IO_CACHE* info);
extern int _my_b_read(IO_CACHE *info,unsigned char *Buffer,size_t Count);
extern int _my_b_read_r(IO_CACHE *info,unsigned char *Buffer,size_t Count);
extern void init_io_cache_share(IO_CACHE *read_cache, IO_CACHE_SHARE *cshare,
                                IO_CACHE *write_cache, uint32_t num_threads);
extern void remove_io_thread(IO_CACHE *info);
extern int _my_b_seq_read(IO_CACHE *info,unsigned char *Buffer,size_t Count);
extern int _my_b_net_read(IO_CACHE *info,unsigned char *Buffer,size_t Count);
extern int _my_b_get(IO_CACHE *info);
extern int _my_b_async_read(IO_CACHE *info,unsigned char *Buffer,size_t Count);
extern int _my_b_write(IO_CACHE *info,const unsigned char *Buffer,
                       size_t Count);
extern int my_b_append(IO_CACHE *info,const unsigned char *Buffer,
                       size_t Count);
extern int my_b_safe_write(IO_CACHE *info,const unsigned char *Buffer,
                           size_t Count);

extern int my_block_write(IO_CACHE *info, const unsigned char *Buffer,
                          size_t Count, my_off_t pos);
extern int my_b_flush_io_cache(IO_CACHE *info, int need_append_buffer_lock);

#define flush_io_cache(info) my_b_flush_io_cache((info),1)

extern int end_io_cache(IO_CACHE *info);
extern size_t my_b_fill(IO_CACHE *info);
extern void my_b_seek(IO_CACHE *info,my_off_t pos);
extern size_t my_b_gets(IO_CACHE *info, char *to, size_t max_length);
extern my_off_t my_b_filelength(IO_CACHE *info);
extern size_t my_b_printf(IO_CACHE *info, const char* fmt, ...);
extern size_t my_b_vprintf(IO_CACHE *info, const char* fmt, va_list ap);
extern bool open_cached_file(IO_CACHE *cache,const char *dir,
                             const char *prefix, size_t cache_size,
                             myf cache_myflags);
extern bool real_open_cached_file(IO_CACHE *cache);
extern void close_cached_file(IO_CACHE *cache);

#if defined(__cplusplus)
}
#endif

#endif
