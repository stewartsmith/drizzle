/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_STATEMENT_SHOW_ENGINE_STATUS_H
#define DRIZZLED_STATEMENT_SHOW_ENGINE_STATUS_H

#include <drizzled/statement.h>

class Session;

namespace drizzled
{
namespace statement
{

class ShowEngineStatus : public Statement
{
public:
  ShowEngineStatus(Session *in_session, StorageEngine *show_engine_arg)
    :
      Statement(in_session),
      show_engine(show_engine_arg)
  {}

  bool execute();
  StorageEngine *show_engine;
};

} /* end namespace statement */

} /* end namespace drizzled */

#endif /* DRIZZLED_STATEMENT_SHOW_ENGINE_STATUS_H */
