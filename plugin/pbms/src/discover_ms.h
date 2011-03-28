/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase MS
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *  Created by Leslie on 8/27/08.
 *
 */

#pragma once
#ifndef __DISCOVER_MS_H__
#define __DISCOVER_MS_H__
#include "cslib/CSConfig.h"

#define UTF8_CHARSET	my_charset_utf8_general_ci

/*
 * ---------------------------------------------------------------
 * TABLE DISCOVERY HANDLER
 */
#define NOVAL 0

typedef struct dt_field_info
	{
	/** 
	This is used as column name. 
	*/
	const char* field_name;
	/**
	For string-type columns, this is the maximum number of
	characters. For numeric data this can be NULL.
	*/
	uint field_length;

	/**
	For decimal  columns, this is the maximum number of
	digits after the decimal. For other data this can be NULL.
	*/
	char* field_decimal_length;
	/**
	This denotes data type for the column. For the most part, there seems to
	be one entry in the enum for each SQL data type, although there seem to
	be a number of additional entries in the enum.
	*/
#ifdef DRIZZLED
	enum drizzled::enum_field_types field_type;
#else
	enum enum_field_types field_type;
#endif

	/**
	This is the charater set for non numeric data types including blob data.
	*/
#ifdef DRIZZLED
	const drizzled::charset_info_st *field_charset;
#else
	CHARSET_INFO *field_charset;
#endif
	uint field_flags;        // Field atributes(maybe_null, signed, unsigned etc.)
	const char* comment;
} DT_FIELD_INFO;

typedef struct dt_key_info
{
	const char*	key_name;
	uint		key_type; /* PRI_KEY_FLAG, UNIQUE_KEY_FLAG, MULTIPLE_KEY_FLAG */
	const char*	key_columns[8]; // The size of this can be set to what ever you need.
} DT_KEY_INFO;

typedef struct internal_table_info {
	bool			is_pbms;
	const char		*name;
	DT_FIELD_INFO	*info;
	DT_KEY_INFO		*keys;
} INTERRNAL_TABLE_INFO;


#ifndef DRIZZLED
int ms_create_table_frm(handlerton *hton, THD* thd, const char *db, const char *name, DT_FIELD_INFO *info, DT_KEY_INFO *keys, uchar **frmblob, size_t *frmlen);
#endif

#endif

