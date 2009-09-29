/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

#ifndef DRIZZLED_PLUGIN_COMMAND_APPLIER_H
#define DRIZZLED_PLUGIN_COMMAND_APPLIER_H

/**
 * @file Defines the API for a CommandApplier
 *
 * A CommandApplier applies an event it has received from a CommandReplicator
 * (via a replicator's replicate() call, or it has read using a CommandReader's
 * read() call.
 */

namespace drizzled
{

namespace message { class Command; }

namespace plugin
{

/**
 * Base class for appliers of Command messages
 */
class CommandApplier
{
public:
  CommandApplier() {}
  virtual ~CommandApplier() {}
  /**
   * Apply something to a target.
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, appliers which are
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Command message to be replicated
   */
  virtual void apply(const message::Command &to_apply)= 0;
  /** 
   * An applier plugin should override this with its
   * internal method for determining if it is active or not.
   */
  virtual bool isActive() {return false;}

  static void add(CommandApplier *applier);
  static void remove(CommandApplier *applier);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_COMMAND_APPLIER_H */
