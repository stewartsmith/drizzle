/* Copyright (C) 2000-2003 MySQL AB

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

#ifndef DRIZZLE_SERVER_SQL_ALLOC_H
#define DRIZZLE_SERVER_SQL_ALLOC_H

void init_sql_alloc(MEM_ROOT *root, uint block_size, uint pre_alloc_size);
void *sql_alloc(size_t);
void *sql_calloc(size_t);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str, size_t len);
void *sql_memdup(const void * ptr, size_t size);
void sql_element_free(void *ptr);
char *sql_strmake_with_convert(const char *str, size_t arg_length,
			       const CHARSET_INFO * const from_cs,
			       size_t max_res_length,
			       const CHARSET_INFO * const to_cs, size_t *result_length);
void sql_kill(THD *thd, ulong id, bool only_kill_query);
bool net_request_file(NET* net, const char* fname);
char* query_table_status(THD *thd,const char *db,const char *table_name);

#define x_free(A)	{ my_free((uchar*) (A),MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR)); }
#define safeFree(x)	{ if(x) { my_free((uchar*) x,MYF(0)); x = NULL; } }

#endif /* DRIZZLE_SERVER_SQL_ALLOC_H */
