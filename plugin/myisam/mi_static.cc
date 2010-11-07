/* Copyright (C) 2000-2002, 2004-2005 MySQL AB

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
  Static variables for MyISAM library. All definied here for easy making of
  a shared library
*/

#include "myisam_priv.h"

std::list<MI_INFO *> myisam_open_list;
unsigned char	 myisam_file_magic[]=
{ (unsigned char) 254, (unsigned char) 254,'\007', '\001', };
unsigned char	 myisam_pack_file_magic[]=
{ (unsigned char) 254, (unsigned char) 254,'\010', '\002', };
char * myisam_log_filename=(char*) "myisam.log";
int	myisam_log_file= -1;
uint	myisam_quick_table_bits=9;
uint32_t myisam_block_size= MI_KEY_BLOCK_LENGTH;		/* Best by test */
uint32_t myisam_concurrent_insert= 2;
uint32_t myisam_bulk_insert_tree_size=8192*1024;
uint32_t data_pointer_size= 6;

/*
  read_vec[] is used for converting between P_READ_KEY.. and SEARCH_
  Position is , == , >= , <= , > , <
*/

uint32_t  myisam_read_vec[]=
{
  SEARCH_FIND, SEARCH_FIND | SEARCH_BIGGER, SEARCH_FIND | SEARCH_SMALLER,
  SEARCH_NO_FIND | SEARCH_BIGGER, SEARCH_NO_FIND | SEARCH_SMALLER,
  SEARCH_FIND | SEARCH_PREFIX, SEARCH_LAST, SEARCH_LAST | SEARCH_SMALLER,
  MBR_CONTAIN, MBR_INTERSECT, MBR_WITHIN, MBR_DISJOINT, MBR_EQUAL
};

uint32_t  myisam_readnext_vec[]=
{
  SEARCH_BIGGER, SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_BIGGER, SEARCH_SMALLER,
  SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_SMALLER
};
