/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Barry Leslie
 *
 * 2009-01-09
 *
 * H&G2JCtL
 *
 * PBMS Meta Data utilities.
 *
 */
#pragma once
#ifndef __METADATA_MS_H__
#define __METADATA_MS_H__

#ifdef DRIZZLED
#include <drizzled/internal/m_string.h>
#include <drizzled/charset.h>
#else
#include "m_ctype.h"
#endif
 
#include "pbmslib.h"

class MetaData
{
private:
	char *data;
	char *eod;
	char *position;
	
public:
	MetaData(): data(NULL), eod(NULL), position(NULL){}
	MetaData(char *meta_data, size_t meta_data_size): data(meta_data), eod(meta_data + meta_data_size), position(meta_data){}
	
	char *getBuffer() { return data;}
		
	void use_data(char *meta_data, size_t meta_data_size) 
	{
		data = meta_data;
		position = data;
		eod = data + meta_data_size;
	}
	
	void reset() 
	{
		position = data;
	}
	
	char *findNext(char **value)
	{
		char *name = position;
		if (position >= eod)
			return NULL;
			
		position += strlen(position) +1;
		if (position >= eod)
			return NULL;
			
		*value = position;
		position += strlen(position) +1;
		
		return name;
	}
	
	char *findName(const char *name)
	{
		char  *metadata = data;
		
		while (metadata < eod && my_strcasecmp(&my_charset_utf8_general_ci, metadata, name)) {
			metadata += strlen(metadata) +1;
			metadata += strlen(metadata) +1;
		}
		
		if (metadata < eod)
			return metadata + strlen(metadata) +1;
			
		return NULL;
	}
	
	char *findNamePosition(const char *name)
	{
		char  *metadata = data;
		
		while (metadata < eod && my_strcasecmp(&my_charset_utf8_general_ci, metadata, name)) {
			metadata += strlen(metadata) +1;
			metadata += strlen(metadata) +1;
		}
		
		if (metadata < eod)
			return metadata;
			
		return NULL;
	}
	
#ifdef HAVE_ALIAS_SUPPORT
	char *findAlias() {return findName(MS_ALIAS_TAG);}
#endif
	
	static uint32_t recSize(const char *rec) 
	{ 
		uint32_t len = strlen(rec) + 1;
		
		rec += len;
		return (len + strlen(rec) + 1);
	}
	
};
#endif //__METADATA_MS_H__
