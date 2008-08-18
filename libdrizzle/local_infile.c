/* Copyright (C) 2000-2004 DRIZZLE AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software. View the full text of the exception in file
   EXCEPTIONS-CLIENT in the directory of this software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <drizzled/global.h>
#include "my_time.h"
#include "drizzle.h"
#include "errmsg.h"
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef   HAVE_PWD_H
#include <pwd.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE  -1
#endif

#include <sql_common.h>
#include "client_settings.h"


bool handle_local_infile(DRIZZLE *drizzle, const char *net_filename)
{
  bool result= true;
  uint packet_length=MY_ALIGN(drizzle->net.max_packet-16,IO_SIZE);
  NET *net= &drizzle->net;
  int readcount;
  void *li_ptr;          /* pass state to local_infile functions */
  char *buf;    /* buffer to be filled by local_infile_read */
  struct st_drizzle_options *options= &drizzle->options;

  /* check that we've got valid callback functions */
  if (!(options->local_infile_init &&
  options->local_infile_read &&
  options->local_infile_end &&
  options->local_infile_error))
  {
    /* if any of the functions is invalid, set the default */
    drizzle_set_local_infile_default(drizzle);
  }

  /* copy filename into local memory and allocate read buffer */
  if (!(buf=malloc(packet_length)))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
    return(1);
  }

  /* initialize local infile (open file, usually) */
  if ((*options->local_infile_init)(&li_ptr, net_filename,
    options->local_infile_userdata))
  {
    VOID(my_net_write(net,(const uchar*) "",0)); /* Server needs one packet */
    net_flush(net);
    strcpy(net->sqlstate, unknown_sqlstate);
    net->last_errno=
      (*options->local_infile_error)(li_ptr,
                                     net->last_error,
                                     sizeof(net->last_error)-1);
    goto err;
  }

  /* read blocks of data from local infile callback */
  while ((readcount =
    (*options->local_infile_read)(li_ptr, buf,
          packet_length)) > 0)
  {
    if (my_net_write(net, (uchar*) buf, readcount))
    {
      goto err;
    }
  }

  /* Send empty packet to mark end of file */
  if (my_net_write(net, (const uchar*) "", 0) || net_flush(net))
  {
    set_drizzle_error(drizzle, CR_SERVER_LOST, unknown_sqlstate);
    goto err;
  }

  if (readcount < 0)
  {
    net->last_errno=
      (*options->local_infile_error)(li_ptr,
                                     net->last_error,
                                     sizeof(net->last_error)-1);
    goto err;
  }

  result=false;					/* Ok */

err:
  /* free up memory allocated with _init, usually */
  (*options->local_infile_end)(li_ptr);
  if(buf)
    free(buf);
  return(result);
}


/****************************************************************************
  Default handlers for LOAD LOCAL INFILE
****************************************************************************/

typedef struct st_default_local_infile
{
  int fd;
  int error_num;
  const char *filename;
  char error_msg[LOCAL_INFILE_ERROR_LEN];
} default_local_infile_data;


/*
  Open file for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_init()
    ptr      Store pointer to internal data here
    filename    File name to open. This may be in unix format !


  NOTES
    Even if this function returns an error, the load data interface
    guarantees that default_local_infile_end() is called.

  RETURN
    0  ok
    1  error
*/

static int default_local_infile_init(void **ptr, const char *filename,
             void *userdata __attribute__ ((unused)))
{
  default_local_infile_data *data;
  char tmp_name[FN_REFLEN];

  if (!(*ptr= data= ((default_local_infile_data *)
		     malloc(sizeof(default_local_infile_data)))))
    return 1; /* out of memory */

  data->error_msg[0]= 0;
  data->error_num=    0;
  data->filename= filename;

  if ((data->fd = open(tmp_name, O_RDONLY)) < 0)
  {
    data->error_num= errno;
    snprintf(data->error_msg, sizeof(data->error_msg)-1,
             _("File '%s' not found (Errcode: %d)"), tmp_name, data->error_num);
    return 1;
  }
  return 0; /* ok */
}


/*
  Read data for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_read()
    ptr      Points to handle allocated by _init
    buf      Read data here
    buf_len    Ammount of data to read

  RETURN
    > 0    number of bytes read
    == 0  End of data
    < 0    Error
*/

static int default_local_infile_read(void *ptr, char *buf, uint buf_len)
{
  int count;
  default_local_infile_data*data = (default_local_infile_data *) ptr;

  if ((count= (int) read(data->fd, (uchar *) buf, buf_len)) < 0)
  {
    data->error_num= 2; /* the errmsg for not entire file read */
    snprintf(data->error_msg, sizeof(data->error_msg)-1,
             _("Error reading file '%s' (Errcode: %d)"),
             data->filename, errno);
  }
  return count;
}


/*
  Read data for LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_end()
    ptr      Points to handle allocated by _init
      May be NULL if _init failed!

  RETURN
*/

static void default_local_infile_end(void *ptr)
{
  default_local_infile_data *data= (default_local_infile_data *) ptr;
  if (data)          /* If not error on open */
  {
    if (data->fd >= 0)
      close(data->fd);
    free(ptr);
  }
}


/*
  Return error from LOAD LOCAL INFILE

  SYNOPSIS
    default_local_infile_end()
    ptr      Points to handle allocated by _init
      May be NULL if _init failed!
    error_msg    Store error text here
    error_msg_len  Max lenght of error_msg

  RETURN
    error message number
*/

static int
default_local_infile_error(void *ptr, char *error_msg, uint error_msg_len)
{
  default_local_infile_data *data = (default_local_infile_data *) ptr;
  if (data)          /* If not error on open */
  {
    strncpy(error_msg, data->error_msg, error_msg_len);
    return data->error_num;
  }
  /* This can only happen if we got error on malloc of handle */
  strcpy(error_msg, ER(CR_OUT_OF_MEMORY));
  return CR_OUT_OF_MEMORY;
}


void
drizzle_set_local_infile_handler(DRIZZLE *drizzle,
                               int (*local_infile_init)(void **, const char *,
                               void *),
                               int (*local_infile_read)(void *, char *, uint),
                               void (*local_infile_end)(void *),
                               int (*local_infile_error)(void *, char *, uint),
                               void *userdata)
{
  drizzle->options.local_infile_init=  local_infile_init;
  drizzle->options.local_infile_read=  local_infile_read;
  drizzle->options.local_infile_end=   local_infile_end;
  drizzle->options.local_infile_error= local_infile_error;
  drizzle->options.local_infile_userdata = userdata;
}


void drizzle_set_local_infile_default(DRIZZLE *drizzle)
{
  drizzle->options.local_infile_init=  default_local_infile_init;
  drizzle->options.local_infile_read=  default_local_infile_read;
  drizzle->options.local_infile_end=   default_local_infile_end;
  drizzle->options.local_infile_error= default_local_infile_error;
}
