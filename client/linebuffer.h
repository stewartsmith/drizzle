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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef CLIENT_LINEBUFFER_H
#define CLIENT_LINEBUFFER_H

#include <vector>
#include <sstream>
#include <iostream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>

class LineBuffer
{
public:
  LineBuffer(uint32_t max_size,FILE *file);
  void addString(const std::string &argument);
  char *readline();
private:
  FILE *file;
  std::vector<char> line;
  std::iostream *file_stream;
  uint32_t max_size;
};

#endif /* CLIENT_LINEBUFFER_H */
