/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#pragma once

#include <drizzled/error.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <drizzled/plugin/function.h>
#include <drizzled/plugin/table_function.h>
#include <drizzled/identifier/catalog.h>

#include <plugin/catalog/filesystem.h>

#include <plugin/catalog/functions/create.h>
#include <plugin/catalog/functions/drop.h>
#include <plugin/catalog/functions/lock.h>
#include <plugin/catalog/functions/unlock.h>

#include <plugin/catalog/tables/catalog_cache.h>
#include <plugin/catalog/tables/catalogs.h>

