/*****************************************************************************

Copyright (C) 1997, 2010, Innobase Oy. All Rights Reserved.
Copyright (C) 2011 Stewart Smith

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

/* this file is the internal functions that are used by xtrabackup.
   They probably shouldn't be called anywhere else.
 */

#pragma once

ulint
recv_find_max_checkpoint(log_group_t**	max_group,
                         ulint*		max_field);

ibool
log_block_checksum_is_ok_or_old_format(const byte* block);

ulint
open_or_create_data_files(ibool* create_new_db,
#ifdef UNIV_LOG_ARCHIVE
	ulint*		min_arch_log_no,/*!< out: min of archived log
					numbers in data files */
	ulint*		max_arch_log_no,/*!< out: max of archived log
					numbers in data files */
#endif /* UNIV_LOG_ARCHIVE */
	ib_uint64_t*	min_flushed_lsn,/*!< out: min of flushed lsn
					values in data files */
	ib_uint64_t*	max_flushed_lsn,/*!< out: max of flushed lsn
					values in data files */
	ulint*		sum_of_new_sizes);/*!< out: sum of sizes of the
					new files added */

ulint
open_or_create_log_file(
/*====================*/
	ibool	create_new_db,		/*!< in: TRUE if we should create a
					new database */
	ibool*	log_file_created,	/*!< out: TRUE if new log file
					created */
	ibool	log_file_has_been_opened,/*!< in: TRUE if a log file has been
					opened before: then it is an error
					to try to create another log file */
	ulint	k,			/*!< in: log group number */
	ulint	i);			/*!< in: log file number in group */

buf_block_t*
btr_root_block_get(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr);	/*!< in: mtr */

buf_block_t*
btr_node_ptr_get_child(
/*===================*/
	const rec_t*	node_ptr,/*!< in: node pointer */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/*!< in: mtr */

ibool
recv_check_cp_is_consistent(
/*========================*/
                            const byte*	buf);	/*!< in: buffer containing checkpoint info */

int
fil_file_readdir_next_file(
/*=======================*/
	ulint*		err,	/*!< out: this is set to DB_ERROR if an error
				was encountered, otherwise not changed */
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info);	/*!< in/out: buffer where the info is returned */
