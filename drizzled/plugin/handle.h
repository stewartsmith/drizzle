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

#ifndef DRIZZLED_PLUGIN_HANDLE_H
#define DRIZZLED_PLUGIN_HANDLE_H

#include <drizzled/lex_string.h>
#include <mysys/my_alloc.h>

class sys_var;

namespace drizzled
{
namespace plugin
{

class Manifest;
class Library;

/* A handle of a plugin */
class Handle
{
public:
  LEX_STRING name;
  Manifest *plugin;
  Library *plugin_dl;
  bool isInited;
  MEM_ROOT mem_root;            /* memory for dynamic plugin structures */
  sys_var *system_vars;         /* server variables for this plugin */
  Handle()
    : name(), plugin(NULL), plugin_dl(NULL), isInited(false), 
      mem_root(), system_vars(NULL) {}
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_HANDLE_H */
