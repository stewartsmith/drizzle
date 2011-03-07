/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <config.h>
#include <string.h>

#include <drizzled/my_hash.h>
#include <drizzled/xid.h>
#include <drizzled/charset.h>
#include <drizzled/global_charset_info.h>
#include <drizzled/charset_info.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>

namespace drizzled {

bool XID::eq(XID *xid)
{
  return eq(xid->gtrid_length, xid->bqual_length, xid->data);
}

bool XID::eq(long g, long b, const char *d)
{
  return g == gtrid_length && b == bqual_length && !memcmp(d, data, g+b);
}

void XID::set(XID *xid)
{
  memcpy(this, xid, xid->length());
}

void XID::set(long f, const char *g, long gl, const char *b, long bl)
{
  formatID= f;
  memcpy(data, g, gtrid_length= gl);
  memcpy(data+gl, b, bqual_length= bl);
}

void XID::set(uint64_t xid)
{
  formatID= 1;
  set(DRIZZLE_XID_PREFIX_LEN, 0, DRIZZLE_XID_PREFIX);
  memcpy(data + DRIZZLE_XID_PREFIX_LEN, &server_id, sizeof(server_id));
  my_xid tmp= xid;
  memcpy(data + DRIZZLE_XID_OFFSET, &tmp, sizeof(tmp));
  gtrid_length= DRIZZLE_XID_GTRID_LEN;
}

void XID::set(long g, long b, const char *d)
{
  formatID= 1;
  gtrid_length= g;
  bqual_length= b;
  memcpy(data, d, g+b);
}

bool XID::is_null()
{
  return formatID == -1;
}

void XID::null()
{
  formatID= -1;
}

my_xid XID::quick_get_my_xid()
{
  my_xid tmp;
  memcpy(&tmp, data + DRIZZLE_XID_OFFSET, sizeof(tmp));
  return tmp;
}

my_xid XID::get_my_xid()
{
  return gtrid_length == DRIZZLE_XID_GTRID_LEN && bqual_length == 0 &&
    !memcmp(data+DRIZZLE_XID_PREFIX_LEN, &server_id, sizeof(server_id)) &&
    !memcmp(data, DRIZZLE_XID_PREFIX, DRIZZLE_XID_PREFIX_LEN) ?
    quick_get_my_xid() : 0;
}

uint32_t XID::length() const
{
  return sizeof(formatID)+sizeof(gtrid_length)+sizeof(bqual_length)+
    gtrid_length+bqual_length;
}

const unsigned char *XID::key() const
{
  return (unsigned char *)&gtrid_length;
}

uint32_t XID::key_length() const
{
  return sizeof(gtrid_length)+sizeof(bqual_length)+gtrid_length+bqual_length;
}

/***************************************************************************
  Handling of XA id cacheing
***************************************************************************/
typedef boost::lock_guard<boost::mutex> lock_guard_t;

boost::mutex LOCK_xid_cache;
HASH xid_cache;

static unsigned char *xid_get_hash_key(const unsigned char *ptr, size_t *length, bool)
{
  *length=((XID_STATE*)ptr)->xid.key_length();
  return (unsigned char*)((XID_STATE*)ptr)->xid.key();
}

static void xid_free_hash(void *ptr)
{
  XID_STATE *state= (XID_STATE *)ptr;
  if (state->in_session == false)
    delete state;
}

bool xid_cache_init()
{
  return hash_init(&xid_cache, &my_charset_bin, 100, 0, 0, xid_get_hash_key, xid_free_hash, 0);
}

void xid_cache_free()
{
  if (hash_inited(&xid_cache))
    hash_free(&xid_cache);
}

void xid_cache_insert(XID *xid, enum xa_states xa_state)
{
  lock_guard_t lock(LOCK_xid_cache);
  if (hash_search(&xid_cache, xid->key(), xid->key_length()))
    return;
  XID_STATE* xs= new XID_STATE;
  if (not xs)
    return;
  xs->xa_state=xa_state;
  xs->xid.set(xid);
  xs->in_session=0;
  my_hash_insert(&xid_cache, (unsigned char*)xs);
}

bool xid_cache_insert(XID_STATE *xid_state)
{
  lock_guard_t lock(LOCK_xid_cache);
  return my_hash_insert(&xid_cache, (unsigned char*)xid_state);
}

void xid_cache_delete(XID_STATE *xid_state)
{
  lock_guard_t lock(LOCK_xid_cache);
  hash_delete(&xid_cache, (unsigned char *)xid_state);
}

} /* namespace drizzled */
