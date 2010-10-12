/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_XID_H
#define DRIZZLED_XID_H

#include <cstring>

namespace drizzled
{

extern uint32_t server_id;

/**
  class XID _may_ be binary compatible with the XID structure as
  in the X/Open CAE Specification, Distributed Transaction Processing:
  The XA Specification, X/Open Company Ltd., 1991.
  http://www.opengroup.org/bookstore/catalog/c193.htm

*/

typedef uint64_t my_xid;

#define DRIZZLE_XIDDATASIZE 128
#define DRIZZLE_XID_PREFIX "MySQLXid"
#define DRIZZLE_XID_PREFIX_LEN 8 // must be a multiple of 8
#define DRIZZLE_XID_OFFSET (DRIZZLE_XID_PREFIX_LEN+sizeof(server_id))
#define DRIZZLE_XID_GTRID_LEN (DRIZZLE_XID_OFFSET+sizeof(my_xid))

class XID {

public:

  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[DRIZZLE_XIDDATASIZE];  // not \0-terminated !

  XID() :
    formatID(-1), /* -1 == null */
    gtrid_length(0),
    bqual_length(0)
  {
    memset(data, 0, DRIZZLE_XIDDATASIZE);
  }
  bool eq(XID *xid);
  bool eq(long g, long b, const char *d);
  void set(XID *xid);
  void set(long f, const char *g, long gl, const char *b, long bl);
  void set(uint64_t xid);
  void set(long g, long b, const char *d);
  bool is_null();
  void null();
  my_xid quick_get_my_xid();
  my_xid get_my_xid();
  uint32_t length();
  unsigned char *key();
  uint32_t key_length();
};

/**
  struct st_drizzle_xid is binary compatible with the XID structure as
  in the X/Open CAE Specification, Distributed Transaction Processing:
  The XA Specification, X/Open Company Ltd., 1991.
  http://www.opengroup.org/bookstore/catalog/c193.htm

*/

class drizzle_xid {
public:
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[DRIZZLE_XIDDATASIZE];  /* Not \0-terminated */

  drizzle_xid() :
    formatID(0),
    gtrid_length(0),
    bqual_length(0)
  {
    memset(data, 0, DRIZZLE_XIDDATASIZE);
  }
};
typedef class drizzle_xid DRIZZLE_XID;

enum xa_states {XA_NOTR=0, XA_ACTIVE, XA_IDLE, XA_PREPARED};
extern const char *xa_state_names[];

/* for recover() plugin::StorageEngine call */
#define MIN_XID_LIST_SIZE  128
#define MAX_XID_LIST_SIZE  (1024*128)

class XID_STATE 
{
public:
  XID_STATE() :
    xid(),
    xa_state(XA_NOTR),
    in_session(false)
  {}
  /* For now, this is only used to catch duplicated external xids */
  XID  xid;                           // transaction identifier
  enum xa_states xa_state;            // used by external XA only
  bool in_session;
};

bool xid_cache_init(void);
void xid_cache_free(void);
XID_STATE *xid_cache_search(XID *xid);
bool xid_cache_insert(XID *xid, enum xa_states xa_state);
bool xid_cache_insert(XID_STATE *xid_state);
void xid_cache_delete(XID_STATE *xid_state);

} /* namespace drizzled */

#endif /* DRIZZLED_XID_H */
