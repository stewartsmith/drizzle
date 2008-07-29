/* Copyright (C) 2000-2001, 2004 MySQL AB

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

/*
  Logging of MyISAM commands and records on logfile for debugging
  The log can be examined with help of the myisamlog command.
*/

#include "myisamdef.h"

#undef GETPID					/* For HPUX */
#define GETPID() (log_type == 1 ? (long) myisam_pid : (long) my_thread_dbug_id())

	/* Activate logging if flag is 1 and reset logging if flag is 0 */

static int log_type=0;
ulong myisam_pid=0;

int mi_log(int activate_log)
{
  int error=0;
  char buff[FN_REFLEN];

  log_type=activate_log;
  if (activate_log)
  {
    if (!myisam_pid)
      myisam_pid=(ulong) getpid();
    if (myisam_log_file < 0)
    {
      if ((myisam_log_file = my_create(fn_format(buff,myisam_log_filename,
						"",".log",4),
				      0,(O_RDWR | O_BINARY | O_APPEND),MYF(0)))
	  < 0)
	return(my_errno);
    }
  }
  else if (myisam_log_file >= 0)
  {
    error=my_close(myisam_log_file,MYF(0)) ? my_errno : 0 ;
    myisam_log_file= -1;
  }
  return(error);
}


	/* Logging of records and commands on logfile */
	/* All logs starts with command(1) dfile(2) process(4) result(2) */

void _myisam_log(enum myisam_log_commands command, MI_INFO *info,
		 const uchar *buffert, uint length)
{
  uchar buff[11];
  int old_errno;
  ulong pid=(ulong) GETPID();
  old_errno=my_errno;
  memset(buff, 0, sizeof(buff));
  buff[0]=(char) command;
  mi_int2store(buff+1,info->dfile);
  mi_int4store(buff+3,pid);
  mi_int2store(buff+9,length);

  pthread_mutex_lock(&THR_LOCK_myisam);
  VOID(my_write(myisam_log_file,buff,sizeof(buff),MYF(0)));
  VOID(my_write(myisam_log_file,buffert,length,MYF(0)));
  pthread_mutex_unlock(&THR_LOCK_myisam);
  my_errno=old_errno;
}


void _myisam_log_command(enum myisam_log_commands command, MI_INFO *info,
			 const uchar *buffert, uint length, int result)
{
  uchar buff[9];
  int old_errno;
  ulong pid=(ulong) GETPID();

  old_errno=my_errno;
  buff[0]=(char) command;
  mi_int2store(buff+1,info->dfile);
  mi_int4store(buff+3,pid);
  mi_int2store(buff+7,result);
  pthread_mutex_lock(&THR_LOCK_myisam);
  VOID(my_write(myisam_log_file,buff,sizeof(buff),MYF(0)));
  if (buffert)
    VOID(my_write(myisam_log_file,buffert,length,MYF(0)));
  pthread_mutex_unlock(&THR_LOCK_myisam);
  my_errno=old_errno;
}


void _myisam_log_record(enum myisam_log_commands command, MI_INFO *info,
			const uchar *record, my_off_t filepos, int result)
{
  uchar buff[21],*pos;
  int old_errno;
  uint length;
  ulong pid=(ulong) GETPID();

  old_errno=my_errno;
  if (!info->s->base.blobs)
    length=info->s->base.reclength;
  else
    length=info->s->base.reclength+ _my_calc_total_blob_length(info,record);
  buff[0]=(uchar) command;
  mi_int2store(buff+1,info->dfile);
  mi_int4store(buff+3,pid);
  mi_int2store(buff+7,result);
  mi_sizestore(buff+9,filepos);
  mi_int4store(buff+17,length);
  pthread_mutex_lock(&THR_LOCK_myisam);
  VOID(my_write(myisam_log_file, buff,sizeof(buff),MYF(0)));
  VOID(my_write(myisam_log_file, record,info->s->base.reclength,MYF(0)));
  if (info->s->base.blobs)
  {
    MI_BLOB *blob,*end;

    for (end=info->blobs+info->s->base.blobs, blob= info->blobs;
	 blob != end ;
	 blob++)
    {
      memcpy_fixed((uchar*) &pos, record+blob->offset+blob->pack_length,
                   sizeof(char*));
      VOID(my_write(myisam_log_file,pos,blob->length,MYF(0)));
    }
  }
  pthread_mutex_unlock(&THR_LOCK_myisam);
  my_errno=old_errno;
}
