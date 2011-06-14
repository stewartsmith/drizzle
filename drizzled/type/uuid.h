/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2010 Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// I looked at code which also had this Copyright header.

/*
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#pragma once

#include <cstdio>
#include <iostream>

namespace drizzled
{
namespace type
{

class Uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint16_t	clock_seq;
	uint8_t	node[6];

public:

  Uuid() :
    time_low(0),
    time_mid(0),
    time_hi_and_version(0),
    clock_seq(0)
  {
    node[0]= node[1]= node[2]= node[3]= node[4]= node[5]= 0;
  }

  void unpack(const unsigned char *in)
  {
    const uint8_t	*ptr= reinterpret_cast<const uint8_t *>(in);
    uint32_t tmp;

    tmp= *ptr++;
    tmp= (tmp << 8) | *ptr++;
    tmp= (tmp << 8) | *ptr++;
    tmp= (tmp << 8) | *ptr++;
    time_low= tmp;

    tmp= *ptr++;
    tmp= (tmp << 8) | *ptr++;
    time_mid= tmp;

    tmp= *ptr++;
    tmp= (tmp << 8) | *ptr++;
    time_hi_and_version = tmp;

    tmp= *ptr++;
    tmp= (tmp << 8) | *ptr++;
    clock_seq= tmp;

    memcpy(node, ptr, 6);
  }

  void pack(unsigned char *out)
  {
    uint32_t	tmp;

    tmp = time_low;
    out[3] = (unsigned char) tmp;
    tmp >>= 8;
    out[2] = (unsigned char) tmp;
    tmp >>= 8;
    out[1] = (unsigned char) tmp;
    tmp >>= 8;
    out[0] = (unsigned char) tmp;

    tmp = time_mid;
    out[5] = (unsigned char) tmp;
    tmp >>= 8;
    out[4] = (unsigned char) tmp;

    tmp = time_hi_and_version;
    out[7] = (unsigned char) tmp;
    tmp >>= 8;
    out[6] = (unsigned char) tmp;

    tmp = clock_seq;
    out[9] = (unsigned char) tmp;
    tmp >>= 8;
    out[8] = (unsigned char) tmp;

    memcpy(out+10, node, 6);
  }

  bool parse(const char *in)
  {
    const char	*cp;
    char buf[3];
    size_t i;

    for (i= 0, cp= in; i < DISPLAY_LENGTH; i++, cp++)
    {
      if ((i == 8) || (i == 13) || (i == 18) || (i == 23))
      {
        if (*cp == '-')
        {
          continue;
        }
        else
        {
          return true;
        }
      }

      if (not isxdigit(*cp))
        return true;
    }

    time_low= strtoul(in, NULL, 16);
    time_mid= strtoul(in+9, NULL, 16);
    time_hi_and_version= strtoul(in+14, NULL, 16);
    clock_seq= strtoul(in+19, NULL, 16);
    cp= in+24;
    buf[2]= 0;

    for (i= 0; i < 6; i++)
    {
      buf[0]= *cp++;
      buf[1]= *cp++;
      node[i]= strtoul(buf, NULL, 16);
    }

    return false;
  }

  void unparse(char *out)
  {
    snprintf(out, DISPLAY_BUFFER_LENGTH, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            time_low,
            time_mid,
            time_hi_and_version,
            clock_seq >> 8,
            clock_seq & 0xFF,
            node[0],
            node[1],
            node[2],
            node[3],
            node[4],
            node[5]);
  }

  void time(struct timeval& ret_val)
  {
    uint32_t high;
    uint64_t clock_reg;

    high= time_mid | ((time_hi_and_version & 0xFFF) << 16);
    clock_reg= time_low | ((uint64_t) high << 32);

    clock_reg -= (((uint64_t) 0x01B21DD2) << 32) + 0x13814000;
    ret_val.tv_sec = clock_reg / 10000000;
    ret_val.tv_usec = (clock_reg % 10000000) / 10;
  }
  bool isTimeType()
  {
    return ((time_hi_and_version >> 12) & 0xF) == 1 ? true : false; 
  }

  static const size_t LENGTH= 16;
  static const size_t DISPLAY_LENGTH= 36;
  static const size_t DISPLAY_BUFFER_LENGTH= DISPLAY_LENGTH+1;
};

} /* namespace type */
} /* namespace drizzled */


