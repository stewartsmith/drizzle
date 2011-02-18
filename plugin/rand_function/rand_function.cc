/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Stewart Smith
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
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/math/real.h>
#include <drizzled/session.h>

using namespace std;
using namespace drizzled;

class RandFunction :public Item_real_func
{
  uint64_t seed1;
  uint64_t seed2;
  uint64_t max_value;
  double max_value_dbl;
  void _seed_random_int(uint64_t new_seed1, uint64_t new_seed2);

public:
  RandFunction()        :Item_real_func() {}
  double val_real();
  const char *func_name() const { return "rand"; }
  bool const_item() const { return 0; }
  void update_used_tables();
  bool fix_fields(Session *session, Item **ref);

  bool check_argument_count(int n)
  {
    return (n == 0 || n == 1);
  }

private:
  void seed_random (Item * val);
};

static uint32_t sql_rnd()
{
  return (uint32_t) (rand() * 0xffffffff); /* make all bits random */
}


void RandFunction::seed_random(Item *arg)
{
  /*
    TODO: do not do reinit 'rand' for every execute of PS/SP if
    args[0] is a constant.
  */
  uint64_t tmp= (uint64_t) arg->val_int();
  _seed_random_int(tmp * 0x10001L + 55555555L, tmp * 0x10000001L);
}

void RandFunction::_seed_random_int(uint64_t new_seed1, uint64_t new_seed2)
{
  max_value= 0x3FFFFFFFL;
  max_value_dbl=(double) max_value;
  seed1= new_seed1 % max_value;
  seed2= new_seed2 % max_value;
}

bool RandFunction::fix_fields(Session *session,Item **ref)
{
  if (Item_real_func::fix_fields(session, ref))
    return true;

  used_tables_cache|= RAND_TABLE_BIT;
  if (arg_count)
  {                                     // Only use argument once in query
    /*
      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.
    */
    if (args[0]->const_item())
      seed_random(args[0]);
  }
  else
  {
    uint64_t tmp= sql_rnd();
    _seed_random_int(tmp + (uint64_t) ref, tmp + (uint64_t) session->thread_id);
  }

  return false;
}

void RandFunction::update_used_tables()
{
  Item_real_func::update_used_tables();
  used_tables_cache|= RAND_TABLE_BIT;
}

double RandFunction::val_real()
{
  assert(fixed == 1);
  if (arg_count && !args[0]->const_item())
    seed_random(args[0]);

  seed1= (seed1 * 3 + seed2) % max_value;
  seed2= (seed1 + seed2 + 33) % max_value;
  return (((double) seed1) / max_value_dbl);
}

plugin::Create_function<RandFunction> *rand_function= NULL;

static int initialize(module::Context &registry)
{
  rand_function= new plugin::Create_function<RandFunction>("rand");
  registry.add(rand_function);
  return 0;
}

DRIZZLE_PLUGIN(initialize, NULL, NULL);
