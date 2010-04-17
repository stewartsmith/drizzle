/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include "config.h"

#include <dlfcn.h>

#include <cerrno>
#include <string>

#include "drizzled/plugin.h"
#include "drizzled/definitions.h"
#include "drizzled/error.h"
#include "drizzled/errmsg_print.h"
#include "drizzled/plugin/library.h"

using namespace std;

namespace drizzled
{

plugin::Library::Library(const std::string &name_arg,
                         void *handle_arg,
                         const Manifest *manifest_arg)
 : name(name_arg), handle(handle_arg), manifest(manifest_arg)
{}

plugin::Library::~Library()
{
  /**
   * @TODO: This breaks valgrind at the moment. 
  if (handle)
    dlclose(handle);
  */
}

plugin::Library *plugin::Library::loadLibrary(const string &plugin_name)
{
  /*
    Ensure that the dll doesn't have a path.
    This is done to ensure that only approved libraries from the
    plugin directory are used (to make this even remotely secure).
  */
  size_t found= plugin_name.find(FN_LIBCHAR);
  if (found != string::npos)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, "%s",ER(ER_PLUGIN_NO_PATHS));
    return NULL;
  }

  /* Compile dll path */
  string dlpath;
  dlpath.reserve(FN_REFLEN);
  dlpath.append(opt_plugin_dir);
  dlpath.append("/");
  dlpath.append("lib");
  dlpath.append(plugin_name);
  dlpath.append("_plugin");
#if defined(TARGET_OS_OSX)
  dlpath.append(".dylib");
#else
  dlpath.append(".so");
#endif

  /* Open new dll handle */
  void *handle= dlopen(dlpath.c_str(), RTLD_NOW|RTLD_GLOBAL);
  if (handle == NULL)
  {
    const char *errmsg= dlerror();
    uint32_t dlpathlen= dlpath.length();
    if (!dlpath.compare(0, dlpathlen, errmsg))
    { // if errmsg starts from dlpath, trim this prefix.
      errmsg+=dlpathlen;
      if (*errmsg == ':') errmsg++;
      if (*errmsg == ' ') errmsg++;
    }
    errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_CANT_OPEN_LIBRARY),
                  dlpath.c_str(), errno, errmsg);

    // This is, in theory, should cause dlerror() to deallocate the error
    // message. Found this via Google'ing :)
    (void)dlerror();

    return NULL;
  }


  string plugin_decl_sym("_drizzled_");
  plugin_decl_sym.append(plugin_name);
  plugin_decl_sym.append("_plugin_");

  /* Find plugin declarations */
  void *sym= dlsym(handle, plugin_decl_sym.c_str());
  if (sym == NULL)
  {
    const char* errmsg= dlerror();
    errmsg_printf(ERRMSG_LVL_ERROR, errmsg);
    errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_CANT_FIND_DL_ENTRY),
                  plugin_decl_sym.c_str(), dlpath.c_str());
    (void)dlerror();
    dlclose(handle);
    return NULL;
  }

  const Manifest *manifest= static_cast<plugin::Manifest *>(sym); 
  if (manifest->drizzle_version != DRIZZLE_VERSION_ID)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Plugin module %s was compiled for version %" PRIu64 ", "
                    "which does not match the current running version of "
                    "Drizzle: %" PRIu64"."),
                 dlpath.c_str(), manifest->drizzle_version,
                 DRIZZLE_VERSION_ID);
    return NULL;
  }

  return new (nothrow) plugin::Library(plugin_name, handle, manifest);
}

} /* namespace drizzled */
