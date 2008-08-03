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
#include <libdrizzle/gettext.h>

/* Error message numbers in global map */
const char * globerrs[GLOBERRS];

void init_glob_errs()
{
  EE(EE_CANTCREATEFILE) = gettext_noop("Can't create/write to file '%s' (Errcode: %d)");
  EE(EE_READ)		= gettext_noop("Error reading file '%s' (Errcode: %d)");
  EE(EE_WRITE)		= gettext_noop("Error writing file '%s' (Errcode: %d)");
  EE(EE_BADCLOSE)	= gettext_noop("Error on close of '%'s (Errcode: %d)");
  EE(EE_OUTOFMEMORY)	= gettext_noop("Out of memory (Needed %u bytes)");
  EE(EE_DELETE)		= gettext_noop("Error on delete of '%s' (Errcode: %d)");
  EE(EE_LINK)		= gettext_noop("Error on rename of '%s' to '%s' (Errcode: %d)");
  EE(EE_EOFERR)		= gettext_noop("Unexpected eof found when reading file '%s' (Errcode: %d)");
  EE(EE_CANTLOCK)	= gettext_noop("Can't lock file (Errcode: %d)");
  EE(EE_CANTUNLOCK)	= gettext_noop("Can't unlock file (Errcode: %d)");
  EE(EE_DIR)		= gettext_noop("Can't read dir of '%s' (Errcode: %d)");
  EE(EE_STAT)		= gettext_noop("Can't get stat of '%s' (Errcode: %d)");
  EE(EE_CANT_CHSIZE)	= gettext_noop("Can't change size of file (Errcode: %d)");
  EE(EE_CANT_OPEN_STREAM)= gettext_noop("Can't open stream from handle (Errcode: %d)");
  EE(EE_GETWD)		= gettext_noop("Can't get working dirctory (Errcode: %d)");
  EE(EE_SETWD)		= gettext_noop("Can't change dir to '%s' (Errcode: %d)");
  EE(EE_LINK_WARNING)	= gettext_noop("Warning: '%s' had %d links");
  EE(EE_OPEN_WARNING)	= gettext_noop("Warning: %d files and %d streams is left open\n");
  EE(EE_DISK_FULL)	= gettext_noop("Disk is full writing '%s'. Waiting for someone to free space...");
  EE(EE_CANT_MKDIR)	= gettext_noop("Can't create directory '%s' (Errcode: %d)");
  EE(EE_UNKNOWN_CHARSET)= gettext_noop("Character set '%s' is not a compiled character set and is not specified in the %s file");
  EE(EE_OUT_OF_FILERESOURCES)= gettext_noop("Out of resources when opening file '%s' (Errcode: %d)");
  EE(EE_CANT_READLINK)= gettext_noop("Can't read value for symlink '%s' (Error %d)");
  EE(EE_CANT_SYMLINK)= gettext_noop("Can't create symlink '%s' pointing at '%s' (Error %d)");
  EE(EE_REALPATH)= gettext_noop("Error on realpath() on '%s' (Error %d)");
  EE(EE_SYNC)=	 gettext_noop("Can't sync file '%s' to disk (Errcode: %d)");
  EE(EE_UNKNOWN_COLLATION)= gettext_noop("Collation '%s' is not a compiled collation and is not specified in the %s file");
  EE(EE_FILENOTFOUND)	= gettext_noop("File '%s' not found (Errcode: %d)");
  EE(EE_FILE_NOT_CLOSED) = gettext_noop("File '%s' (fileno: %d) was not closed");
}
