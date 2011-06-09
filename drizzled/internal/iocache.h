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

#pragma once

#include <drizzled/internal/my_sys.h>

namespace drizzled
{
namespace internal
{

struct io_cache_st;
typedef int (*IO_CACHE_CALLBACK)(struct io_cache_st*);

struct io_cache_st    /* Used when cacheing files */
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
    A caller will use my_b_read() macro to read from the cache
    if the data is already in cache, it will be simply copied with
    memcpy() and internal variables will be accordinging updated with
    no functions invoked. However, if the data is not fully in the cache,
    my_b_read() will call read_function to fetch the data. read_function
    must never be invoked directly.
  */
  int (*read_function)(struct io_cache_st *,unsigned char *,size_t);
  /*
    Same idea as in the case of read_function, except my_b_write() needs to
    be replaced with my_b_append() for a SEQ_READ_APPEND cache
  */
  int (*write_function)(struct io_cache_st *,const unsigned char *,size_t);
  /*
    Specifies the type of the cache. Depending on the type of the cache
    certain operations might not be available and yield unpredicatable
    results. Details to be documented later
  */
  enum cache_type type;
  int error;
  /*
    Callbacks when the actual read I/O happens. These were added and
    are currently used for binary logging of LOAD DATA INFILE - when a
    block is read from the file, we create a block create/append event, and
    when io_cache_st is closed, we create an end event. These functions could,
    of course be used for other things
  */
  IO_CACHE_CALLBACK pre_read;
  IO_CACHE_CALLBACK post_read;
  IO_CACHE_CALLBACK pre_close;
  void* arg;        /* for use by pre/post_read */
  char *file_name;      /* if used with 'open_cached_file' */
  char *dir,*prefix;
  int file; /* file descriptor */
  /*
    seek_not_done is set by my_b_seek() to inform the upcoming read/write
    operation that a seek needs to be preformed prior to the actual I/O
    error is 0 if the cache operation was successful, -1 if there was a
    "hard" error, and the actual number of I/O-ed bytes if the read/write was
    partial.
  */
  int  seek_not_done;
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

  io_cache_st() :
    pos_in_file(0),
    end_of_file(0),
    read_pos(0),
    read_end(0),
    buffer(0),
    request_pos(0),
    write_buffer(0),
    append_read_pos(0),
    write_pos(0),
    write_end(0),
    current_pos(0),
    current_end(0),
    read_function(0),
    write_function(0),
    type(TYPE_NOT_SET),
    error(0),
    pre_read(0),
    post_read(0),
    pre_close(0),
    arg(0),
    file_name(0),
    dir(0),
    prefix(0),
    file(0),
    seek_not_done(0),
    buffer_length(0),
    read_length(0),
    myflags(0),
    alloced_buffer(0)
  { }

  ~io_cache_st()
  { }

  void close_cached_file();
  bool real_open_cached_file();
  int end_io_cache();
  int init_io_cache(int file, size_t cachesize,
                    enum cache_type type, my_off_t seek_offset,
                    bool use_async_io, myf cache_myflags);
  void init_functions();

  bool reinit_io_cache(enum cache_type type_arg,
                       my_off_t seek_offset,
                       bool use_async_io,
                       bool clear_cache);
  void setup_io_cache();
  bool open_cached_file(const char *dir,
                        const char *prefix, size_t cache_size,
                        myf cache_myflags);

};

typedef struct io_cache_st IO_CACHE;    /* Used when cacheing files */

extern int _my_b_get(io_cache_st *info);
extern int _my_b_async_read(io_cache_st *info,unsigned char *Buffer,size_t Count);

extern int my_block_write(io_cache_st *info, const unsigned char *Buffer,
                          size_t Count, my_off_t pos);
extern int my_b_flush_io_cache(io_cache_st *info, int need_append_buffer_lock);

#define flush_io_cache(info) my_b_flush_io_cache((info),1)

} /* namespace internal */
} /* namespace drizzled */

