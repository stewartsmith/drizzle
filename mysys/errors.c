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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <drizzled/gettext.h>

/* Error message numbers in global map */
const char * globerrs[GLOBERRS];

void init_glob_errs()
{
  EE(EE_CANTCREATEFILE) = N_("Can't create/write to file '%s' (Errcode: %d)");
  EE(EE_READ)		= N_("Error reading file '%s' (Errcode: %d)");
  EE(EE_WRITE)		= N_("Error writing file '%s' (Errcode: %d)");
  EE(EE_BADCLOSE)	= N_("Error on close of '%s' (Errcode: %d)");
  EE(EE_OUTOFMEMORY)	= N_("Out of memory (Needed %u bytes)");
  EE(EE_DELETE)		= N_("Error on delete of '%s' (Errcode: %d)");
  EE(EE_LINK)		= N_("Error on rename of '%s' to '%s' (Errcode: %d)");
  EE(EE_EOFERR)		= N_("Unexpected eof found when reading file '%s' (Errcode: %d)");
  EE(EE_CANTLOCK)	= N_("Can't lock file (Errcode: %d)");
  EE(EE_CANTUNLOCK)	= N_("Can't unlock file (Errcode: %d)");
  EE(EE_DIR)		= N_("Can't read dir of '%s' (Errcode: %d)");
  EE(EE_STAT)		= N_("Can't get stat of '%s' (Errcode: %d)");
  EE(EE_CANT_CHSIZE)	= N_("Can't change size of file (Errcode: %d)");
  EE(EE_CANT_OPEN_STREAM)= N_("Can't open stream from handle (Errcode: %d)");
  EE(EE_GETWD)		= N_("Can't get working dirctory (Errcode: %d)");
  EE(EE_SETWD)		= N_("Can't change dir to '%s' (Errcode: %d)");
  EE(EE_LINK_WARNING)	= N_("Warning: '%s' had %d links");
  EE(EE_OPEN_WARNING)	= N_("Warning: %d files and %d streams is left open\n");
  EE(EE_DISK_FULL)	= N_("Disk is full writing '%s'. Waiting for someone to free space...");
  EE(EE_CANT_MKDIR)	= N_("Can't create directory '%s' (Errcode: %d)");
  EE(EE_UNKNOWN_CHARSET)= N_("Character set '%s' is not a compiled character set and is not specified in the %s file");
  EE(EE_OUT_OF_FILERESOURCES)= N_("Out of resources when opening file '%s' (Errcode: %d)");
  EE(EE_CANT_READLINK)= N_("Can't read value for symlink '%s' (Error %d)");
  EE(EE_CANT_SYMLINK)= N_("Can't create symlink '%s' pointing at '%s' (Error %d)");
  EE(EE_REALPATH)= N_("Error on realpath() on '%s' (Error %d)");
  EE(EE_SYNC)=	 N_("Can't sync file '%s' to disk (Errcode: %d)");
  EE(EE_UNKNOWN_COLLATION)= N_("Collation '%s' is not a compiled collation and is not specified in the %s file");
  EE(EE_FILENOTFOUND)	= N_("File '%s' not found (Errcode: %d)");
  EE(EE_FILE_NOT_CLOSED) = N_("File '%s' (fileno: %d) was not closed");
}
