/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <drizzled/global.h>
#include "libdrizzle.h"

/* Get the length of next field. Change parameter to point at fieldstart */
uint32_t net_field_length(uchar **packet)
{
  register uchar *pos= (uchar *)*packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (uint32_t) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (uint32_t) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (uint32_t) uint3korr(pos+1);
  }
  (*packet)+=9;          /* Must be 254 when here */
  return (uint32_t) uint4korr(pos+1);
}

/* The same as above but returns int64_t */
uint64_t net_field_length_ll(uchar **packet)
{
  register uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (uint64_t) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return (uint64_t) NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (uint64_t) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (uint64_t) uint3korr(pos+1);
  }
  (*packet)+=9;          /* Must be 254 when here */
#ifdef NO_CLIENT_LONGLONG
  return (uint64_t) uint4korr(pos+1);
#else
  return (uint64_t) uint8korr(pos+1);
#endif
}

/*
  Store an integer with simple packing into a output package

  SYNOPSIS
    net_store_length()
    pkg      Store the packed integer here
    length    integers to store

  NOTES
    This is mostly used to store lengths of strings.
    We have to cast the result for the LL() becasue of a bug in Forte CC
    compiler.

  RETURN
   Position in 'pkg' after the packed length
*/

uchar *net_store_length(uchar *packet, uint64_t length)
{
  if (length < (uint64_t) 251LL)
  {
    *packet=(uchar) length;
    return packet+1;
  }
  /* 251 is reserved for NULL */
  if (length < (uint64_t) 65536LL)
  {
    *packet++=252;
    int2store(packet,(uint) length);
    return packet+2;
  }
  if (length < (uint64_t) 16777216LL)
  {
    *packet++=253;
    int3store(packet,(uint32_t) length);
    return packet+3;
  }
  *packet++=254;
  int8store(packet,length);
  return packet+8;
}

