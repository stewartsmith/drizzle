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
 * 2007-06-02
 *
 * CORE SYSTEM:
 * Common definitions that may be required be included at the
 * top of every header file.
 *
 */

#pragma once
#ifndef __CSDEFS_H__
#define __CSDEFS_H__

#include <sys/types.h>

// Use standard portable data types
#include <stdint.h>

/* Those compilers that support the function
 * macro (in particular the "pretty" function
 * macro must be defined here.
 */
#ifdef OS_WINDOWS
#define __FUNC__				__FUNCTION__
#elif defined(OS_SOLARIS)
#define __FUNC__				"__func__"
#else
#define __FUNC__				__PRETTY_FUNCTION__
#endif

/*
 * An unsigned integer, 1 byte long:
 */
#ifndef u_char
#define u_char			unsigned char
#endif

/*
 * An usigned integer, 1 byte long:
 */
#define s_char			unsigned char

/* We assumes that off_t is 8 bytes so to ensure this always use  off64_t*/
#define off64_t			uint64_t


/*
 * A signed integer at least 32 bits long.
 * The size used is whatever is most
 * convenient to the machine.
 */
#define s_int int_fast32_t

/* Forward declartion of a thread: */
class CSThread;

// Used to avoid warnings about unused parameters.
#define UNUSED(x) (void)x

#ifdef OS_WINDOWS

#define CS_DEFAULT_EOL		"\r\n"
#define CS_DIR_CHAR			'\\'
#define CS_DIR_DELIM		"\\"
#define IS_DIR_CHAR(ch)		((ch) == CS_DIR_CHAR || (ch) == '/')

#ifndef PATH_MAX
#define PATH_MAX		MAX_PATH
#endif

#ifndef NAME_MAX
#define NAME_MAX		MAX_PATH
#endif

#else

#define CS_DEFAULT_EOL		"\n"
#define CS_DIR_CHAR			'/'
#define CS_DIR_DELIM		"/"
#define IS_DIR_CHAR(ch)		((ch) == CS_DIR_CHAR)

#endif // OS_WINDOWS

#define CS_CALL_STACK_SIZE		100
#define CS_RELEASE_STACK_SIZE	200
#define CS_JUMP_STACK_SIZE		20	// NOTE: If a stack overflow occurs check that there are no returns inside of try_() blocks.

/* C string display width sizes including space for a null terminator and possible sign. */
#define CS_WIDTH_INT_8	5
#define CS_WIDTH_INT_16	7
#define CS_WIDTH_INT_32	12
#define CS_WIDTH_INT_64	22

typedef uint8_t			CSDiskValue1[1];	
typedef uint8_t			CSDiskValue2[2];	
typedef uint8_t			CSDiskValue3[3];	
typedef uint8_t			CSDiskValue4[4];	
typedef uint8_t			CSDiskValue6[6];	
typedef uint8_t			CSDiskValue8[8];	

/*
 * Byte order on the disk is little endian! This is the byte order of the i386.
 * Little endian byte order starts with the least significan byte.
 *
 * The reason for choosing this byte order for the disk is 2-fold:
 * Firstly the i386 is the cheapest and fasted platform today.
 * Secondly the i386, unlike RISK chips (with big endian) can address
 * memory that is not aligned!
 *
 * Since the disk image of PrimeBase XT is not aligned, the second point
 * is significant. A RISK chip needs to access it byte-wise, so we might as
 * well do the byte swapping at the same time.
 *
 * The macros below are of 4 general types:
 *
 * GET/SET - Get and set 1,2,4,8 byte values (short, int, long, etc).
 * Values are swapped only on big endian platforms. This makes these
 * functions very efficient on little-endian platforms.
 *
 * COPY - Transfer data without swapping regardless of platform. This
 * function is a bit more efficient on little-endian platforms
 * because alignment is not an issue.
 *
 * MOVE - Similar to get and set, but the deals with memory instead
 * of values. Since no swapping is done on little-endian platforms
 * this function is identical to COPY on little-endian platforms.
 *
 * SWAP - Transfer and swap data regardless of the platform type.
 * Aligment is not assumed.
 *
 * The DISK component of the macro names indicates that alignment of
 * the value cannot be assumed.
 *
 */
#if BYTE_ORDER == BIG_ENDIAN
/* The native order of the machine is big endian. Since the native disk
 * disk order of XT is little endian, all data to and from disk
 * must be swapped.
 */
#define CS_SET_DISK_1(d, s)		((d)[0] = (uint8_t) (s))

#define CS_SET_DISK_2(d, s)		do { (d)[0] = (uint8_t)  (((uint16_t) (s))        & 0xFF); (d)[1] = (uint8_t) ((((uint16_t) (s)) >> 8 ) & 0xFF); } while (0)

#define CS_SET_DISK_3(d, s)		do { (d)[0] = (uint8_t)  (((uint32_t) (s))        & 0xFF); (d)[1] = (uint8_t) ((((uint32_t) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (uint8_t) ((((uint32_t) (s)) >> 16) & 0xFF); } while (0)

#define CS_SET_DISK_4(d, s)		do { (d)[0] = (uint8_t)  (((uint32_t) (s))        & 0xFF); (d)[1] = (uint8_t) ((((uint32_t) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (uint8_t) ((((uint32_t) (s)) >> 16) & 0xFF); (d)[3] = (uint8_t) ((((uint32_t) (s)) >> 24) & 0xFF); } while (0)

#define CS_SET_DISK_6(d, s)		do { (d)[0] = (uint8_t)  (((uint64_t) (s))        & 0xFF); (d)[1] = (uint8_t) ((((uint64_t) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (uint8_t) ((((uint64_t) (s)) >> 16) & 0xFF); (d)[3] = (uint8_t) ((((uint64_t) (s)) >> 24) & 0xFF); \
									 (d)[4] = (uint8_t) ((((uint64_t) (s)) >> 32) & 0xFF); (d)[5] = (uint8_t) ((((uint64_t) (s)) >> 40) & 0xFF); } while (0)

#define CS_SET_DISK_8(d, s)		do { (d)[0] = (uint8_t)  (((uint64_t) (s))        & 0xFF); (d)[1] = (uint8_t) ((((uint64_t) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (uint8_t) ((((uint64_t) (s)) >> 16) & 0xFF); (d)[3] = (uint8_t) ((((uint64_t) (s)) >> 24) & 0xFF); \
									 (d)[4] = (uint8_t) ((((uint64_t) (s)) >> 32) & 0xFF); (d)[5] = (uint8_t) ((((uint64_t) (s)) >> 40) & 0xFF); \
									 (d)[6] = (uint8_t) ((((uint64_t) (s)) >> 48) & 0xFF); (d)[7] = (uint8_t) ((((uint64_t) (s)) >> 56) & 0xFF); } while (0)

#define CS_GET_DISK_1(s)		((s)[0])

#define CS_GET_DISK_2(s)		((uint16_t) (((uint16_t) (s)[0]) | (((uint16_t) (s)[1]) << 8)))

#define CS_GET_DISK_3(s)		((uint32_t) (((uint32_t) (s)[0]) | (((uint32_t) (s)[1]) << 8) | (((uint32_t) (s)[2]) << 16)))

#define CS_GET_DISK_4(s)		(((uint32_t) (s)[0])        | (((uint32_t) (s)[1]) << 8 ) | \
								(((uint32_t) (s)[2]) << 16) | (((uint32_t) (s)[3]) << 24))

#define CS_GET_DISK_6(s)		(((uint64_t) (s)[0])        | (((uint64_t) (s)[1]) << 8 ) | \
								(((uint64_t) (s)[2]) << 16) | (((uint64_t) (s)[3]) << 24) | \
								(((uint64_t) (s)[4]) << 32) | (((uint64_t) (s)[5]) << 40))

#define CS_GET_DISK_8(s)		(((uint64_t) (s)[0])        | (((uint64_t) (s)[1]) << 8 ) | \
								(((uint64_t) (s)[2]) << 16) | (((uint64_t) (s)[3]) << 24) | \
								(((uint64_t) (s)[4]) << 32) | (((uint64_t) (s)[5]) << 40) | \
								(((uint64_t) (s)[6]) << 48) | (((uint64_t) (s)[7]) << 56))

/* Move will copy memory, and swap the bytes on a big endian machine.
 * On a little endian machine it is the same as COPY.
 */
#define CS_MOVE_DISK_1(d, s)	((d)[0] = (s)[0])
#define CS_MOVE_DISK_2(d, s)	do { (d)[0] = (s)[1]; (d)[1] = (s)[0]; } while (0)
#define CS_MOVE_DISK_3(d, s)	do { (d)[0] = (s)[2]; (d)[1] = (s)[1]; (d)[2] = (s)[0]; } while (0)
#define CS_MOVE_DISK_4(d, s)	do { (d)[0] = (s)[3]; (d)[1] = (s)[2]; (d)[2] = (s)[1]; (d)[3] = (s)[0]; } while (0)
#define CS_MOVE_DISK_8(d, s)	do { (d)[0] = (s)[7]; (d)[1] = (s)[6]; \
									 (d)[2] = (s)[5]; (d)[3] = (s)[4]; \
									 (d)[4] = (s)[3]; (d)[5] = (s)[2]; \
									 (d)[6] = (s)[1]; (d)[7] = (s)[0]; } while (0)

/*
 * Copy just copies the number of bytes assuming the data is not alligned.
 */
#define CS_COPY_DISK_1(d, s)	(d)[0] = s
#define CS_COPY_DISK_2(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; } while (0)
#define CS_COPY_DISK_3(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; (d)[2] = (s)[2]; } while (0)
#define CS_COPY_DISK_4(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; (d)[2] = (s)[2]; (d)[3] = (s)[3]; } while (0)
#define CS_COPY_DISK_6(d, s)	memcpy(&((d)[0]), &((s)[0]), 6)
#define CS_COPY_DISK_8(d, s)	memcpy(&((d)[0]), &((s)[0]), 8)
#define CS_COPY_DISK_10(d, s)	memcpy(&((d)[0]), &((s)[0]), 10)

#define CS_SET_NULL_DISK_1(d)	CS_SET_DISK_1(d, 0)
#define CS_SET_NULL_DISK_2(d)	do { (d)[0] = 0; (d)[1] = 0; } while (0)
#define CS_SET_NULL_DISK_4(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; } while (0)
#define CS_SET_NULL_DISK_6(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; (d)[4] = 0; (d)[5] = 0; } while (0)
#define CS_SET_NULL_DISK_8(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; (d)[4] = 0; (d)[5] = 0; (d)[6] = 0; (d)[7] = 0; } while (0)

#define CS_IS_NULL_DISK_1(d)	(!(CS_GET_DISK_1(d)))
#define CS_IS_NULL_DISK_4(d)	(!(d)[0] && !(d)[1] && !(d)[2] && !(d)[3])
#define CS_IS_NULL_DISK_8(d)	(!(d)[0] && !(d)[1] && !(d)[2] && !(d)[3] && !(d)[4] && !(d)[5] && !(d)[6] && !(7)[3])

#define CS_EQ_DISK_4(d, s)		((d)[0] == (s)[0] && (d)[1] == (s)[1] && (d)[2] == (s)[2] && (d)[3] == (s)[3])
#define CS_EQ_DISK_8(d, s)		((d)[0] == (s)[0] && (d)[1] == (s)[1] && (d)[2] == (s)[2] && (d)[3] == (s)[3] && \
								(d)[4] == (s)[4] && (d)[5] == (s)[5] && (d)[6] == (s)[6] && (d)[7] == (s)[7])

#define CS_IS_FF_DISK_4(d)		((d)[0] == 0xFF && (d)[1] == 0xFF && (d)[2] == 0xFF && (d)[3] == 0xFF)
#else
/*
 * The native order of the machine is little endian. This means the data to
 * and from disk need not be swapped. In addition to this, since
 * the i386 can access non-aligned memory we are not required to
 * handle the data byte-for-byte.
 */
#define CS_SET_DISK_1(d, s)		((d)[0] = (uint8_t) (s))
#define CS_SET_DISK_2(d, s)		(*((uint16_t *) &((d)[0])) = (uint16_t) (s))
#define CS_SET_DISK_3(d, s)		do { (*((uint16_t *) &((d)[0])) = (uint16_t) (s));  *((uint8_t *) &((d)[2])) = (uint8_t) (((uint32_t) (s)) >> 16); } while (0)
#define CS_SET_DISK_4(d, s)		(*((uint32_t *) &((d)[0])) = (uint32_t) (s))
#define CS_SET_DISK_6(d, s)		do { *((uint32_t *) &((d)[0])) = (uint32_t) (s); *((uint16_t *) &((d)[4])) = (uint16_t) (((uint64_t) (s)) >> 32); } while (0)
#define CS_SET_DISK_8(d, s)		(*((uint64_t *) &((d)[0])) = (uint64_t) (s))

#define CS_GET_DISK_1(s)		((s)[0])
#define CS_GET_DISK_2(s)		*((uint16_t *) &((s)[0]))
#define CS_GET_DISK_3(s)		((uint32_t) *((uint16_t *) &((s)[0])) | (((uint32_t) *((uint8_t *) &((s)[2]))) << 16))
#define CS_GET_DISK_4(s)		*((uint32_t *) &((s)[0]))
#define CS_GET_DISK_6(s)		((uint64_t) *((uint32_t *) &((s)[0])) | (((uint64_t) *((uint16_t *) &((s)[4]))) << 32))
#define CS_GET_DISK_8(s)		*((uint64_t *) &((s)[0]))

#define CS_MOVE_DISK_1(d, s)	((d)[0] = (s)[0])
#define CS_MOVE_DISK_2(d, s)	CS_COPY_DISK_2(d, s)
#define CS_MOVE_DISK_3(d, s)	CS_COPY_DISK_3(d, s)
#define CS_MOVE_DISK_4(d, s)	CS_COPY_DISK_4(d, s)
#define CS_MOVE_DISK_8(d, s)	CS_COPY_DISK_8(d, s)

#define CS_COPY_DISK_1(d, s)	(d)[0] = s
#define CS_COPY_DISK_2(d, s)	(*((uint16_t *) &((d)[0])) = (*((uint16_t *) &((s)[0]))))
#define CS_COPY_DISK_3(d, s)	do { *((uint16_t *) &((d)[0])) = *((uint16_t *) &((s)[0])); (d)[2] = (s)[2]; } while (0)
#define CS_COPY_DISK_4(d, s)	(*((uint32_t *) &((d)[0])) = (*((uint32_t *) &((s)[0]))))
#define CS_COPY_DISK_6(d, s)	do { *((uint32_t *) &((d)[0])) = *((uint32_t *) &((s)[0])); *((uint16_t *) &((d)[4])) = *((uint16_t *) &((s)[4])); } while (0)
#define CS_COPY_DISK_8(d, s)	(*((uint64_t *) &(d[0])) = (*((uint64_t *) &((s)[0]))))
#define CS_COPY_DISK_10(d, s)	memcpy(&((d)[0]), &((s)[0]), 10)

#define CS_SET_NULL_DISK_1(d)	CS_SET_DISK_1(d, 0)
#define CS_SET_NULL_DISK_2(d)	CS_SET_DISK_2(d, 0)
#define CS_SET_NULL_DISK_3(d)	CS_SET_DISK_3(d, 0)
#define CS_SET_NULL_DISK_4(d)	CS_SET_DISK_4(d, 0L)
#define CS_SET_NULL_DISK_6(d)	CS_SET_DISK_6(d, 0LL)
#define CS_SET_NULL_DISK_8(d)	CS_SET_DISK_8(d, 0LL)

#define CS_IS_NULL_DISK_1(d)	(!(CS_GET_DISK_1(d)))
#define CS_IS_NULL_DISK_2(d)	(!(CS_GET_DISK_2(d)))
#define CS_IS_NULL_DISK_3(d)	(!(CS_GET_DISK_3(d)))
#define CS_IS_NULL_DISK_4(d)	(!(CS_GET_DISK_4(d)))
#define CS_IS_NULL_DISK_8(d)	(!(CS_GET_DISK_8(d)))

#define CS_EQ_DISK_4(d, s)		(CS_GET_DISK_4(d) == CS_GET_DISK_4(s))
#define CS_EQ_DISK_8(d, s)		(CS_GET_DISK_8(d) == CS_GET_DISK_8(s))

#define CS_IS_FF_DISK_4(d)		(CS_GET_DISK_4(d) == 0xFFFFFFFF)
#endif

#define CS_CMP_DISK_4(a, b)		((int32_t) CS_GET_DISK_4(a) - (int32_t) CS_GET_DISK_4(b))
#define CS_CMP_DISK_8(d, s)		memcmp(&((d)[0]), &((s)[0]), 8)
//#define CS_CMP_DISK_8(d, s)		(CS_CMP_DISK_4((d).h_number_4, (s).h_number_4) == 0 ? CS_CMP_DISK_4((d).h_file_4, (s).h_file_4) : CS_CMP_DISK_4((d).h_number_4, (s).h_number_4))

#define CS_SWAP_DISK_2(d, s)	do { (d)[0] = (s)[1]; (d)[1] = (s)[0]; } while (0)
#define CS_SWAP_DISK_3(d, s)	do { (d)[0] = (s)[2]; (d)[1] = (s)[1]; (d)[2] = (s)[0]; } while (0)
#define CS_SWAP_DISK_4(d, s)	do { (d)[0] = (s)[3]; (d)[1] = (s)[2]; (d)[2] = (s)[1]; (d)[3] = (s)[0]; } while (0)
#define CS_SWAP_DISK_8(d, s)	do { (d)[0] = (s)[7]; (d)[1] = (s)[6]; (d)[2] = (s)[5]; (d)[3] = (s)[4]; \
									 (d)[4] = (s)[3]; (d)[5] = (s)[2]; (d)[6] = (s)[1]; (d)[7] = (s)[0]; } while (0)

typedef union {
	CSDiskValue1 val_1;
	CSDiskValue4 val_4;
} CSIntRec, *CSIntPtr;

typedef union {
		const char *rec_cchars;
		char *rec_chars;
		CSIntPtr int_val;
} CSDiskData;

#define CHECKSUM_VALUE_SIZE			16
typedef struct {
	u_char val[CHECKSUM_VALUE_SIZE];
} Md5Digest;

	
#endif
