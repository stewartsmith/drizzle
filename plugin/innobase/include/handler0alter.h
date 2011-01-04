/*****************************************************************************

Copyright (C) 2005, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

namespace drizzled
{
class Table;
}

/**************************************************//**
@file include/handler0alter.h
Smart ALTER TABLE
*******************************************************/

/*************************************************************//**
Copies an InnoDB record to table->getInsertRecord(). */
UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	::drizzled::Table*		table,		/*!< in/out: MySQL table */
	const rec_t*		rec,		/*!< in: record */
	const dict_index_t*	index,		/*!< in: index */
	const ulint*		offsets);	/*!< in: rec_get_offsets(
						rec, index, ...) */

/*************************************************************//**
Resets table->getInsertRecord(). */
UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	::drizzled::Table*		table);		/*!< in/out: MySQL table */
