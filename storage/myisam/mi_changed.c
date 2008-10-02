/* Copyright (C) 2000-2001 MySQL AB

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

/* Check if somebody has changed table since last check. */

#include "myisamdef.h"

       /* Return 0 if table isn't changed */

int mi_is_changed(MI_INFO *info)
{
  int result;
  if (fast_mi_readinfo(info))
    return(-1);
  _mi_writeinfo(info,0);
  result=(int) info->data_changed;
  info->data_changed=0;
  return(result);
}
