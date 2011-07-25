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

#include <config.h>
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
  states.insert(state_pair("Cursor()", "Cursor()")); // dummy for constructor
  states.insert(state_pair("Cursor()", "~Cursor()"));
  states.insert(state_pair("Cursor()", "::doOpen()"));
  states.insert(state_pair("::doOpen()", "::store_lock()"));

  // only in alter table
  states.insert(state_pair("::doOpen()", "::external_lock()"));

  states.insert(state_pair("::doOpen()", "::close()"));
  states.insert(state_pair("::close()", "Cursor()"));

  states.insert(state_pair("::reset()", "::doOpen()"));
  states.insert(state_pair("::doEndTableScan()", "::reset()"));
  states.insert(state_pair("locked", "::reset()"));
  states.insert(state_pair("locked", "::scan_time()"));
  states.insert(state_pair("::scan_time()", "locked"));
  states.insert(state_pair("::scan_time()", "::scan_time()"));


  // we can always set a new lock
  states.insert(state_pair("::store_lock()", "::store_lock()"));
  states.insert(state_pair("locked", "::store_lock()"));

  states.insert(state_pair("::store_lock()", "::external_lock()"));
  states.insert(state_pair("::external_lock()", "locked"));
  states.insert(state_pair("locked", "::external_lock()"));

  states.insert(state_pair("locked", "::info()"));
  states.insert(state_pair("::info()", "locked"));

  states.insert(state_pair("locked", "::close()"));
  states.insert(state_pair("locked", "::doStartTableScan()"));
  states.insert(state_pair("::doStartTableScan()", "::rnd_next()"));
  states.insert(state_pair("::doStartTableScan()", "::rnd_pos()"));

  states.insert(state_pair("::rnd_pos()", "::rnd_pos()"));
  states.insert(state_pair("::rnd_pos()", "::doUpdateRecord()"));

  states.insert(state_pair("::rnd_pos()", "::doEndTableScan()"));
  states.insert(state_pair("locked", "::doEndTableScan()"));

  states.insert(state_pair("::rnd_next()", "::doEndTableScan()"));
  states.insert(state_pair("::rnd_next()", "::rnd_next()"));

  states.insert(state_pair("::doEndTableScan()", "::close()"));
  states.insert(state_pair("::doEndTableScan()", "::doStartTableScan()"));

  // below two are bugs - sholud call endtablescan
  states.insert(state_pair("::rnd_next()", "::store_lock()"));
  states.insert(state_pair("::rnd_next()", "::close()"));

  states.insert(state_pair("::rnd_next()", "::position()"));
  states.insert(state_pair("::position()", "::rnd_next()"));
  states.insert(state_pair("::rnd_next()", "::doUpdateRecord()"));

  states.insert(state_pair("::doEndTableScan()", "Cursor()"));
  states.insert(state_pair("::doEndTableScan()", "::store_lock()"));
  states.insert(state_pair("locked", "::doInsertRecord()"));
  states.insert(state_pair("::doInsertRecord()", "::external_lock()"));
  states.insert(state_pair("::doInsertRecord()", "::doInsertRecord()"));
  states.insert(state_pair("::doInsertRecord()", "::reset()"));

  states.insert(state_pair("::doUpdateRecord()", "::doEndTableScan()"));
  states.insert(state_pair("::doUpdateRecord()", "::rnd_next()"));

  states.insert(state_pair("locked", "::doStartIndexScan()"));
  states.insert(state_pair("::doStartIndexScan()", "::doEndIndexScan()"));
  states.insert(state_pair("::doEndIndexScan()", "locked"));

  states.insert(state_pair("::doStartIndexScan()", "::index_first()"));
  states.insert(state_pair("::doStartIndexScan()", "::index_last()"));
  states.insert(state_pair("::doStartIndexScan()", "::index_next()"));
  states.insert(state_pair("::doStartIndexScan()", "::index_prev()"));
  states.insert(state_pair("::index_first()", "::doStartIndexScan()"));
  states.insert(state_pair("::index_last()", "::doStartIndexScan()"));
  states.insert(state_pair("::index_next()", "::doStartIndexScan()"));
  states.insert(state_pair("::index_prev()", "::doStartIndexScan()"));
  states.insert(state_pair("::doStartIndexScan()", "::doStartIndexScan() ERROR"));
  states.insert(state_pair("::doStartIndexScan() ERROR", "locked"));

  states.insert(state_pair("::doStartIndexScan()", "::index_read()"));
  states.insert(state_pair("::doStartIndexScan()", "::index_read_idx_map()"));
  states.insert(state_pair("::index_read()", "::doStartIndexScan()"));
  states.insert(state_pair("::index_read_idx_map()", "::doStartIndexScan()"));

}
