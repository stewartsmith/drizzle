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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-05-21
 *
 * CORE SYSTEM:
 * Simple utility functions.
 *
 */

#include "CSConfig.h"
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "CSDefs.h"
#include "CSStrUtil.h"
#include "CSMemory.h"
#include "CSGlobal.h"

void cs_strcpy(size_t size, char *to, const char *from, size_t len)
{
	if (size > 0) {
		size--;
		if (len > size)
			len = size;
		memcpy(to, from, len);
		to[len] = 0;
	}
}

void cs_strcpy(size_t size, char *to, const char *from)
{
	if (size > 0) {
		size--;
		while (*from && size--)
			*to++ = *from++;
		*to = 0;
	}
}

void cs_strcpy_left(size_t size, char *to, const char *from, char ch)
{
	if (size > 0) {
		size--;
		while (*from && size-- && *from != ch)
			*to++ = *from++;
		*to = 0;
	}
}

void cs_strcpy_right(size_t size, char *to, const char *from, char ch)
{
	if (size > 0) {
		size--;
		while (*from && *from != ch)
			from++;
		if (*from == ch)
			from++;
		while (*from && size-- && *from != ch)
			*to++ = *from++;
		*to = 0;
	}
}

void cs_strcat(size_t size, char *to, const char *from)
{
	while (*to && size--) to++;
	cs_strcpy(size, to, from);
}

void cs_strcat(size_t size, char *to, char ch)
{
	while (*to && size--) to++;
	if (size >= 1) {
		*to = ch;
		*(to+1) = 0;
	}
}

void cs_strcat(char **to, const char *to_cat)
{
	size_t len = strlen(*to) + strlen(to_cat) + 1;
	
	cs_realloc((void **) to, len);
	strcat(*to, to_cat);
}

void cs_strcat(size_t size, char *to, int i)
{
	char buffer[20];
	
	snprintf(buffer, 20, "%d", i);
	cs_strcat(size, to, buffer);
}

void cs_strcat(size_t size, char *to, uint32_t i)
{
	char buffer[20];
	
	snprintf(buffer, 20, "%"PRIu32"", i);
	cs_strcat(size, to, buffer);
}

void cs_strcat(size_t size, char *to, uint64_t i)
{
	char buffer[40];
	
	snprintf(buffer, 40, "%"PRIu64"", i);
	cs_strcat(size, to, buffer);
}

void cs_strcat_left(size_t size, char *to, const char *from, char ch)
{
	while (*to && size--) to++;
	cs_strcpy_left(size, to, from, ch);
}

void cs_strcat_right(size_t size, char *to, const char *from, char ch)
{
	while (*to && size--) to++;
	cs_strcpy_right(size, to, from, ch);
}

void cs_strcat_hex(size_t size, char *to, uint64_t i)
{
	char buffer[80];
	
	snprintf(buffer, 80, "%"PRIx64"", i);
	cs_strcat(size, to, buffer);
}

void cs_format_context(size_t size, char *buffer, const char *func, const char *file, int line)
{
	char *ptr;

	if (func) {
		cs_strcpy(size, buffer, func);
		// If the "pretty" function includes parameters, remove them:
		if ((ptr = strchr(buffer, '(')))
			*ptr = 0;
		cs_strcat(size, buffer, "(");
	}
	else
		*buffer = 0;
	if (file) {
		cs_strcat(size, buffer, cs_last_name_of_path(file));
		if (line) {
			cs_strcat(size, buffer, ":");
			cs_strcat(size, buffer, line);
		}
	}
	if (func)
		cs_strcat(size, buffer, ")");
}

/* This function returns "" if the path ends with a dir char */
char *cs_last_name_of_path(const char *path, int count)
{
	size_t		length;
	const char	*ptr;

	length = strlen(path);
	if (!length)
		return((char *) path);
	ptr = path + length - 1;
	while (ptr != path) {
		if (IS_DIR_CHAR(*ptr)) {
			count--;
			if (!count)
				break;
		}
		ptr--;
	}
	if (IS_DIR_CHAR(*ptr)) ptr++;
	return((char *) ptr);
}

char *cs_last_name_of_path(const char *path)
{
	return cs_last_name_of_path(path, 1);
}

/* This function returns the last name component, even if the path ends with a dir char */
char *cs_last_directory_of_path(char *path)
{
	size_t	length;
	char	*ptr;

	length = strlen(path);
	if (!length)
		return(path);
	ptr = path + length - 1;
	if (IS_DIR_CHAR(*ptr))
		ptr--;
	while (ptr != path && !IS_DIR_CHAR(*ptr)) ptr--;
	if (IS_DIR_CHAR(*ptr)) ptr++;
	return(ptr);
}

const char *cs_find_extension(const char *file_name)
{
	const char	*ptr;

	for (ptr = file_name + strlen(file_name) - 1; ptr >= file_name; ptr--) {
		if (IS_DIR_CHAR(*ptr))
			break;
		if (*ptr == '.')
			return ptr + 1;
	}
	return NULL;
}

void cs_remove_extension(char *file_name)
{
	char *ptr = (char *) cs_find_extension(file_name);

	if (ptr)
		*(ptr - 1) = 0;
}

bool cs_is_extension(const char *file_name, const char *ext)
{
	const char *ptr;

	if ((ptr = cs_find_extension(file_name)))
		return strcmp(ptr, ext) == 0;
	return false;
}

/*
 * Optionally remove a trailing directory delimiter (If the directory name consists of one
 * character, the directory delimiter is not removed).
 */
bool cs_remove_dir_char(char *dir_name)
{
	size_t length;
	
	length = strlen(dir_name);
	if (length > 1) {
		if (IS_DIR_CHAR(dir_name[length - 1])) {
			dir_name[length - 1] = '\0';
			return true;
		}
	}
	return false;
}

void cs_remove_last_name_of_path(char *path)
{
	char *ptr;

	if ((ptr = cs_last_name_of_path(path)))
		*ptr = 0;
}

static void cs_remove_last_directory_of_path(char *path)
{
	char *ptr;

	if ((ptr = cs_last_directory_of_path(path)))
		*ptr = 0;
}

bool cs_add_dir_char(size_t max, char *path)
{
	size_t slen = strlen(path);

	if (slen >= max)
		return false;

	if (slen == 0) {
		/* If no path is given we will be at the current working directory, under UNIX we must
		 * NOT add a directory delimiter character:
		 */
		return false;
	}

	if (!IS_DIR_CHAR(path[slen - 1])) {
		path[slen] = CS_DIR_CHAR;
		path[slen + 1] = '\0';
		return true;
	}
	return false;
}

bool cs_is_absolute(const char *path)
{
	return IS_DIR_CHAR(*path);
}

void cs_add_name_to_path(size_t max, char *path, const char *name)
{
	char *end_ptr = path + max - 1;

	cs_add_dir_char(max, path);
	path = path + strlen(path);

	if (IS_DIR_CHAR(*name))
		name++;
	while (*name && !IS_DIR_CHAR(*name) && path < end_ptr)
		*path++ = *name++;
	*path = 0;
}

const char *cs_next_name_of_path(const char *path)
{
	if (IS_DIR_CHAR(*path))
		path++;
	while (*path && !IS_DIR_CHAR(*path))
		path++;
	if (IS_DIR_CHAR(*path))
		path++;
	return path;
}

static void cs_adjust_absolute_path(size_t max, char *path, const char *rel_path)
{
	while (*rel_path) {
		if (*rel_path == '.') {
			if (*(rel_path + 1) == '.') {
				if (!*(rel_path + 2) || IS_DIR_CHAR(*(rel_path + 2))) {
					/* ..: move up one: */
					cs_remove_last_directory_of_path(path);
					goto loop;
				}
			}
			else {
				if (!*(rel_path + 1) || IS_DIR_CHAR(*(rel_path + 1)))
					/* .: stay here: */
					goto loop;
			}
		}

		/* Change to this directory: */
		cs_add_name_to_path(max, path, rel_path);
		loop:
		rel_path = cs_next_name_of_path(rel_path);
	}
}

void cs_make_absolute_path(size_t max, char *path, const char *rel_path, const char *cwd)
{
	if (cs_is_absolute(rel_path))
		cs_strcpy(max, path, rel_path);
	else {
		/* Path is relative to the current directory */
		cs_strcpy(max, path, cwd);
		cs_adjust_absolute_path(max, path, rel_path);
	}
	cs_remove_dir_char(path);
}

char *cs_strdup(const char *in_str)
{
	char *str;
	
	if (!in_str)
		return NULL;

	str = (char *) cs_malloc(strlen(in_str) + 1);
	strcpy(str, in_str);
	return str;
}

char *cs_strdup(int i)
{
	char buffer[20];
	char *str;

	snprintf(buffer, 20, "%d", i);
	str = (char *) cs_malloc(strlen(buffer) + 1);
	strcpy(str, buffer);
	return str;
}

char *cs_strdup(const char *in_str, size_t len)
{
	char *str;
	
	if (!in_str)
		return NULL;

	str = (char *) cs_malloc(len + 1);
	memcpy(str, in_str, len);
	str[len] = 0;
	return str;
}

bool cs_starts_with(const char *cstr, const char *w_cstr)
{
	while (*cstr && *w_cstr) {
		if (*cstr != *w_cstr)
			return false;
		cstr++;
		w_cstr++;
	}
	return *cstr || !*w_cstr;
}

void cs_replace_string(size_t size, char *into, const char ch, const char *str)
{
	char *ptr;

	if ((ptr = strchr(into, ch))) {
		size_t len = strlen(into);
		size_t len2 = strlen(str);
		
		if ((len + len2) > size)
			len2 = size - len;
		
		memmove(ptr+1, ptr+len2, len - (ptr - into + 1));
		memcpy(ptr, str, len2);
		into[len + len2 - 1] = 0;
	}
}

uint64_t cs_str_to_word8(const char *ptr, bool *overflow)
{
	uint64_t value = 0;

	if (overflow)
		*overflow = false;
	while (*ptr == '0') ptr++;
	if (!*ptr)
		value = (uint64_t) 0;
	else {
		sscanf(ptr, "%"PRIu64"", &value);
		if (!value && overflow)
			*overflow = true;
	}
	return value;
}

int64_t cs_str_to_int8(const char *ptr, bool *overflow)
{
	int64_t value = 0;

	if (overflow)
		*overflow = false;
	while (*ptr == '0') ptr++;
	if (!*ptr)
		value = (int64_t) 0;
	else {
		sscanf(ptr, "%"PRId64"", &value);
		if (!value && overflow)
			*overflow = true;
	}
	return value;
}

int64_t cs_byte_size_to_int8(const char *ptr)
{
	char	number[101], *num_ptr;
	int64_t	size;

	while (*ptr && isspace(*ptr))
		ptr++;

	num_ptr = number;
	while (*ptr && isdigit(*ptr)) {
		if (num_ptr < number+100) {
			*num_ptr = *ptr;
			num_ptr++;
		}
		ptr++;
	}
	*num_ptr = 0;
	size = cs_str_to_int8(number, NULL);

	while (*ptr && isspace(*ptr))
		ptr++;
	
	switch (toupper(*ptr)) {
		case 'G':
			size *= 1024LL * 1024LL * 1024LL;
			break;
		case 'M':
			size *= 1024LL * 1024LL;
			break;
		case 'K':
			size *= 1024LL;
			break;
	}
	
	return size;
}


/*--------------------------------------------------------------------------------------------------*/
size_t cs_hex_to_bin(size_t size, void *bin, size_t len, const char *hex)
{	
	unsigned char *bin_ptr, *hex_ptr, c, val;
	size_t result = 0;

	if (len %2)  /* The hex string must be an even number of bytes. */
		len--;
		
	if (len > (2 *size)) {
		len = 2 * size;
	} 
			
	bin_ptr = (unsigned char *) bin;	
	hex_ptr = (unsigned char *) hex;	

	
	for (; len > 0; len--, hex_ptr++) {
		c = *hex_ptr;
		if ((c >= '0') && (c <= '9')) {
			val = c - '0';
		}
		else {
			c = toupper(c);
			if ((c >= 'A') && (c <= 'F')) {
				val = c - 'A' + 10;
			}
			else
				return(result);
		}
		
		if ( len & 0X01) {
			*bin_ptr += val;
			bin_ptr++;
			result++;
		}
		else {
			*bin_ptr = val << 4;
		}
	}
	
	return(result);
}
	
/*--------------------------------------------------------------------------------------------------*/
size_t cs_bin_to_hex(size_t size, char *hex, size_t len, const void *bin)
{
	static uint16_t hex_table[256], initialized = 0;
	uint16_t *hex_ptr = (uint16_t *)hex;
	unsigned char *bin_ptr = (unsigned char *)bin;
	size_t	result = 0;

	/* init the hex table if required */
	if (!initialized) {
		char buf[20];
		int i;
		for ( i=0; i < 256; i++) {
			snprintf(buf, 20,"%X", i + 256);
			memcpy(&(hex_table[i]), buf +1, 2);
		}
		
		initialized = 1;
	}
	/*----------------------------------*/	

	if (size < len *2) {
		len = size/2;
	}
		
	result = len *2;
		
	hex_ptr += len -1;
	bin_ptr += len -1;
	for (; len != 0; len--, hex_ptr--, bin_ptr--) {
		memcpy(hex_ptr, hex_table + *bin_ptr, 2);
	}
	
	// If there is room null terminate the hex string.
	if (size > result)
		hex[result] = 0;
		
	return(result);
}	

void cs_strToUpper(char *ptr)
{
	while (*ptr) {
		*ptr = toupper(*ptr);
		ptr++;
	}
}

void cs_strToLower(char *ptr)
{
	while (*ptr) {
		*ptr = tolower(*ptr);
		ptr++;
	}
}

