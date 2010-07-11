/*
  Copyright (C) 2010 Zimin

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
#include <drizzled/field.h>
#include <string>
#include <boost/algorithm/string.hpp>

#include "formatinfo.h"

using namespace std;

static const char* FORMAT_INFO_FILE_PATH= "FILE";
static const char* FORMAT_INFO_ROW_SEPARATOR= "ROW_SEPARATOR";
static const char* FORMAT_INFO_COL_SEPARATOR= "COL_SEPARATOR";
//static const char* FORMAT_INFO_FORMAT= "FORMAT";
static const char* FORMAT_INFO_SEPARATOR_MODE= "SEPARATOR_MODE";
static const char* FORMAT_INFO_SEPARATOR_MODE_STRICT= "STRICT";
static const char* FORMAT_INFO_SEPARATOR_MODE_GENERAL= "GENERAL";
static const char* FORMAT_INFO_SEPARATOR_MODE_WEAK= "WEAK";
enum filesystem_option_separator_mode_type
{
  FORMAT_INFO_SEPARATOR_MODE_STRICT_ENUM= 1,
  FORMAT_INFO_SEPARATOR_MODE_GENERAL_ENUM,
  FORMAT_INFO_SEPARATOR_MODE_WEAK_ENUM
};

static const char* DEFAULT_ROW_SEPARATOR= "\n";
static const char* DEFAULT_COL_SEPARATOR= " \t";

FormatInfo::FormatInfo()
  : row_separator(DEFAULT_ROW_SEPARATOR),
  col_separator(DEFAULT_COL_SEPARATOR),
  separator_mode(FORMAT_INFO_SEPARATOR_MODE_GENERAL_ENUM)
{
}

void FormatInfo::parseFromTable(drizzled::message::Table *proto)
{
  if (!proto)
    return;

  for (int x= 0; x < proto->engine().options_size(); x++)
  {
    const drizzled::message::Engine::Option& option= proto->engine().options(x);

    if (boost::iequals(option.name(), FORMAT_INFO_FILE_PATH))
      real_file_name= option.state();
    else if (boost::iequals(option.name(), FORMAT_INFO_ROW_SEPARATOR))
      row_separator= option.state();
    else if (boost::iequals(option.name(), FORMAT_INFO_COL_SEPARATOR))
      col_separator= option.state();
    else if (boost::iequals(option.name(), FORMAT_INFO_SEPARATOR_MODE))
    {
      if (boost::iequals(option.state(), FORMAT_INFO_SEPARATOR_MODE_STRICT))
        separator_mode= FORMAT_INFO_SEPARATOR_MODE_STRICT_ENUM;
      else if (boost::iequals(option.state(), FORMAT_INFO_SEPARATOR_MODE_GENERAL))
        separator_mode= FORMAT_INFO_SEPARATOR_MODE_GENERAL_ENUM;
      else if (boost::iequals(option.state(), FORMAT_INFO_SEPARATOR_MODE_WEAK))
        separator_mode= FORMAT_INFO_SEPARATOR_MODE_WEAK_ENUM;
    }
  }
}

bool FormatInfo::isFileGiven() const
{
  return !real_file_name.empty();
}

string FormatInfo::getFileName() const
{
  return real_file_name;
}

bool FormatInfo::isRowSeparator(char ch) const
{
  return (row_separator.find(ch) != string::npos);
}

bool FormatInfo::isColSeparator(char ch) const
{
  return (col_separator.find(ch) != string::npos);
}

string FormatInfo::getRowSeparatorHead() const
{
  return row_separator.substr(0, 1);
}

string FormatInfo::getColSeparatorHead() const
{
  return col_separator.substr(0, 1);
}

bool FormatInfo::validateOption(const std::string &key, const std::string &state)
{
  if (boost::iequals(key, FORMAT_INFO_FILE_PATH) &&
      ! state.empty())
    return true;
  if ((boost::iequals(key, FORMAT_INFO_ROW_SEPARATOR) ||
       boost::iequals(key, FORMAT_INFO_COL_SEPARATOR)) &&
      ! state.empty())
    return true;
  if (boost::iequals(key, FORMAT_INFO_SEPARATOR_MODE) &&
      (boost::iequals(state, FORMAT_INFO_SEPARATOR_MODE_STRICT) ||
       boost::iequals(state, FORMAT_INFO_SEPARATOR_MODE_GENERAL) ||
       boost::iequals(state, FORMAT_INFO_SEPARATOR_MODE_WEAK)))
    return true;
  return false;
}

bool FormatInfo::isSeparatorModeGeneral() const
{
  return (separator_mode >= FORMAT_INFO_SEPARATOR_MODE_GENERAL_ENUM);
}

bool FormatInfo::isSeparatorModeWeak() const
{
  return (separator_mode >= FORMAT_INFO_SEPARATOR_MODE_WEAK_ENUM);
}
