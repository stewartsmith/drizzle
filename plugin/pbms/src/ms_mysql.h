/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * MySQL interface.
 *
 */

#ifndef __MS_MYSQL_H__
#define __MS_MYSQL_H__

void		*ms_my_get_thread();
const char	*ms_my_version();
u_long		ms_my_get_server_id();
uint64_t		ms_my_1970_to_mysql_time(time_t t);
const char	*ms_my_get_mysql_home_path();
void		ms_my_set_column_name(const char *table, uint16_t col_index, char *col_name);
bool		ms_is_autocommit();

#endif
