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
 * 2007-06-15
 *
 * CORE SYSTEM:
 * Memory allocation
 *
 */

#pragma once
#ifndef __CSMEMORY_H__
#define __CSMEMORY_H__

#include "CSDefs.h"

bool cs_init_memory(void);
void cs_exit_memory(void);

void *cs_malloc(size_t size);
void *cs_calloc(size_t size);
void cs_realloc(void **ptr, size_t size);
void cs_free(void *);

#define cs_memmove(dst, src, size)		memmove(dst, src, size)
#define cs_memcpy(dst, src, size)		memcpy(dst, src, size)
#define cs_memset(b, v, size)			memset(b, v, size)

#ifdef DEBUG
#define cs_malloc(siz)			cs_mm_malloc(CS_CONTEXT, siz)
#define cs_calloc(siz)			cs_mm_calloc(CS_CONTEXT, siz)
#define cs_realloc(ptr, siz)	cs_mm_realloc(CS_CONTEXT, ptr, siz)
#define cs_free(ptr)			cs_mm_free(ptr)
/*
#define cs_memmove(dst, src, size)		cs_mm_memmove(dst, src, size)
#define cs_memcpy(dst, src, size)		cs_mm_memcpy(dst, src, size)
#define cs_memset(b, v, size)			cs_mm_memset(b, v, size)
*/

bool cs_mm_scan_core(void);
void cs_mm_memmove(void *block, void *dest, void *source, size_t size);
void cs_mm_memcpy(void *block, void *dest, void *source, size_t size);
void cs_mm_memset(void *block, void *dest, int value, size_t size);
void *cs_mm_malloc(const char *func, const char *file, int line, size_t size);
void *cs_mm_calloc(const char *func, const char *file, int line, size_t size);
void cs_mm_realloc(const char *func, const char *file, int line, void **ptr, size_t newsize);
void cs_mm_free(void *ptr);
void cs_mm_pfree(void **ptr);
size_t cs_mm_malloc_size(void *ptr);
void cs_mm_print_track(const char *func, const char *file, uint32_t line, void *p, bool inc, uint32_t ref_cnt, int track_me);
void cs_mm_track_memory(const char *func, const char *file, uint32_t line, void *p, bool inc, uint32_t ref_cnt, int track_me);
uint32_t cs_mm_get_check_point();
void cs_mm_assert_check_point(uint32_t check_point);
#endif

#endif

