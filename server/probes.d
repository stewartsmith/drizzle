/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

provider mysql {
probe external_lock(int);
probe insert_row_start();
probe insert_row_end();
probe filesort_start();
probe filesort_end();
probe delete_start();
probe delete_end();
probe insert_start();
probe insert_end();
probe select_start();
probe select_end();
probe update_start();
probe update_end();
};
