/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* This implements 'user defined functions' */
#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/registry.h>
#include "drizzled/service/function.h"

using namespace std;
using namespace drizzled;


const plugin::Function *service::Function::get(const char *name, size_t length) const
{
  return udf_registry.find(name, length);
}

void service::Function::add(plugin::Function *udf)
{
  udf_registry.add(udf);
}

void service::Function::remove(const plugin::Function *udf)
{
  udf_registry.remove(udf);
}


