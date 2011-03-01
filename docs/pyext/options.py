#  Copyright (C) 2011 Monty Taylor
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import re
try:
  from sphinx.domains import std
  std.option_desc_re = re.compile(
      r'((?:/|-|--)[-_a-zA-Z0-9\.]+)(\s*.*?)(?=,\s+(?:/|-|--)|$)')
except:
  pass

def setup(app):
  pass
