/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/

#ifndef i_s_h
#define i_s_h

int i_s_common_deinit(PluginRegistry &registry);

int innodb_locks_init();
int innodb_trx_init();
int innodb_lock_waits_init();
int i_s_cmp_init();
int i_s_cmp_reset_init();
int i_s_cmpmem_init();
int i_s_cmpmem_reset_init();

extern ST_SCHEMA_TABLE *innodb_trx_schema_table;
extern ST_SCHEMA_TABLE *innodb_locks_schema_table;
extern ST_SCHEMA_TABLE *innodb_lock_waits_schema_table;
extern ST_SCHEMA_TABLE *innodb_cmp_schema_table;
extern ST_SCHEMA_TABLE *innodb_cmp_reset_schema_table;
extern ST_SCHEMA_TABLE *innodb_cmpmem_schema_table;
extern ST_SCHEMA_TABLE *innodb_cmpmem_reset_schema_table;

#endif /* i_s_h */
