/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef OS_WINDOWS
#include <fnmatch.h>
#endif

#include "CSDefs.h"
#include "CSStrUtil.h"
#include "CSMemory.h"
#include "CSGlobal.h"

const char *cs_version()
{
	static char version[124];
	
	if (!version[0]) {
		snprintf(version, 124, "%s(Built %s %s)", VERSION, __DATE__, __TIME__);
	}
	
	return version;
}

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

/* This function adds '...' to the end of the string.
 * if it does not fit!
 */
void cs_strcpy_dottt(size_t size, char *d, const char *s, size_t len)
{
	if (len+1 <= size) {
		cs_strcpy(size, d, s, len);
		return;
	}
	if (size < 5) {
		/* Silly, but anyway... */
		cs_strcpy(size, d, "...");
		return;
	}
	memcpy(d, s, size-4);
	memcpy(d+size-4, "...", 3);
	d[size-1] = 0;
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

int cs_path_depth(const char *path)
{
	int count = 0;
	while (*path) {
		if (IS_DIR_CHAR(*path))
			count++;

		path++;
	}
	return count;
}

static const char *find_wildcard(const char *pattern)
{
	bool escaped = false;
	while (*pattern) {
		if ((*pattern == '*' || *pattern == '?' ) && !escaped)
			return pattern;
			
		if (*pattern == '\\')
			escaped = !escaped;
		else
			escaped = false;
			
		pattern++;
	}
	
	return NULL;
}

// Check if the path contains any variable components.
bool cs_fixed_pattern(const char *str)
{
	return (find_wildcard(str) == NULL);
}

#ifdef OS_WINDOWS
/* 
bool cs_match_patern(const char *pattern, const char *str, bool ignore_case)
{
	bool escaped = false;
	
	while (*pattern && *str) {
		if ((*pattern == '*' || *pattern == '?' ) && !escaped) {
			if (*pattern == '?') {
				pattern++;
				str++;	
				continue;			
			}
			
			while (*pattern == '*' || *pattern == '?' ) pattern++; // eat the pattern matching characters.
			
			if (!*pattern) // A * at the end of the pattern matches everything.
				return true;
				
			// This is where it gets complicted.
			
			coming soon!
			
		}
					
		if (*pattern == '\\')
			escaped = !escaped;
		else
			escaped = false;
			
		if (escaped)
			pattern++;
		else {
			if (ignore_case) {
				if (toupper(*pattern) != toupper(*str))
					return false;
			} else if (*pattern != *str)
				return false;
			pattern++;
			str++;				
		}
		
	}
	
	return ((!*pattern) && (!*str));
}
*/
#else
bool cs_match_patern(const char *pattern, const char *str, bool ignore_case)
{
	return (fnmatch(pattern, str, (ignore_case)?FNM_CASEFOLD:0) == 0);
}
#endif

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
char *cs_last_directory_of_path(const char *path)
{
	size_t	length;
	const char	*ptr;

	length = strlen(path);
	if (!length)
		return((char *)path);
	ptr = path + length - 1;
	if (IS_DIR_CHAR(*ptr))
		ptr--;
	while (ptr != path && !IS_DIR_CHAR(*ptr)) ptr--;
	if (IS_DIR_CHAR(*ptr)) ptr++;
	return((char *)ptr);
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

	// Allow for allocation of an oversized buffer.
	size_t str_len = strlen(in_str);
	if (len > str_len)
		len = str_len;
		
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

bool cs_ends_with(const char *cstr, const char *w_cstr)
{
	size_t		len = strlen(cstr);
	size_t		w_len = strlen(w_cstr);
	const char	*ptr = cstr + len - 1;
	const char	*w_ptr = w_cstr + w_len - 1;
	
	if (w_len > len)
		return false;

	if (w_len == 0)
		return false;

	while (w_ptr >= w_cstr) {
		if (*w_ptr != *ptr)
			return false;
		w_ptr--;
		ptr--;
	}

	return true;
}

void cs_replace_string(size_t size, char *into, const char *find_str, const char *str)
{
	char *ptr;

	if ((ptr = strstr(into, find_str))) {
		size_t len = strlen(into);
		size_t len2 = strlen(str);
		size_t len3 = strlen(find_str);
		
		if (len + len2 + len3 >= size)
			len2 = size - len;
		
		memmove(ptr+len2, ptr+len3, len - (ptr + len3 - into));
		memcpy(ptr, str, len2);
		into[len + len2 - len3] = 0;
	}
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

int64_t cs_byte_size_to_int8(const char *ptr, bool *invalid)
{
	char	*end_ptr;
	int64_t	size;

	if (invalid)
		*invalid = false;

	while (*ptr && isspace(*ptr))
		ptr++;

	if (!isdigit(*ptr) && *ptr != '.')
		goto failed;

	size = (int64_t) strtod(ptr, &end_ptr);

	ptr = end_ptr;
	while (*ptr && isspace(*ptr))
		ptr++;
	
	switch (toupper(*ptr)) {
		case 'P':
			size *= (int64_t) 1024;
		case 'T':
			size *= (int64_t) 1024;
		case 'G':
			size *= (int64_t) 1024;
		case 'M':
			size *= (int64_t) 1024;
		case 'K':
			size *= (int64_t) 1024;
			ptr++;
			break;
		case '\0':
			break;
		default:
			goto failed;
	}
	
	if (toupper(*ptr) == 'B')
		ptr++;

	while (*ptr && isspace(*ptr))
		ptr++;

	if (*ptr)
		goto failed;

	return (int64_t) size;

	failed:
	if (invalid)
		*invalid = true;
	return 0;
}


static uint32_t cs_hex_value(char ch)
{
	u_char uch = (u_char) ch;

	if (uch >= '0' && uch <= '9')
		return uch - '0';
	if (uch >= 'A' && uch <= 'F')
		return uch - 'A' + 10; 
	if (uch >= 'a' && uch <= 'f')
		return uch - 'a' + 10;
	return 0;
}

size_t cs_hex_to_bin(size_t size, void *v_bin, size_t len, const char *hex)
{
	size_t	tot_size = size;
	uint32_t	val = 0;
	size_t	shift = 0;
	u_char *bin = (u_char *) v_bin;

	if (len & 1)
		shift = 1;
	for (size_t i=shift; i<len+shift && size > 0; i++) {
		if (i & 1) {
			val = val | cs_hex_value(*hex);
			*bin = val;
			bin++;
			size--;
		}
		else
			val = cs_hex_value(*hex) << 4;
		hex++;
	}
	return tot_size - size;
}

size_t cs_hex_to_bin(size_t size, void *bin, const char *hex)
{
	return cs_hex_to_bin(size, bin, strlen(hex), hex);
}

#define HEX_DIGIT(x)	((x) <= 9 ? '0' + (x) : 'A' + ((x) - 10))

// NOTE: cs_bin_to_hex() Always null terminates the result.
void cs_bin_to_hex(size_t size, char *hex, size_t len, const void *v_bin)
{
	const u_char *bin = (u_char *) v_bin;
	if (size == 0)
		return;
	size--;
	for (size_t i=0; i<len && size > 0; i++) {
		*hex = HEX_DIGIT(*bin >> 4);
		hex++;
		size--;
		if (size == 0)
			break;
		*hex = HEX_DIGIT(*bin & 0x0F);
		hex++;
		size--;
		bin++;
	}
	*hex = 0;
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

/*
 * Return failed if this is not a valid number.
 */
bool cs_str_to_value(const char *ptr, uint32_t *value, uint8_t base)
{
	char *endptr;

	*value = strtoul(ptr, &endptr, base);
	return *endptr ? false : true;
}


