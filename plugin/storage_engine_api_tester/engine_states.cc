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
void load_engine_state_transitions(state_multimap &states);

void load_engine_state_transitions(state_multimap &states)
{
  states.insert(state_pair("INIT", "::SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::~SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::max_supported_key_length()"));
  states.insert(state_pair("::max_supported_key_length()", "::max_supported_keys()"));
  states.insert(state_pair("::max_supported_key_length()", "::max_supported_key_parts()"));
  states.insert(state_pair("::max_supported_keys()", "::doCreateTable()"));

  states.insert(state_pair("::max_supported_keys()", "::max_supported_key_part_length()"));

  // what the
  states.insert(state_pair("::max_supported_keys()", "END STATEMENT"));

  states.insert(state_pair("::max_supported_key_part_length()", "::max_supported_key_part_length()"));
  states.insert(state_pair("::max_supported_key_part_length()", "::doCreateTable()"));

  states.insert(state_pair("::max_supported_key_parts()", "::max_supported_key_parts()"));
  states.insert(state_pair("::max_supported_key_parts()", "::max_supported_keys()"));

  // what the
  states.insert(state_pair("START STATEMENT", "::max_supported_key_length()"));

  states.insert(state_pair("::doCreateTable()", "::SEAPITester()"));
/*  states.insert(state_pair("::SEAPITester()", "::create()"));
    states.insert(state_pair("::create()", "::SEAPITester()"));*/
  states.insert(state_pair("::SEAPITester()", "BEGIN"));
  states.insert(state_pair("BEGIN", "In Transaction"));
  states.insert(state_pair("In Transaction", "START STATEMENT"));

  /* really a bug */
  states.insert(state_pair("In Transaction", "END STATEMENT"));
  /* also a bug */
  states.insert(state_pair("::SEAPITester()", "COMMIT"));

  states.insert(state_pair("In Transaction", "COMMIT"));
  states.insert(state_pair("In Transaction", "ROLLBACK"));
  states.insert(state_pair("START STATEMENT", "END STATEMENT"));
  states.insert(state_pair("START STATEMENT", "ROLLBACK STATEMENT"));
  states.insert(state_pair("ROLLBACK STATEMENT", "In Transaction"));
  states.insert(state_pair("END STATEMENT", "START STATEMENT"));
  states.insert(state_pair("END STATEMENT", "COMMIT STATEMENT"));
  states.insert(state_pair("COMMIT STATEMENT", "In Transaction"));
  states.insert(state_pair("END STATEMENT", "COMMIT"));
  states.insert(state_pair("END STATEMENT", "ROLLBACK"));
  states.insert(state_pair("END STATEMENT", "ROLLBACK STATEMENT"));

  states.insert(state_pair("In Transaction", "SET SAVEPOINT"));
  states.insert(state_pair("In Transaction", "ROLLBACK TO SAVEPOINT"));
  states.insert(state_pair("In Transaction", "RELEASE SAVEPOINT"));
  states.insert(state_pair("SET SAVEPOINT", "In Transaction"));
  states.insert(state_pair("ROLLBACK TO SAVEPOINT", "BEGIN"));
  states.insert(state_pair("RELEASE SAVEPOINT", "BEGIN"));

  states.insert(state_pair("COMMIT", "::SEAPITester()"));
  states.insert(state_pair("ROLLBACK", "::SEAPITester()"));
  states.insert(state_pair("::SEAPITester()", "::doGetTableDefinition()"));
  states.insert(state_pair("::doGetTableDefinition()", "::SEAPITester()"));

  /* below just for autocommit statement. doesn't seem right to me */
  states.insert(state_pair("::SEAPITester()", "START STATEMENT"));
}
