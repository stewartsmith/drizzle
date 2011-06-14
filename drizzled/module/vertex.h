/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Monty Taylor
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

#include <string>

#define BOOST_NO_HASH 1
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

namespace drizzled
{
  enum vertex_properties_t { vertex_properties };
}

namespace boost
{
  template <> struct property_kind<drizzled::vertex_properties_t>
  {
    typedef vertex_property_tag type;
  };
}

namespace drizzled {
namespace module {

class Vertex
{
  std::string name_;
  Module *module_;
public:
  Vertex() :
    name_(""),
    module_(NULL)
  { }
  explicit Vertex(const std::string& name) :
    name_(name),
    module_(NULL)
  { }
  Vertex(const std::string& name, Module *module) :
    name_(name),
    module_(module)
  { }
  Vertex(const Vertex& old) :
    name_(old.name_),
    module_(old.module_)
  { }
  Vertex& operator=(const Vertex& old)
  {
    name_= old.name_;
    module_= old.module_;
    return *this;
  }
  const std::string &getName() const
  {
    return name_;
  }
  void setModule(Module *module)
  {
    module_= module;
  }
  Module* getModule()
  {
    return module_;
  }
};

typedef std::pair<std::string, std::string> ModuleEdge;
typedef boost::adjacency_list<boost::vecS,
        boost::vecS,
        boost::bidirectionalS, 
        boost::property<boost::vertex_color_t,
        boost::default_color_type,
        boost::property<vertex_properties_t, Vertex> >
          > VertexGraph;
typedef boost::graph_traits<VertexGraph>::vertex_descriptor VertexDesc;
typedef std::vector<VertexDesc> VertexList;

typedef boost::graph_traits<VertexGraph>::vertex_iterator vertex_iter;

} /* namespace module */
} /* namespace vertex */

