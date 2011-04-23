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
#include <drizzled/dtcollation.h>

#include <drizzled/definitions.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/error.h>
#include <drizzled/function/str/conv_charset.h>
#include <drizzled/session.h>
#include <drizzled/charset.h>

namespace drizzled
{

DTCollation::DTCollation()
{
  collation= &my_charset_bin;
  derivation= DERIVATION_NONE;
}


DTCollation::DTCollation(const charset_info_st * const collation_arg,
                         Derivation derivation_arg)
{
  collation= collation_arg;
  derivation= derivation_arg;
}


void DTCollation::set(DTCollation &dt)
{
  collation= dt.collation;
  derivation= dt.derivation;
}


void DTCollation::set(const charset_info_st * const collation_arg,
                      Derivation derivation_arg)
{
  collation= collation_arg;
  derivation= derivation_arg;
}


void DTCollation::set(const charset_info_st * const collation_arg)
{
  collation= collation_arg;
}


void DTCollation::set(Derivation derivation_arg)
{
  derivation= derivation_arg;
}


bool DTCollation::aggregate(DTCollation &dt, uint32_t flags)
{
  if (!my_charset_same(collation, dt.collation))
  {
    /*
      We do allow to use binary strings (like BLOBS)
      together with character strings.
      Binaries have more precedence than a character
      string of the same derivation.
    */
    if (collation == &my_charset_bin)
    {
      if (derivation <= dt.derivation)
        ; // Do nothing
      else
      {
        set(dt);
      }
    }
    else if (dt.collation == &my_charset_bin)
    {
      if (dt.derivation <= derivation)
      {
        set(dt);
      }
      else
      {
        // Do nothing
      }
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             collation->state & MY_CS_UNICODE &&
             (derivation < dt.derivation ||
              (derivation == dt.derivation &&
               !(dt.collation->state & MY_CS_UNICODE))))
    {
      // Do nothing
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             dt.collation->state & MY_CS_UNICODE &&
             (dt.derivation < derivation ||
              (dt.derivation == derivation &&
               !(collation->state & MY_CS_UNICODE))))
    {
      set(dt);
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             derivation < dt.derivation &&
             dt.derivation >= DERIVATION_SYSCONST)
    {
      // Do nothing;
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             dt.derivation < derivation &&
             derivation >= DERIVATION_SYSCONST)
    {
      set(dt);
    }
    else
    {
      // Cannot apply conversion
      set(0, DERIVATION_NONE);
      return true;
    }
  }
  else if (derivation < dt.derivation)
  {
    // Do nothing
  }
  else if (dt.derivation < derivation)
  {
    set(dt);
  }
  else
  {
    if (collation == dt.collation)
    {
      // Do nothing
    }
    else
    {
      if (derivation == DERIVATION_EXPLICIT)
      {
        set(0, DERIVATION_NONE);
        return true;
      }
      if (collation->state & MY_CS_BINSORT)
        return false;
      if (dt.collation->state & MY_CS_BINSORT)
      {
        set(dt);
        return false;
      }
      const charset_info_st * const bin= get_charset_by_csname(collation->csname, MY_CS_BINSORT);
      set(bin, DERIVATION_NONE);
    }
  }

  return false;
}


bool DTCollation::set(DTCollation &dt1, DTCollation &dt2, uint32_t flags)
{ set(dt1); return aggregate(dt2, flags); }


const char *DTCollation::derivation_name() const
{
  switch(derivation)
  {
  case DERIVATION_IGNORABLE: return "IGNORABLE";
  case DERIVATION_COERCIBLE: return "COERCIBLE";
  case DERIVATION_IMPLICIT:  return "IMPLICIT";
  case DERIVATION_SYSCONST:  return "SYSCONST";
  case DERIVATION_EXPLICIT:  return "EXPLICIT";
  case DERIVATION_NONE:      return "NONE";
  default: return "UNKNOWN";
  }
}


bool agg_item_collations(DTCollation &c, const char *fname,
                         Item **av, uint32_t count,
                         uint32_t flags, int item_sep)
{
  uint32_t i;
  Item **arg;
  c.set(av[0]->collation);
  for (i= 1, arg= &av[item_sep]; i < count; i++, arg++)
  {
    if (c.aggregate((*arg)->collation, flags))
    {
      my_coll_agg_error(av, count, fname, item_sep);
      return true;
    }
  }
  if ((flags & MY_COLL_DISALLOW_NONE) &&
      c.derivation == DERIVATION_NONE)
  {
    my_coll_agg_error(av, count, fname, item_sep);
    return true;
  }
  return false;
}


bool agg_item_collations_for_comparison(DTCollation &c, const char *fname,
                                        Item **av, uint32_t count,
                                        uint32_t flags)
{
  return (agg_item_collations(c, fname, av, count,
                              flags | MY_COLL_DISALLOW_NONE, 1));
}


bool agg_item_charsets(DTCollation &coll, const char *fname,
                       Item **args, uint32_t nargs, uint32_t flags,
                       int item_sep)
{
  if (agg_item_collations(coll, fname, args, nargs, flags, item_sep))
    return true;

  return false;
}


void my_coll_agg_error(DTCollation &c1,
                       DTCollation &c2, const char *fname)
{
  my_error(ER_CANT_AGGREGATE_2COLLATIONS,MYF(0),
           c1.collation->name,c1.derivation_name(),
           c2.collation->name,c2.derivation_name(),
           fname);
}


void my_coll_agg_error(DTCollation &c1,
                       DTCollation &c2,
                       DTCollation &c3,
                       const char *fname)
{
  my_error(ER_CANT_AGGREGATE_3COLLATIONS,MYF(0),
           c1.collation->name,c1.derivation_name(),
           c2.collation->name,c2.derivation_name(),
           c3.collation->name,c3.derivation_name(),
           fname);
}


void my_coll_agg_error(Item** args, uint32_t count, const char *fname,
                       int item_sep)
{
  if (count == 2)
    my_coll_agg_error(args[0]->collation, args[item_sep]->collation, fname);
  else if (count == 3)
    my_coll_agg_error(args[0]->collation, args[item_sep]->collation,
                      args[2*item_sep]->collation, fname);
  else
    my_error(ER_CANT_AGGREGATE_NCOLLATIONS,MYF(0),fname);
}

} /* namespace drizzled */
