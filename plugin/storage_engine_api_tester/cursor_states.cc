/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "config.h"
#include <string>
#include <map>
#include <boost/unordered_map.hpp>

using namespace std;

typedef multimap<string, string> state_multimap;
typedef multimap<string, string>::value_type state_pair;
typedef multimap<string, string>::iterator state_multimap_iter;
void load_cursor_state_transitions(state_multimap &states);

void load_cursor_state_transitions(state_multimap &states)
{
  states.insert(state_pair("Cursor()", "::open()"));
  states.insert(state_pair("::open()", "::store_lock()"));
  states.insert(state_pair("::open()", "::close()"));
  states.insert(state_pair("::close()", "Cursor()"));

  // we can always set a new lock
  states.insert(state_pair("::store_lock()", "::store_lock()"));

  states.insert(state_pair("::store_lock()", "::close()"));
  states.insert(state_pair("::store_lock()", "::doStartTableScan()"));
  states.insert(state_pair("::open()", "::doStartTableScan()"));
  states.insert(state_pair("::doStartTableScan()", "::rnd_next()"));
  states.insert(state_pair("::rnd_next()", "::doEndTableScan()"));

  // below two are bugs - sholud call endtablescan
  states.insert(state_pair("::rnd_next()", "::store_lock()"));
  states.insert(state_pair("::rnd_next()", "::close()"));

  states.insert(state_pair("::doEndTableScan()", "Cursor()"));
  states.insert(state_pair("::store_lock()", "::doInsertRecord()"));
  states.insert(state_pair("::doInsertRecord()", "::store_lock()"));
}
