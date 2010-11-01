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
void load_engine_state_transitions(state_multimap &states);

void load_engine_state_transitions(state_multimap &states)
{
  states.insert(state_pair("INIT", "::SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::~SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::doCreateTable()"));
  states.insert(state_pair("::doCreateTable()", "::SEAPITester()"));
/*  states.insert(state_pair("::SEAPITester()", "::create()"));
    states.insert(state_pair("::create()", "::SEAPITester()"));*/
  states.insert(state_pair("::SEAPITester()", "BEGIN"));
  states.insert(state_pair("BEGIN", "START STATEMENT"));

  /* really a bug */
  states.insert(state_pair("BEGIN", "END STATEMENT"));
  /* also a bug */
  states.insert(state_pair("::SEAPITester()", "COMMIT"));

  states.insert(state_pair("BEGIN", "COMMIT"));
  states.insert(state_pair("BEGIN", "ROLLBACK"));
  states.insert(state_pair("START STATEMENT", "END STATEMENT"));
  states.insert(state_pair("END STATEMENT", "START STATEMENT"));
  states.insert(state_pair("END STATEMENT", "COMMIT"));
  states.insert(state_pair("END STATEMENT", "ROLLBACK"));

  states.insert(state_pair("COMMIT", "::SEAPITester()"));
  states.insert(state_pair("ROLLBACK", "::SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::doGetTableDefinition()"));
  states.insert(state_pair("::doGetTableDefinition()", "::SEAPITester()"));

  /* below just for autocommit statement. doesn't seem right to me */
  states.insert(state_pair("::SEAPITester()", "START STATEMENT"));
}
