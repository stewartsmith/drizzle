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

/*
  Cashing of files with only does (sequential) read or writes of fixed-
  length records. A read isn't allowed to go over file-length. A read is ok
  if it ends at file-length and next read can try to read after file-length
  (and get a EOF-error).
  Possibly use of asyncronic io.
  macros for read and writes for faster io.
  Used instead of FILE when reading or writing whole files.
  This code makes mf_rec_cache obsolete (currently only used by ISAM)
  One can change info->pos_in_file to a higher value to skip bytes in file if
  also info->read_pos is set to info->read_end.
  If called through open_cached_file(), then the temporary file will
  only be created if a write exeeds the file buffer or if one calls
  my_b_flush_io_cache().

  If one uses SEQ_READ_APPEND, then two buffers are allocated, one for
  reading and another for writing.  Reads are first done from disk and
  then done from the write buffer.  This is an efficient way to read
  from a log file when one is writing to it at the same time.
  For this to work, the file has to be opened in append mode!
  Note that when one uses SEQ_READ_APPEND, one MUST write using
  my_b_append !  This is needed because we need to lock the mutex
  every time we access the write buffer.

TODO:
  When one SEQ_READ_APPEND and we are reading and writing at the same time,
  each time the write buffer gets full and it's written to disk, we will
  always do a disk read to read a part of the buffer from disk to the
  read buffer.
  This should be fixed so that when we do a my_b_flush_io_cache() and
  we have been reading the write buffer, we should transfer the rest of the
  write buffer to the read buffer before we start to reuse it.
*/

#include <config.h>

#include <drizzled/definitions.h>
#include <drizzled/error_t.h>
#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/drizzled.h>
#include <drizzled/internal/iocache.h>
#include <errno.h>
#include <drizzled/util/test.h>
#include <stdlib.h>
#include <algorithm>

using namespace std;

namespace drizzled
{
namespace internal
{

static int _my_b_read(io_cache_st *info, unsigned char *Buffer, size_t Count);
static int _my_b_write(io_cache_st *info, const unsigned char *Buffer, size_t Count);

/**
 * @brief
 *   Lock appends for the io_cache_st if required (need_append_buffer_lock)   
 */
inline
static void lock_append_buffer(io_cache_st *, int )
{
}

/**
 * @brief
 *   Unlock appends for the io_cache_st if required (need_append_buffer_lock)   
 */
inline
static void unlock_append_buffer(io_cache_st *, int )
{
}

/**
 * @brief
 *   Round up to the nearest IO_SIZE boundary 
 */
inline
static size_t io_round_up(size_t x)
{
  return ((x+IO_SIZE-1) & ~(IO_SIZE-1));
}

/**
 * @brief
 *   Round down to the nearest IO_SIZE boundary   
 */
inline
static size_t io_round_dn(size_t x)
{
  return (x & ~(IO_SIZE-1));
}


/**
 * @brief 
 *   Setup internal pointers inside io_cache_st
 * 
 * @details
 *   This is called on automatically on init or reinit of io_cache_st
 *   It must be called externally if one moves or copies an io_cache_st object.
 * 
 * @param info Cache handler to setup
 */
void io_cache_st::setup_io_cache()
{
  /* Ensure that my_b_tell() and my_b_bytes_in_cache works */
  if (type == WRITE_CACHE)
  {
    current_pos= &write_pos;
    current_end= &write_end;
  }
  else
  {
    current_pos= &read_pos;
    current_end= &read_end;
  }
}


void io_cache_st::init_functions()
{
  switch (type) {
  case READ_NET:
    /*
      Must be initialized by the caller. The problem is that
      _my_b_net_read has to be defined in sql directory because of
      the dependency on THD, and therefore cannot be visible to
      programs that link against mysys but know nothing about THD, such
      as myisamchk
    */
    break;
  default:
    read_function = _my_b_read;
    write_function = _my_b_write;
  }

  setup_io_cache();
}

/**
 * @brief
 *   Initialize an io_cache_st object
 *
 * @param file File that should be associated with the handler
 *                 If == -1 then real_open_cached_file() will be called when it's time to open file.
 * @param cachesize Size of buffer to allocate for read/write
 *                      If == 0 then use my_default_record_cache_size
 * @param type Type of cache
 * @param seek_offset Where cache should start reading/writing
 * @param use_async_io Set to 1 if we should use async_io (if avaiable)
 * @param cache_myflags Bitmap of different flags
                            MY_WME | MY_FAE | MY_NABP | MY_FNABP | MY_DONT_CHECK_FILESIZE
 * 
 * @retval 0 success
 * @retval # error
 */
int io_cache_st::init_io_cache(int file_arg, size_t cachesize,
                               enum cache_type type_arg, my_off_t seek_offset,
                               bool use_async_io, myf cache_myflags)
{
  size_t min_cache;
  off_t pos;
  my_off_t end_of_file_local= ~(my_off_t) 0;

  file= file_arg;
  type= TYPE_NOT_SET;	    /* Don't set it until mutex are created */
  pos_in_file= seek_offset;
  pre_close = pre_read = post_read = 0;
  arg = 0;
  alloced_buffer = 0;
  buffer=0;
  seek_not_done= 0;

  if (file >= 0)
  {
    pos= lseek(file, 0, SEEK_CUR);
    if ((pos == MY_FILEPOS_ERROR) && (errno == ESPIPE))
    {
      /*
         This kind of object doesn't support seek() or tell(). Don't set a
         flag that will make us again try to seek() later and fail.
      */
      seek_not_done= 0;
      /*
        Additionally, if we're supposed to start somewhere other than the
        the beginning of whatever this file is, then somebody made a bad
        assumption.
      */
      assert(seek_offset == 0);
    }
    else
      seek_not_done= test(seek_offset != (my_off_t)pos);
  }

  if (!cachesize && !(cachesize= my_default_record_cache_size))
    return 1;				/* No cache requested */
  min_cache=use_async_io ? IO_SIZE*4 : IO_SIZE*2;
  if (type_arg == READ_CACHE)
  {						/* Assume file isn't growing */
    if (!(cache_myflags & MY_DONT_CHECK_FILESIZE))
    {
      /* Calculate end of file to avoid allocating oversized buffers */
      end_of_file_local=lseek(file,0L,SEEK_END);
      /* Need to reset seek_not_done now that we just did a seek. */
      seek_not_done= end_of_file_local == seek_offset ? 0 : 1;
      if (end_of_file_local < seek_offset)
	end_of_file_local=seek_offset;
      /* Trim cache size if the file is very small */
      if ((my_off_t) cachesize > end_of_file_local-seek_offset+IO_SIZE*2-1)
      {
	cachesize= (size_t) (end_of_file_local-seek_offset)+IO_SIZE*2-1;
	use_async_io=0;				/* No need to use async */
      }
    }
  }
  cache_myflags &= ~MY_DONT_CHECK_FILESIZE;
  if (type_arg != READ_NET && type_arg != WRITE_NET)
  {
    /* Retry allocating memory in smaller blocks until we get one */
    cachesize= ((cachesize + min_cache-1) & ~(min_cache-1));
    if (cachesize < min_cache)
      cachesize = min_cache;
    size_t buffer_block= cachesize;
    if (type_arg == READ_CACHE && not global_read_buffer.add(buffer_block))
    {
      my_error(ER_OUT_OF_GLOBAL_READMEMORY, MYF(ME_ERROR+ME_WAITTANG));
      return 2;
    }

    buffer= (unsigned char*) malloc(buffer_block);
    write_buffer=buffer;
    alloced_buffer= true;
  }

  read_length=buffer_length=cachesize;
  myflags=cache_myflags & ~(MY_NABP | MY_FNABP);
  request_pos= read_pos= write_pos = buffer;

  if (type_arg == WRITE_CACHE)
    write_end=
      buffer+buffer_length- (seek_offset & (IO_SIZE-1));
  else
    read_end=buffer;		/* Nothing in cache */

  /* End_of_file may be changed by user later */
  end_of_file= end_of_file_local;
  error= 0;
  type= type_arg;
  init_functions();
  return 0;
}						/* init_io_cache */

/**
 * @brief 
 *   Reset the cache
 * 
 * @detail
 *   Use this to reset cache to re-start reading or to change the type 
 *   between READ_CACHE <-> WRITE_CACHE
 *   If we are doing a reinit of a cache where we have the start of the file
 *   in the cache, we are reusing this memory without flushing it to disk.
 */
bool io_cache_st::reinit_io_cache(enum cache_type type_arg,
                                  my_off_t seek_offset,
                                  bool,
                                  bool clear_cache)
{
  /* One can't do reinit with the following types */
  assert(type_arg != READ_NET && type != READ_NET &&
	      type_arg != WRITE_NET && type != WRITE_NET);

  /* If the whole file is in memory, avoid flushing to disk */
  if (! clear_cache &&
      seek_offset >= pos_in_file &&
      seek_offset <= my_b_tell(this))
  {
    /* Reuse current buffer without flushing it to disk */
    unsigned char *pos;
    if (type == WRITE_CACHE && type_arg == READ_CACHE)
    {
      read_end=write_pos;
      end_of_file=my_b_tell(this);
      /*
        Trigger a new seek only if we have a valid
        file handle.
      */
      seek_not_done= (file != -1);
    }
    else if (type_arg == WRITE_CACHE)
    {
      if (type == READ_CACHE)
      {
	write_end=write_buffer+buffer_length;
	seek_not_done=1;
      }
      end_of_file = ~(my_off_t) 0;
    }
    pos=request_pos+(seek_offset-pos_in_file);
    if (type_arg == WRITE_CACHE)
      write_pos=pos;
    else
      read_pos= pos;
  }
  else
  {
    /*
      If we change from WRITE_CACHE to READ_CACHE, assume that everything
      after the current positions should be ignored
    */
    if (type == WRITE_CACHE && type_arg == READ_CACHE)
      end_of_file=my_b_tell(this);
    /* flush cache if we want to reuse it */
    if (!clear_cache && my_b_flush_io_cache(this, 1))
      return 1;
    pos_in_file=seek_offset;
    /* Better to do always do a seek */
    seek_not_done=1;
    request_pos=read_pos=write_pos=buffer;
    if (type_arg == READ_CACHE)
    {
      read_end=buffer;		/* Nothing in cache */
    }
    else
    {
      write_end=(buffer + buffer_length -
		       (seek_offset & (IO_SIZE-1)));
      end_of_file= ~(my_off_t) 0;
    }
  }
  type= type_arg;
  error=0;
  init_functions();

  return 0;
} /* reinit_io_cache */

/**
 * @brief 
 *   Read buffered.
 * 
 * @detail
 *   This function is only called from the my_b_read() macro when there
 *   aren't enough characters in the buffer to satisfy the request.
 *
 * WARNING
 *   When changing this function, be careful with handling file offsets
 *   (end-of_file, pos_in_file). Do not cast them to possibly smaller
 *   types than my_off_t unless you can be sure that their value fits.
 *   Same applies to differences of file offsets.
 *
 * @param info io_cache_st pointer @param Buffer Buffer to retrieve count bytes
 * from file @param Count Number of bytes to read into Buffer
 * 
 * @retval 0 We succeeded in reading all data
 * @retval 1 Error: can't read requested characters
 */
static int _my_b_read(io_cache_st *info, unsigned char *Buffer, size_t Count)
{
  size_t length_local,diff_length,left_length, max_length;
  my_off_t pos_in_file_local;

  if ((left_length= (size_t) (info->read_end-info->read_pos)))
  {
    assert(Count >= left_length);	/* User is not using my_b_read() */
    memcpy(Buffer,info->read_pos, left_length);
    Buffer+=left_length;
    Count-=left_length;
  }

  /* pos_in_file always point on where info->buffer was read */
  pos_in_file_local=info->pos_in_file+ (size_t) (info->read_end - info->buffer);

  /*
    Whenever a function which operates on io_cache_st flushes/writes
    some part of the io_cache_st to disk it will set the property
    "seek_not_done" to indicate this to other functions operating
    on the io_cache_st.
  */
  if (info->seek_not_done)
  {
    if ((lseek(info->file,pos_in_file_local,SEEK_SET) != MY_FILEPOS_ERROR))
    {
      /* No error, reset seek_not_done flag. */
      info->seek_not_done= 0;
    }
    else
    {
      /*
        If the seek failed and the error number is ESPIPE, it is because
        info->file is a pipe or socket or FIFO.  We never should have tried
        to seek on that.  See Bugs#25807 and #22828 for more info.
      */
      assert(errno != ESPIPE);
      info->error= -1;
      return 1;
    }
  }

  diff_length= (size_t) (pos_in_file_local & (IO_SIZE-1));
  if (Count >= (size_t) (IO_SIZE+(IO_SIZE-diff_length)))
  {					/* Fill first intern buffer */
    size_t read_length;
    if (info->end_of_file <= pos_in_file_local)
    {					/* End of file */
      info->error= (int) left_length;
      return 1;
    }
    length_local=(Count & (size_t) ~(IO_SIZE-1))-diff_length;
    if ((read_length= my_read(info->file,Buffer, length_local, info->myflags)) != length_local)
    {
      info->error= (read_length == (size_t) -1 ? -1 :
		    (int) (read_length+left_length));
      return 1;
    }
    Count-= length_local;
    Buffer+= length_local;
    pos_in_file_local+= length_local;
    left_length+= length_local;
    diff_length=0;
  }

  max_length= info->read_length-diff_length;
  if (info->type != READ_FIFO &&
      max_length > (info->end_of_file - pos_in_file_local))
    max_length= (size_t) (info->end_of_file - pos_in_file_local);
  if (!max_length)
  {
    if (Count)
    {
      info->error= static_cast<int>(left_length);	/* We only got this many char */
      return 1;
    }
     length_local=0;				/* Didn't read any chars */
  }
  else if (( length_local= my_read(info->file,info->buffer, max_length,
                            info->myflags)) < Count ||
	    length_local == (size_t) -1)
  {
    if ( length_local != (size_t) -1)
      memcpy(Buffer, info->buffer,  length_local);
    info->pos_in_file= pos_in_file_local;
    info->error=  length_local == (size_t) -1 ? -1 : (int) ( length_local+left_length);
    info->read_pos=info->read_end=info->buffer;
    return 1;
  }
  info->read_pos=info->buffer+Count;
  info->read_end=info->buffer+ length_local;
  info->pos_in_file=pos_in_file_local;
  memcpy(Buffer, info->buffer, Count);
  return 0;
}

/**
 * @brief
 *   Read one byte when buffer is empty
 */
int _my_b_get(io_cache_st *info)
{
  unsigned char buff;
  IO_CACHE_CALLBACK pre_read,post_read;

  if ((pre_read = info->pre_read))
    (*pre_read)(info);

  if ((*(info)->read_function)(info,&buff,1))
    return my_b_EOF;

  if ((post_read = info->post_read))
    (*post_read)(info);

  return (int) (unsigned char) buff;
}

/**
 * @brief
 *   Write a byte buffer to io_cache_st and flush to disk if io_cache_st is full.
 *
 * @retval -1 On error; errno contains error code.
 * @retval 0 On success
 * @retval 1 On error on write
 */
int _my_b_write(io_cache_st *info, const unsigned char *Buffer, size_t Count)
{
  size_t rest_length,length_local;

  if (info->pos_in_file+info->buffer_length > info->end_of_file)
  {
    errno=EFBIG;
    return info->error = -1;
  }

  rest_length= (size_t) (info->write_end - info->write_pos);
  memcpy(info->write_pos,Buffer,(size_t) rest_length);
  Buffer+=rest_length;
  Count-=rest_length;
  info->write_pos+=rest_length;

  if (my_b_flush_io_cache(info,1))
    return 1;
  if (Count >= IO_SIZE)
  {					/* Fill first intern buffer */
    length_local=Count & (size_t) ~(IO_SIZE-1);
    if (info->seek_not_done)
    {
      /*
        Whenever a function which operates on io_cache_st flushes/writes
        some part of the io_cache_st to disk it will set the property
        "seek_not_done" to indicate this to other functions operating
        on the io_cache_st.
      */
      if (lseek(info->file,info->pos_in_file,SEEK_SET))
      {
        info->error= -1;
        return (1);
      }
      info->seek_not_done=0;
    }
    if (my_write(info->file, Buffer, length_local, info->myflags | MY_NABP))
      return info->error= -1;

    Count-=length_local;
    Buffer+=length_local;
    info->pos_in_file+=length_local;
  }
  memcpy(info->write_pos,Buffer,(size_t) Count);
  info->write_pos+=Count;
  return 0;
}

/**
 * @brief
 *   Write a block to disk where part of the data may be inside the record buffer.
 *   As all write calls to the data goes through the cache,
 *   we will never get a seek over the end of the buffer.
 */
int my_block_write(io_cache_st *info, const unsigned char *Buffer, size_t Count,
		   my_off_t pos)
{
  size_t length_local;
  int error=0;

  if (pos < info->pos_in_file)
  {
    /* Of no overlap, write everything without buffering */
    if (pos + Count <= info->pos_in_file)
      return (pwrite(info->file, Buffer, Count, pos) == 0);
    /* Write the part of the block that is before buffer */
    length_local= (uint32_t) (info->pos_in_file - pos);
    if (pwrite(info->file, Buffer, length_local, pos) == 0)
      info->error= error= -1;
    Buffer+=length_local;
    pos+=  length_local;
    Count-= length_local;
  }

  /* Check if we want to write inside the used part of the buffer.*/
  length_local= (size_t) (info->write_end - info->buffer);
  if (pos < info->pos_in_file + length_local)
  {
    size_t offset= (size_t) (pos - info->pos_in_file);
    length_local-=offset;
    if (length_local > Count)
      length_local=Count;
    memcpy(info->buffer+offset, Buffer, length_local);
    Buffer+=length_local;
    Count-= length_local;
    /* Fix length_local of buffer if the new data was larger */
    if (info->buffer+length_local > info->write_pos)
      info->write_pos=info->buffer+length_local;
    if (!Count)
      return (error);
  }
  /* Write at the end of the current buffer; This is the normal case */
  if (_my_b_write(info, Buffer, Count))
    error= -1;
  return error;
}

/**
 * @brief
 *   Flush write cache 
 */
int my_b_flush_io_cache(io_cache_st *info, int need_append_buffer_lock)
{
  size_t length_local;
  bool append_cache= false;
  my_off_t pos_in_file_local;

  if (info->type == WRITE_CACHE || append_cache)
  {
    if (info->file == -1)
    {
      if (info->real_open_cached_file())
	return((info->error= -1));
    }
    lock_append_buffer(info, need_append_buffer_lock);

    if ((length_local=(size_t) (info->write_pos - info->write_buffer)))
    {
      pos_in_file_local=info->pos_in_file;
      /*
	If we have append cache, we always open the file with
	O_APPEND which moves the pos to EOF automatically on every write
      */
      if (!append_cache && info->seek_not_done)
      {					/* File touched, do seek */
	if (lseek(info->file,pos_in_file_local,SEEK_SET) == MY_FILEPOS_ERROR)
	{
	  unlock_append_buffer(info, need_append_buffer_lock);
	  return((info->error= -1));
	}
	if (!append_cache)
	  info->seek_not_done=0;
      }
      if (!append_cache)
	info->pos_in_file+=length_local;
      info->write_end= (info->write_buffer+info->buffer_length-
			((pos_in_file_local+length_local) & (IO_SIZE-1)));

      if (my_write(info->file,info->write_buffer,length_local,
		   info->myflags | MY_NABP))
	info->error= -1;
      else
	info->error= 0;
      if (!append_cache)
      {
        set_if_bigger(info->end_of_file,(pos_in_file_local+length_local));
      }
      else
      {
	info->end_of_file+=(info->write_pos-info->append_read_pos);
	my_off_t tell_ret= lseek(info->file, 0, SEEK_CUR);
	assert(info->end_of_file == tell_ret);
      }

      info->append_read_pos=info->write_pos=info->write_buffer;
      unlock_append_buffer(info, need_append_buffer_lock);
      return(info->error);
    }
  }
  unlock_append_buffer(info, need_append_buffer_lock);
  return 0;
}

/**
 * @brief
 *   Free an io_cache_st object
 * 
 * @detail
 *   It's currently safe to call this if one has called init_io_cache()
 *   on the 'info' object, even if init_io_cache() failed.
 *   This function is also safe to call twice with the same handle.
 * 
 * @param info io_cache_st Handle to free
 * 
 * @retval 0 ok
 * @retval # Error
 */
int io_cache_st::end_io_cache()
{
  int _error=0;

  if (pre_close)
  {
    (*pre_close)(this);
    pre_close= 0;
  }
  if (alloced_buffer)
  {
    if (type == READ_CACHE)
      global_read_buffer.sub(buffer_length);
    alloced_buffer=0;
    if (file != -1)			/* File doesn't exist */
      _error= my_b_flush_io_cache(this, 1);
    free((unsigned char*) buffer);
    buffer=read_pos=(unsigned char*) 0;
  }

  return _error;
} /* end_io_cache */

} /* namespace internal */
} /* namespace drizzled */
