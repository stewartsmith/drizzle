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
#include <iostream>
#include <string>
#include <map>
#include <boost/unordered_map.hpp>

using namespace std;

typedef multimap<string, string> state_multimap;
typedef multimap<string, string>::value_type state_pair;
typedef multimap<string, string>::iterator state_multimap_iter;

void load_cursor_state_transitions(state_multimap &states);

int main(void)
{
  state_multimap states;

  load_cursor_state_transitions(states);

  cout << "digraph CursorStates {" << endl;

  state_multimap_iter it= states.begin();

  while(it != states.end())
  {
    cout << "  \"" << (*it).first << "\" -> \"" << (*it).second << "\"\n";
    it++;
  }

  cout << "}" << endl;
}
