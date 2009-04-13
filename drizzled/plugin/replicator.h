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

#ifndef DRIZZLED_PLUGIN_REPLICATOR_H
#define DRIZZLED_PLUGIN_REPLICATOR_H

/**
 * @file Defines the API for a Replicator.  
 *
 * All a replicator does is replicate/reproduce
 * events, optionally transforming them before sending them off to an applier.
 *
 * An applier is responsible for applying events, not a replicator...
 */

/* some forward declarations needed */
namespace drizzled
{
  namespace message
  {
    class Command;
  }
  class Applier;
}

namespace drizzled
{
namespace plugin
{

/**
 * Class which replicates Command messages
 */
class Replicator
{
public:
  Replicator() {}
  virtual ~Replicator() {}
  /**
   * Replicate a Command message to an Applier.
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, replicators and appliers 
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Command message to be replicated
   */
  virtual void replicate(Applier *in_applier, drizzled::message::Command *to_replicate)= 0;
  /** 
   * A replicator plugin should override this with its
   * internal method for determining if it is active or not.
   */
  virtual bool isActive() {return false;}
};

} /* end namespace drizzled::plugin */
} /* end namespace drizzled */

#endif /* DRIZZLED_PLUGIN_REPLICATOR_H */
