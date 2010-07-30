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

#ifndef PLUGIN_FILESYSTEM_ENGINE_FORMATINFO_H
#define PLUGIN_FILESYSTEM_ENGINE_FORMATINFO_H

#include <drizzled/message/table.pb.h>

class FormatInfo
{
public:
  FormatInfo();
  void parseFromTable(drizzled::message::Table *proto);
  bool isFileGiven() const;
  bool isRowSeparator(char ch) const;
  bool isColSeparator(char ch) const;
  bool isEscapedChar(char ch) const;
  std::string getRowSeparatorHead() const;
  std::string getColSeparatorHead() const;
  std::string getColSeparator() const;
  std::string getFileName() const;
  bool isSeparatorModeGeneral() const;
  bool isSeparatorModeWeak() const;
  bool isTagFormat() const;
  static bool validateOption(const std::string &key, const std::string &state);
  static char getEscapedChar(const char ch);
private:
  std::string real_file_name;
  std::string row_separator;
  std::string col_separator;
  std::string file_format;
  std::string escape;
  int separator_mode;
};

#endif /* PLUGIN_FILESYSTEM_ENGINE_FORMATINFO_H */
