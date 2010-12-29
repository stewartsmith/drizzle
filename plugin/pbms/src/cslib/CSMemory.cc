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
 * For debugging memory leaks search for "DEBUG-BREAK-POINT" and watch mm_tracking_id
 */

#include "CSConfig.h"

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

#include "CSException.h"
#include "CSMemory.h"
#include "CSThread.h"
#include "CSStrUtil.h"
#include "CSGlobal.h"

#ifdef DEBUG
#undef cs_malloc
#undef cs_calloc
#undef cs_realloc
#undef cs_free
#undef new
#endif

void *cs_malloc(size_t size)
{
	void *ptr;

	if (!(ptr = malloc(size)))
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	return ptr;
}

void *cs_calloc(size_t size)
{
	void *ptr;

	if (!(ptr = malloc(size)))
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	memset(ptr, 0, size);
	return ptr;
}

void cs_realloc(void **ptr, size_t size)
{
	void *new_ptr;
	
	if (!(new_ptr = realloc(*ptr, size)))
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	*ptr = new_ptr;
}

void cs_free(void *ptr)
{
	free(ptr);
}

/*
 * -----------------------------------------------------------------------
 * DEBUG MEMORY ALLOCATION AND HEAP CHECKING
 */

#ifdef DEBUG

#define RECORD_MM

#define ADD_TOTAL_ALLOCS			4000

#define SHIFT_RIGHT(ptr, n)			memmove(((char *) (ptr)) + sizeof(MissingMemoryRec), (ptr), (long) (n) * sizeof(MissingMemoryRec))
#define SHIFT_LEFT(ptr, n)			memmove((ptr), ((char *) (ptr)) + sizeof(MissingMemoryRec), (long) (n) * sizeof(MissingMemoryRec))

typedef struct MissingMemory {
	void			*ptr;
	uint32_t			id;
	const char		*func_name;
	const char		*file_name;
	uint32_t			line_nr;
} MissingMemoryRec, *MissingMemoryPtr;

static MissingMemoryRec	*mm_addresses = NULL;
static uint32_t			mm_nr_in_use = 0L;
static uint32_t			mm_total_allocated = 0L;
static uint32_t			mm_alloc_count = 0;
static pthread_mutex_t	mm_mutex;

/* Set this variable to the ID of the memory you want
 * to track.
 */
static uint32_t			mm_tracking_id = 0;

static void mm_println(const char *str)
{
	printf("%s\n", str);
}

static long mm_find_pointer(void *ptr)
{
	register long	i, n, guess;

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr == mm_addresses[guess].ptr)
			return(guess);
		if (ptr < mm_addresses[guess].ptr)
			n = guess;
		else
			i = guess + 1;
	}
	return(-1);
}

static long mm_add_pointer(void *ptr)
{
	register int	i, n, guess;

	if (mm_nr_in_use == mm_total_allocated) {
		/* Not enough space, add more: */
		MissingMemoryRec *new_addresses;

		new_addresses = (MissingMemoryRec *) malloc(sizeof(MissingMemoryRec) * (mm_total_allocated + ADD_TOTAL_ALLOCS));
		if (!new_addresses)
			return(-1);

		if (mm_addresses) {
			memcpy(new_addresses, mm_addresses, sizeof(MissingMemoryRec) * mm_total_allocated);
			free(mm_addresses);
		}

		mm_addresses = new_addresses;
		mm_total_allocated += ADD_TOTAL_ALLOCS;
	}

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr < mm_addresses[guess].ptr)
			n = guess;
		else
			i = guess + 1;
	}

	SHIFT_RIGHT(&mm_addresses[i], mm_nr_in_use - i);
	mm_nr_in_use++;
	mm_addresses[i].ptr = ptr;
	return(i);
}

static char *cs_mm_watch_point = 0;

static long mm_remove_pointer(void *ptr)
{
	register int	i, n, guess;

	if (cs_mm_watch_point == ptr)
		printf("Hit watch point\n");

	i = 0;
	n = mm_nr_in_use;
	while (i < n) {
		guess = (i + n - 1) >> 1;
		if (ptr == mm_addresses[guess].ptr)
			goto remove;
		if (ptr < mm_addresses[guess].ptr)
			n = guess;
		else
			i = guess + 1;
	}
	return(-1);

	remove:
	/* Decrease the number of sets, and shift left: */
	mm_nr_in_use--;
	SHIFT_LEFT(&mm_addresses[guess], mm_nr_in_use - guess);	
	return(guess);
}

static void mm_throw_assertion(MissingMemoryPtr mm_ptr, void *p, const char *message)
{
	char str[200];

	if (mm_ptr) {
		snprintf(str, 200, "MM ERROR: %8p (#%"PRId32") %s:%"PRId32" %s",
					   mm_ptr->ptr,
					   mm_ptr->id,
					   cs_last_name_of_path(mm_ptr->file_name),
					   mm_ptr->line_nr,
					   message);
	}
	else
		snprintf(str, 200, "MM ERROR: %8p %s", p, message);
	mm_println(str);
}

static uint32_t mm_add_core_ptr(void *ptr, const char *func, const char *file, int line)
{
	long	mm;
	uint32_t	id;

	mm = mm_add_pointer(ptr);
	if (mm < 0) {
		mm_println("MM ERROR: Cannot allocate table big enough!");
		return 0;
	}

	/* Record the pointer: */
	if (mm_alloc_count == mm_tracking_id) { 
		/* Enable you to set a breakpoint: */
		id = mm_alloc_count++; // DEBUG-BREAK-POINT
	}
	else {
		id = mm_alloc_count++;
	}
	mm_addresses[mm].ptr = ptr;
	mm_addresses[mm].id = id;
	mm_addresses[mm].func_name = func;
	if (file)
		mm_addresses[mm].file_name = file;
	else
		mm_addresses[mm].file_name = "?";
	mm_addresses[mm].line_nr = line;
	return id;
}

static void mm_remove_core_ptr(void *ptr)
{
	long mm;

	mm = mm_remove_pointer(ptr);
	if (mm < 0) {
		mm_println("MM ERROR: Pointer not allocated");
		return;
	}
}

static long mm_find_core_ptr(void *ptr)
{
	long mm;

	mm = mm_find_pointer(ptr);
	if (mm < 0)
		mm_throw_assertion(NULL, ptr, "Pointer not allocated");
	return(mm);
}

static void mm_replace_core_ptr(long i, void *ptr)
{
	MissingMemoryRec	tmp = mm_addresses[i];
	long				mm;

	mm_remove_pointer(mm_addresses[i].ptr);
	mm = mm_add_pointer(ptr);
	if (mm < 0) {
		mm_println("MM ERROR: Cannot allocate table big enough!");
		return;
	}
	mm_addresses[mm] = tmp;
	mm_addresses[mm].ptr = ptr;
}

/*
 * -----------------------------------------------------------------------
 * MISSING MEMORY PUBLIC ROUTINES
 */

#define MEM_DEBUG_HDR_SIZE		offsetof(MemoryDebugRec, data)
#define MEM_TRAILER_SIZE		2
#define MEM_HEADER				0x01010101
#define MEM_FREED				0x03030303
#define MEM_TRAILER_BYTE		0x02
#define MEM_FREED_BYTE			0x03

typedef struct MemoryDebug {
	uint32_t		check;
	uint32_t		md_id;				/* The memory ID! */
	uint32_t		size;
	char		data[200];
} MemoryDebugRec, *MemoryDebugPtr;

static size_t mm_check_and_free(MissingMemoryPtr mm_ptr, void *p, bool freeme)
{
	unsigned char	*ptr = (unsigned char *) p - MEM_DEBUG_HDR_SIZE;
	MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ptr;
	size_t			size = debug_ptr->size;
	long			a_value;  /* Added to simplfy debugging. */

	if (!ASSERT(p)) 
		return(0);
	if (!ASSERT(((size_t) p & 1L) == 0)) 
		return(0);
	a_value = MEM_FREED;
	if (debug_ptr->check == MEM_FREED) { 
		mm_println("MM ERROR: Pointer already freed 'debug_ptr->check != MEM_FREED'");
		return(0);
	}
	a_value = MEM_HEADER;
	if (debug_ptr->check != MEM_HEADER) {
		mm_println("MM ERROR: Header not valid 'debug_ptr->check != MEM_HEADER'");
		return(0);
	}
	a_value = MEM_TRAILER_BYTE;
	if (!(*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE) == MEM_TRAILER_BYTE &&
			*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE + 1L) == MEM_TRAILER_BYTE)) { 
		mm_throw_assertion(mm_ptr, p, "Trailer overwritten");
		return(0);
	}

	if (freeme) {
		debug_ptr->check = MEM_FREED;
		*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE) = MEM_FREED_BYTE;
		*((unsigned char *) ptr + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_FREED_BYTE;

		memset(((unsigned char *) ptr) + MEM_DEBUG_HDR_SIZE, 0xF5, size);
		cs_free(ptr);
	}

	return size;
}

bool cs_mm_scan_core(void)
{
	uint32_t mm;
	bool rtc = true;

	if (!mm_addresses)
		return true;

	pthread_mutex_lock(&mm_mutex);

	for (mm=0; mm<mm_nr_in_use; mm++)	{
		if (!mm_check_and_free(&mm_addresses[mm], mm_addresses[mm].ptr, false))
			rtc = false;
	}
	
	pthread_mutex_unlock(&mm_mutex);
	return rtc;
}

void cs_mm_memmove(void *block, void *dest, void *source, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		pthread_mutex_lock(&mm_mutex);
		mm_find_core_ptr(block);
		pthread_mutex_unlock(&mm_mutex);
#endif
		mm_check_and_free(NULL, block, false);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size) {
			mm_println("MM ERROR: Destination not in block");
			return;
		}
		if ((char *) dest + size > (char *) block + debug_ptr->size) {
			mm_println("MM ERROR: Copy will overwrite memory");
			return;
		}
	}

	memmove(dest, source, size);
}

void cs_mm_memcpy(void *block, void *dest, void *source, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		pthread_mutex_lock(&mm_mutex);
		mm_find_core_ptr(block);
		pthread_mutex_unlock(&mm_mutex);
#endif
		mm_check_and_free(NULL, block, false);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Destination not in block");
		if ((char *) dest + size > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Copy will overwrite memory");
	}

	memcpy(dest, source, size);
}

void cs_mm_memset(void *block, void *dest, int value, size_t size)
{
	if (block) {
		MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ((char *) block - MEM_DEBUG_HDR_SIZE);

#ifdef RECORD_MM
		pthread_mutex_lock(&mm_mutex);
		mm_find_core_ptr(block);
		pthread_mutex_unlock(&mm_mutex);
#endif
		mm_check_and_free(NULL, block, false);

		if (dest < block || (char *) dest > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Destination not in block");
		if ((char *) dest + size > (char *) block + debug_ptr->size)
			mm_throw_assertion(NULL, block, "Copy will overwrite memory");
	}

	memset(dest, value, size);
}

void *cs_mm_malloc(const char *func, const char *file, int line, size_t size)
{
	unsigned char *p = (unsigned char *) cs_malloc(size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);

	if (!p)
		return NULL;

	memset(p, 0x55, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);

	((MemoryDebugPtr) p)->check = MEM_HEADER;
	((MemoryDebugPtr) p)->md_id = 0;
	((MemoryDebugPtr) p)->size = (uint32_t) size;
	*(p + size + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(p + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_TRAILER_BYTE;

#ifdef RECORD_MM
	pthread_mutex_lock(&mm_mutex);
	((MemoryDebugPtr) p)->md_id = mm_add_core_ptr(p + MEM_DEBUG_HDR_SIZE, func, file, line);
	pthread_mutex_unlock(&mm_mutex);
#endif

	return p + MEM_DEBUG_HDR_SIZE;
}

void *cs_mm_calloc(const char *func, const char *file, int line, size_t size)
{
	unsigned char *p = (unsigned char *) cs_calloc(size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);

	if (!p) 
		return NULL;

	((MemoryDebugPtr) p)->check = MEM_HEADER;
	((MemoryDebugPtr) p)->md_id = 0;
	((MemoryDebugPtr) p)->size  = (uint32_t) size;
	*(p + size + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(p + size + MEM_DEBUG_HDR_SIZE + 1L) = MEM_TRAILER_BYTE;

#ifdef RECORD_MM
	pthread_mutex_lock(&mm_mutex);
	((MemoryDebugPtr) p)->md_id = mm_add_core_ptr(p + MEM_DEBUG_HDR_SIZE, func, file, line);
	pthread_mutex_unlock(&mm_mutex);
#endif

	return p + MEM_DEBUG_HDR_SIZE;
}

void cs_mm_realloc(const char *func, const char *file, int line, void **ptr, size_t newsize)
{
	unsigned char	*oldptr = (unsigned char *) *ptr;
	size_t			size;
	long			mm;
	unsigned char	*pnew;

	if (!oldptr) {
		*ptr = cs_mm_malloc(func, file, line, newsize);
		return;
	}

#ifdef RECORD_MM
	// The lock must be held until the realloc has completed otherwise
	// a scan of the memory may report a bad memory header.
	pthread_mutex_lock(&mm_mutex);
	if ((mm = mm_find_core_ptr(oldptr)) < 0) {
		pthread_mutex_unlock(&mm_mutex);
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	// pthread_mutex_unlock(&mm_mutex); It will be unlocked below
#endif

	oldptr = oldptr - MEM_DEBUG_HDR_SIZE;
	size = ((MemoryDebugPtr) oldptr)->size;

	ASSERT(((MemoryDebugPtr) oldptr)->check == MEM_HEADER);
	ASSERT(*((unsigned char *) oldptr + size + MEM_DEBUG_HDR_SIZE) == MEM_TRAILER_BYTE && 
			*((unsigned char *) oldptr + size + MEM_DEBUG_HDR_SIZE + 1L) == MEM_TRAILER_BYTE);

	/* Grow allways moves! */
	pnew = (unsigned char *) cs_malloc(newsize + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);
	if (!pnew) {
#ifdef RECORD_MM
		pthread_mutex_unlock(&mm_mutex); 
#endif
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
		return;
	}

	if (newsize > size) {
		memcpy(((MemoryDebugPtr) pnew)->data, ((MemoryDebugPtr) oldptr)->data, size);
		memset(((MemoryDebugPtr) pnew)->data + size, 0x55, newsize - size);
	}
	else
		memcpy(((MemoryDebugPtr) pnew)->data, ((MemoryDebugPtr) oldptr)->data, newsize);
	memset(oldptr, 0x55, size + MEM_DEBUG_HDR_SIZE + MEM_TRAILER_SIZE);

#ifdef RECORD_MM
	// pthread_mutex_lock(&mm_mutex); It was locked above
	if ((mm = mm_find_core_ptr(oldptr + MEM_DEBUG_HDR_SIZE)) < 0) {
		pthread_mutex_unlock(&mm_mutex);
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
		return;
	}
	mm_replace_core_ptr(mm, pnew + MEM_DEBUG_HDR_SIZE);
	pthread_mutex_unlock(&mm_mutex);
#endif

	cs_free(oldptr);

	((MemoryDebugPtr) pnew)->check = MEM_HEADER;
	((MemoryDebugPtr) pnew)->size = (uint32_t) newsize;
	*(pnew + newsize + MEM_DEBUG_HDR_SIZE) = MEM_TRAILER_BYTE;
	*(pnew + newsize + MEM_DEBUG_HDR_SIZE + 1L)	= MEM_TRAILER_BYTE;

	*ptr = pnew + MEM_DEBUG_HDR_SIZE;
}

void cs_mm_free(void *ptr)
{
	bool my_pointer = false;

#ifdef RECORD_MM
	pthread_mutex_lock(&mm_mutex);
	if (mm_find_pointer(ptr) >= 0) {
		my_pointer = true;
		mm_remove_core_ptr(ptr);
	}
	pthread_mutex_unlock(&mm_mutex);
#endif
	if (my_pointer)
		mm_check_and_free(NULL, ptr, true);
	else
		free(ptr);
}

void cs_mm_pfree(void **ptr)
{
	if (*ptr) {
		void *p = *ptr;

		*ptr = NULL;
		cs_mm_free(p);
	}
}

size_t cs_mm_malloc_size(void *ptr)
{
	size_t size = 0;

#ifdef RECORD_MM
	pthread_mutex_lock(&mm_mutex);
	mm_find_core_ptr(ptr);
	pthread_mutex_unlock(&mm_mutex);
#endif
	size = mm_check_and_free(NULL, ptr, false);
	return size;
}

void cs_mm_print_track(const char *func, const char *file, uint32_t line, void *p, bool inc, uint32_t ref_cnt, int track_me)
{
	unsigned char	*ptr = (unsigned char *) p - MEM_DEBUG_HDR_SIZE;
	MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ptr;
	CSThread		*self = CSThread::getSelf();
	char			buffer[300];
	int				cnt = 0;

	if (!track_me && !mm_tracking_id)
		return;

	if (func) {
		cs_format_context(300, buffer, func, file, line);
		fprintf(stderr, "TRACKING (%"PRIu32"): %s %2"PRIu32" %s", debug_ptr->md_id, inc ? "INC" : "DEC", ref_cnt, buffer);
	}
	else
		fprintf(stderr, "TRACKING (%"PRIu32"): %s %2"PRIu32"", debug_ptr->md_id, inc ? "INC" : "DEC", ref_cnt);

	for (int i = self->callTop-1; i>=0 && cnt < 4; i--) {
		cs_format_context(300, buffer, self->callStack[i].cs_func, self->callStack[i].cs_file, self->callStack[i].cs_line);
		fprintf(stderr," %s", buffer);
		cnt++;
	}
	fprintf(stderr,"\n");
}

void cs_mm_track_memory(const char *func, const char *file, uint32_t line, void *p, bool inc, uint32_t ref_cnt, int track_me)
{
	unsigned char	*ptr = (unsigned char *) p - MEM_DEBUG_HDR_SIZE;
	MemoryDebugPtr	debug_ptr = (MemoryDebugPtr) ptr;

	if (track_me || (mm_tracking_id && debug_ptr->md_id == mm_tracking_id))
		cs_mm_print_track(func, file, line, p, inc, ref_cnt, track_me);
}

#endif

/*
 * -----------------------------------------------------------------------
 * INIT/EXIT MEMORY
 */

bool cs_init_memory(void)
{
#ifdef DEBUG
	pthread_mutex_init(&mm_mutex, NULL);
	mm_addresses = (MissingMemoryRec *) malloc(sizeof(MissingMemoryRec) * ADD_TOTAL_ALLOCS);
	if (!mm_addresses) {
		mm_println("MM ERROR: Insuffient memory to allocate MM table");
		pthread_mutex_destroy(&mm_mutex);
		return false;
	}

	memset(mm_addresses, 0, sizeof(MissingMemoryRec) * ADD_TOTAL_ALLOCS);
	mm_total_allocated = ADD_TOTAL_ALLOCS;
	mm_nr_in_use = 0L;
	mm_alloc_count = 0L;
#endif
	return true;
}

void cs_exit_memory(void)
{
#ifdef DEBUG
	uint32_t mm;

	if (!mm_addresses)
		return;

	pthread_mutex_lock(&mm_mutex);
	for (mm=0; mm<mm_nr_in_use; mm++)
		mm_throw_assertion(&mm_addresses[mm], mm_addresses[mm].ptr, "Not freed");
	free(mm_addresses);
	mm_addresses = NULL;
	mm_nr_in_use = 0L;
	mm_total_allocated = 0L;
	mm_alloc_count = 0L;
	pthread_mutex_unlock(&mm_mutex);

	pthread_mutex_destroy(&mm_mutex);
#endif
}

#ifdef DEBUG
uint32_t cs_mm_get_check_point()
{
	return mm_alloc_count;
}

// Reports any memory allocated after the check_point
// but has not been freed.
void cs_mm_assert_check_point(uint32_t check_point)
{
	uint32_t mm;

	if (!mm_addresses)
		return;

	pthread_mutex_lock(&mm_mutex);
	for (mm=0; mm<mm_nr_in_use; mm++) {
		if (mm_addresses[mm].id >= check_point)
			mm_throw_assertion(&mm_addresses[mm], mm_addresses[mm].ptr, "Not freed");
	}

	pthread_mutex_unlock(&mm_mutex);

}
#endif
