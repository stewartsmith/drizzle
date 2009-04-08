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
#include <drizzled/sql_udf.h>
#include <drizzled/registry.h>

#include <string>

using namespace std;

static bool udf_startup= false; /* We do not lock because startup is single threaded */
static drizzled::Registry<Function_builder *> udf_registry;

/* This is only called if using_udf_functions != 0 */
Function_builder *find_udf(const char *name, uint32_t length)
{
  return udf_registry.find(name, length);
}

static bool add_udf(Function_builder *udf)
{
  return udf_registry.add(udf);
}

static void remove_udf(Function_builder *udf)
{
  udf_registry.remove(udf);
}

int initialize_udf(st_plugin_int *plugin)
{
  Function_builder *f;

  udf_startup= true;

  if (plugin->plugin->init)
  {
    int r;
    if ((r= plugin->plugin->init((void *)&f)))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Plugin '%s' init function returned error %d."),
                    plugin->name.str, r);
      return r;
    }
  }
  else
    return 1;

  if (add_udf(f))
    return 1;

  plugin->data= f;
  return 0;

}

int finalize_udf(st_plugin_int *plugin)
{
  Function_builder *udf = static_cast<Function_builder *>(plugin->data);

  if (udf != NULL)
  {
    remove_udf(udf);
  
    if (plugin->plugin->deinit)
    {
      if (plugin->plugin->deinit((void *)udf))
      {
        /* TRANSLATORS: The leading word "udf" is the name
           of the plugin api, and so should not be translated. */
        errmsg_printf(ERRMSG_LVL_ERROR, _("udf plugin '%s' deinit() failed"),
  		      plugin->name.str);
      }
    }

  }

  return 0;
}

