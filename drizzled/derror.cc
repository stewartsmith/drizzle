/* Copyright (C) 2000-2005 MySQL AB

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


/**
  @file

  @brief
  Read language depeneded messagefile
*/

#include "mysql_priv.h"
#include <mysys/mysys_err.h>
#include <drizzled/drizzled_error_messages.h>

static void init_myfunc_errs(void);



/**
  Read messages from errorfile.

  This function can be called multiple times to reload the messages.
  If it fails to load the messages, it will fail softly by initializing
  the errmesg pointer to an array of empty strings or by keeping the
  old array if it exists.

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/

bool init_errmessage(void)
{

  /* Register messages for use with my_error(). */
  if (my_error_register(drizzled_error_messages,
                        ER_ERROR_FIRST, ER_ERROR_LAST))
  {
    return(true);
  }

  init_myfunc_errs();			/* Init myfunc messages */
  return(false);
}


/**
  Initiates error-messages used by my_func-library.
*/

static void init_myfunc_errs()
{
  init_glob_errs();			/* Initiate english errors */
  if (!(specialflag & SPECIAL_ENGLISH))
  {
    EE(EE_FILENOTFOUND)   = ER(ER_FILE_NOT_FOUND);
    EE(EE_CANTCREATEFILE) = ER(ER_CANT_CREATE_FILE);
    EE(EE_READ)           = ER(ER_ERROR_ON_READ);
    EE(EE_WRITE)          = ER(ER_ERROR_ON_WRITE);
    EE(EE_BADCLOSE)       = ER(ER_ERROR_ON_CLOSE);
    EE(EE_OUTOFMEMORY)    = ER(ER_OUTOFMEMORY);
    EE(EE_DELETE)         = ER(ER_CANT_DELETE_FILE);
    EE(EE_LINK)           = ER(ER_ERROR_ON_RENAME);
    EE(EE_EOFERR)         = ER(ER_UNEXPECTED_EOF);
    EE(EE_CANTLOCK)       = ER(ER_CANT_LOCK);
    EE(EE_DIR)            = ER(ER_CANT_READ_DIR);
    EE(EE_STAT)           = ER(ER_CANT_GET_STAT);
    EE(EE_GETWD)          = ER(ER_CANT_GET_WD);
    EE(EE_SETWD)          = ER(ER_CANT_SET_WD);
    EE(EE_DISK_FULL)      = ER(ER_DISK_FULL);
  }
}
