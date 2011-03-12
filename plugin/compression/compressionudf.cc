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

#include <config.h>
#include <drizzled/plugin/function.h>

#include <plugin/compression/compress.h>
#include <plugin/compression/uncompress.h>
#include <plugin/compression/uncompressed_length.h>

using namespace std;
using namespace drizzled;

plugin::Create_function<Item_func_compress> *compressudf= NULL;
plugin::Create_function<Item_func_uncompress> *uncompressudf= NULL;
plugin::Create_function<Item_func_uncompressed_length>
  *uncompressed_lengthudf= NULL;

static int compressionudf_plugin_init(module::Context &context)
{
  compressudf= new plugin::Create_function<Item_func_compress>("compress");
  uncompressudf=
    new plugin::Create_function<Item_func_uncompress>("uncompress");
  uncompressed_lengthudf=
    new plugin::Create_function<Item_func_uncompressed_length>("uncompressed_length");
  context.add(compressudf);
  context.add(uncompressudf);
  context.add(uncompressed_lengthudf);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "compression",
  "1.1",
  "Stewart Smith",
  "UDFs for compression functions",
  PLUGIN_LICENSE_GPL,
  compressionudf_plugin_init, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
