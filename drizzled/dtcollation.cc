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

#include <drizzled/server_includes.h>
#include <drizzled/dtcollation.h>

#include <drizzled/definitions.h>
#include <mysys/my_sys.h>
#include <mystrings/m_ctype.h>
#include <drizzled/error.h>
#include <drizzled/functions/str/conv_charset.h>



static bool
left_is_superset(const DTCollation &left, const DTCollation &right)
{
  /* Allow convert to Unicode */
  if (left.collation->state & MY_CS_UNICODE &&
      (left.derivation < right.derivation ||
       (left.derivation == right.derivation &&
        !(right.collation->state & MY_CS_UNICODE))))
    return true;
  /* Allow convert from ASCII */
  if (right.repertoire == MY_REPERTOIRE_ASCII &&
      (left.derivation < right.derivation ||
       (left.derivation == right.derivation &&
        !(left.repertoire == MY_REPERTOIRE_ASCII))))
    return true;
  /* Disallow conversion otherwise */
  return false;
}


DTCollation::DTCollation()
{
  collation= &my_charset_bin;
  derivation= DERIVATION_NONE;
  repertoire= MY_REPERTOIRE_UNICODE30;
}


DTCollation::DTCollation(const CHARSET_INFO * const collation_arg,
                         Derivation derivation_arg)
{
  collation= collation_arg;
  derivation= derivation_arg;
  set_repertoire_from_charset(collation_arg);
}
void DTCollation::set_repertoire_from_charset(const CHARSET_INFO * const cs)
{
  repertoire= cs->state & MY_CS_PUREASCII ?
    MY_REPERTOIRE_ASCII : MY_REPERTOIRE_UNICODE30;
}


void DTCollation::set(DTCollation &dt)
{
  collation= dt.collation;
  derivation= dt.derivation;
  repertoire= dt.repertoire;
}


void DTCollation::set(const CHARSET_INFO * const collation_arg,
                      Derivation derivation_arg)
{
  collation= collation_arg;
  derivation= derivation_arg;
  set_repertoire_from_charset(collation_arg);
}


void DTCollation::set(const CHARSET_INFO * const collation_arg,
                      Derivation derivation_arg,
                      uint32_t repertoire_arg)
{
  collation= collation_arg;
  derivation= derivation_arg;
  repertoire= repertoire_arg;
}


void DTCollation::set(const CHARSET_INFO * const collation_arg)
{
  collation= collation_arg;
  set_repertoire_from_charset(collation_arg);
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
             left_is_superset(*this, dt))
    {
      // Do nothing
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             left_is_superset(dt, *this))
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
      set(0, DERIVATION_NONE, 0);
      return 1;
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
        set(0, DERIVATION_NONE, 0);
        return 1;
      }
      if (collation->state & MY_CS_BINSORT)
        return 0;
      if (dt.collation->state & MY_CS_BINSORT)
      {
        set(dt);
        return 0;
      }
      const CHARSET_INFO * const bin= get_charset_by_csname(collation->csname,
                                                            MY_CS_BINSORT,
                                                            MYF(0));
      set(bin, DERIVATION_NONE);
    }
  }
  repertoire|= dt.repertoire;
  return 0;
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
  Item **arg, *safe_args[2];

  memset(safe_args, 0, sizeof(safe_args));

  if (agg_item_collations(coll, fname, args, nargs, flags, item_sep))
    return true;

  /*
    For better error reporting: save the first and the second argument.
    We need this only if the the number of args is 3 or 2:
    - for a longer argument list, "Illegal mix of collations"
      doesn't display each argument's characteristics.
    - if nargs is 1, then this error cannot happen.
  */
  if (nargs >=2 && nargs <= 3)
  {
    safe_args[0]= args[0];
    safe_args[1]= args[item_sep];
  }

  Session *session= current_session;
  bool res= false;
  uint32_t i;

  for (i= 0, arg= args; i < nargs; i++, arg+= item_sep)
  {
    Item* conv;
    uint32_t dummy_offset;
    if (!String::needs_conversion(0, (*arg)->collation.collation,
                                  coll.collation,
                                  &dummy_offset))
      continue;

    if (!(conv= (*arg)->safe_charset_converter(coll.collation)) &&
        ((*arg)->collation.repertoire == MY_REPERTOIRE_ASCII))
      conv= new Item_func_conv_charset(*arg, coll.collation, 1);

    if (!conv)
    {
      if (nargs >=2 && nargs <= 3)
      {
        /* restore the original arguments for better error message */
        args[0]= safe_args[0];
        args[item_sep]= safe_args[1];
      }
      my_coll_agg_error(args, nargs, fname, item_sep);
      res= true;
      break; // we cannot return here, we need to restore "arena".
    }
    if ((*arg)->type() == Item::FIELD_ITEM)
      ((Item_field *)(*arg))->no_const_subst= 1;
    /*
      If in statement prepare, then we create a converter for two
      constant items, do it once and then reuse it.
      If we're in execution of a prepared statement, arena is NULL,
      and the conv was created in runtime memory. This can be
      the case only if the argument is a parameter marker ('?'),
      because for all true constants the charset converter has already
      been created in prepare. In this case register the change for
      rollback.
    */
    session->change_item_tree(arg, conv);
    /*
      We do not check conv->fixed, because Item_func_conv_charset which can
      be return by safe_charset_converter can't be fixed at creation
    */
    conv->fix_fields(session, arg);
  }

  return res;
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

