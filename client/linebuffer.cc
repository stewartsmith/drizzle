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

/* readline for batch mode */

#include <config.h>
#include <drizzled/internal/my_sys.h>
#include <client/linebuffer.h>
#include <boost/version.hpp>

#include <vector>

using namespace std;
using namespace drizzled;

LineBuffer::LineBuffer(uint32_t my_max_size,FILE *my_file)
  :
    file(my_file),
    max_size(my_max_size)
{
  if (my_file)

  /*
    if here beacuse the old way of using file_descriptor is deprecated in boost
    1.44.  There is a #define to re-enable the function but this is broken in
    Fedora 14. See https://bugzilla.redhat.com/show_bug.cgi?id=654480
  */
#if BOOST_VERSION < 104400
    file_stream = new boost::iostreams::stream<boost::iostreams::file_descriptor>(fileno(my_file), true);
#else
    file_stream = new boost::iostreams::stream<boost::iostreams::file_descriptor>(fileno(my_file), boost::iostreams::never_close_handle);
#endif
  else
    file_stream = new std::stringstream;
  line.reserve(max_size);
}

void LineBuffer::addString(const string &str)
{
  (*file_stream) << str << endl;
}

char *LineBuffer::readline()
{
  file_stream->getline(&line[0], max_size);

  if (file_stream->fail())
    return 0;
  else
    return &line[0];
}

