#include "formatinfo.h"

FormatInfo::FormatInfo()
  : row_separator(DEFAULT_ROW_SEPARATOR),
  col_separator(DEFAULT_COL_SEPARATOR),
  separator_mode(FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM)
{
}

void FormatInfo::parseFromTable(drizzled::message::Table *proto)
{
  if (!proto)
    return;

  for (int x= 0; x < proto->engine().options_size(); x++)
  {
    const message::Engine::Option& option= proto->engine().options(x);

    if (boost::iequals(option.name(), FILESYSTEM_OPTION_FILE_PATH))
      real_file_name= option.state();
    else if (boost::iequals(option.name(), FILESYSTEM_OPTION_ROW_SEPARATOR))
      row_separator= option.state();
    else if (boost::iequals(option.name(), FILESYSTEM_OPTION_COL_SEPARATOR))
      col_separator= option.state();
    else if (boost::iequals(option.name(), FILESYSTEM_OPTION_SEPARATOR_MODE))
    {
      if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT))
        separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_STRICT_ENUM;
      else if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL))
        separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_GENERAL_ENUM;
      else if (boost::iequals(option.state(), FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK))
        separator_mode= FILESYSTEM_OPTION_SEPARATOR_MODE_WEAK_ENUM;
    }
  }
}

bool FormatInfo::isFileGiven() const
{
  return !real_file_name.empty();
}

bool FormatInfo::isRowSeparator(char ch) const
{
}

bool isColSeparator(char ch) const
{
}
