/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>

#include "plugin/compression/compress.h"
#include "plugin/compression/uncompress.h"
#include "plugin/compression/uncompressed_length.h"
#include <string>


Create_function<Item_func_compress> compressudf("compress");
Create_function<Item_func_uncompress> uncompressudf("uncompress");
Create_function<Item_func_uncompressed_length>
  uncompressed_lengthudf("uncompressed_length");

static int compressionudf_plugin_init(PluginRegistry &registry)
{
  registry.add(&compressudf);
  registry.add(&uncompressudf);
  registry.add(&uncompressed_lengthudf);
  return 0;
}

static int compressionudf_plugin_deinit(PluginRegistry &registry)
{
  registry.remove(&compressudf);
  registry.remove(&uncompressudf);
  registry.remove(&uncompressed_lengthudf);
  return 0;
}

drizzle_declare_plugin(compression)
{
  "compression",
  "1.1",
  "Stewart Smith",
  "UDFs for compression functions",
  PLUGIN_LICENSE_GPL,
  compressionudf_plugin_init, /* Plugin Init */
  compressionudf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
