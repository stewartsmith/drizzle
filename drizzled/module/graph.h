/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/module/vertex.h>


namespace drizzled
{

namespace module
{

class Graph
{

private:
  VertexGraph graph_;

  Graph(const Graph&);
  Graph& operator=(const Graph&);

public:
  Graph() :
    graph_()
  { }

  VertexGraph& getGraph()
  {
    return graph_;
  }

  Vertex& properties(const VertexDesc& v)
  {
     boost::property_map<VertexGraph, vertex_properties_t>::type param=
       boost::get(vertex_properties, graph_);
     return param[v];
  }
};

} /* namespace module */
} /* namespace drizzled */
