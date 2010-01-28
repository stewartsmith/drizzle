/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/**
 * @file Simple named savepoint class.
 */

#ifndef DRIZZLE_NAMED_SAVEPOINT_H
#define DRIZZLE_NAMED_SAVEPOINT_H

class Ha_trx_info;

namespace drizzled
{

/**
 * This is a class which stores information about
 * a named savepoint in a transaction
 */
class NamedSavepoint
{
public:
  /**
   * Constructor
   */
  NamedSavepoint(const char *in_name, size_t in_name_length) :
    ha_list(NULL),
    name(in_name, in_name_length)
  {}
  ~NamedSavepoint()
  {}
  Ha_trx_info *ha_list;
  const std::string &getName() const
  {
    return name;
  }
  const std::string &getName()
  {
    return name;
  }
  NamedSavepoint(const NamedSavepoint &other)
  {
    name.assign(other.getName());
    ha_list= other.ha_list;
  }
private:
  std::string name;
  NamedSavepoint();
};

} /* namespace drizzled */

#endif /* DRIZZLE_NAMED_SAVEPOINT_H */
