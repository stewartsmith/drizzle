/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

/**
 * @file Defines a Plugin Manifest
 *
 * A module::Manifest is the struct contained in every Plugin Library.
 */

#include <drizzled/module/context.h>
#include <drizzled/module/option_context.h>

namespace drizzled
{

struct drizzle_show_var;
struct drizzle_sys_var;

/* We use the following strings to define licenses for plugins */
enum plugin_license_type {
  PLUGIN_LICENSE_GPL,
  PLUGIN_LICENSE_BSD,
  PLUGIN_LICENSE_LGPL,
  PLUGIN_LICENSE_PROPRIETARY,
  PLUGIN_LICENSE_MAX=PLUGIN_LICENSE_LGPL
};


namespace module
{

typedef int (*initialize_func_t)(::drizzled::module::Context &);
typedef void (*options_func_t)(::drizzled::module::option_context &);

/**
 * Plugin Manfiest
 *
 * One Manifest is required per plugin library which is to be dlopened
 *
 * This is a struct and not a class because it is staticly defined in the
 * plugin objects and needs to be a POD as it can, or else it won't compile.
 */
struct Manifest
{
  uint64_t drizzle_version;  /* Drizzle version the plugin was compiled for  */
  const char *name;          /* plugin name (for SHOW PLUGINS)               */
  const char *version;       /* plugin version (for SHOW PLUGINS)            */
  const char *author;        /* plugin author (for SHOW PLUGINS)             */
  const char *descr;         /* general descriptive text (for SHOW PLUGINS ) */
  plugin_license_type license; /* plugin license (PLUGIN_LICENSE_XXX)          */
  initialize_func_t init;     /* function to invoke when plugin is loaded     */
  const char *depends;
  options_func_t init_options; /* register command line options              */
};

} /* namespace module */
} /* namespace drizzled */

