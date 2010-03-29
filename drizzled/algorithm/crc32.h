/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * Copyright (C) 1992-2007 The FreeBSD Project. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * The crc32 functions and data was originally written by Spencer
 * Garrett <srg@quick.com> and was gleaned from the PostgreSQL source
 * tree via the files contrib/ltree/crc32.[ch] and from FreeBSD at
 * src/usr.bin/cksum/crc32.c.
 */

#ifndef DRIZZLED_ALGORITHM_CRC32_H
#define DRIZZLED_ALGORITHM_CRC32_H

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

namespace drizzled
{
namespace algorithm
{

template <class T>
uint32_t crc32(T key, size_t key_length)
{
  uint64_t x;
  uint32_t crc= UINT32_MAX;

  for (x= 0; x < key_length; x++)
    crc= (crc >> 8) ^ crc32tab[(crc ^ static_cast<uint8_t>(key[x])) & 0xff];

  return ~crc;
}

} /* namespace algorithm */
} /* namespace drizzled */

#endif /* DRIZZLED_ALGORITHM_CRC32_H */
