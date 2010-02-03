
#include "config.h"
#include <drizzled/plugin/registry.h>
#include <drizzled/plugin.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/query_rewrite.h>

#include "rewrite_show.h"

#include <vector>
#include <string>

using namespace std;
using namespace drizzled;

static bool sysvar_show_rewriter_enable= true;

bool ShowRewriter::isEnabled() const
{
  return sysvar_show_rewriter_enable;
}

void ShowRewriter::enable()
{
  sysvar_show_rewriter_enable= true;
}

void ShowRewriter::disable()
{
  sysvar_show_rewriter_enable= false;
}

void ShowRewriter::rewrite(std::string &to_rewrite)
{
  /*
   * Extract the first word in the query. This is much cheaper than parsing a queyr every time we
   * enter this plugin and creating a parse tree to navigate through.
   */
  string::size_type pos= to_rewrite.find_first_of(' ', 0);
  string command= to_rewrite.substr(0, pos);

  /*
   * Convert command to upper case so we are case in-sensitive
   */
  std::transform(command.begin(), command.end(),
                 command.begin(), ::toupper);

  /*
   * If this is not a SHOW command, we don't care about it!
   */
  if (command.compare("SHOW") != 0)
  {
    return;
  }

  /* 
   * so it is a SHOW command, now let's figure out what type it is 
   * the type is always the second word.
   */
  pos= to_rewrite.find_first_of(' ', 5);
  string type= to_rewrite.substr(5, pos - 5);
  std::transform(type.begin(), type.end(),
                 type.begin(), ::toupper);

  if (type.compare("DATABASES") == 0)
  {
    to_rewrite.erase(0, pos);
    to_rewrite.insert(0, "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA ");
  }
  else if (type.compare("TABLES") == 0 ||
           type.compare("TABLE_NAMES") == 0)
  {
    to_rewrite.erase(0, pos);
    to_rewrite.insert(0, "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES ");
  }
  else if (type.compare("COLUMNS") == 0)
  {
    to_rewrite.erase(0, pos);
    to_rewrite.insert(0, "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS ");
  }
  else
  {
    return;
  }
}

static ShowRewriter *show_rewriter= NULL; /* The singleton rewriter */

static int init(plugin::Registry &registry)
{
  show_rewriter= new ShowRewriter("show_rewriter");
  registry.add(show_rewriter);
  return 0;
}

static int deinit(plugin::Registry &registry)
{
  if (show_rewriter)
  {
    registry.remove(show_rewriter);
    delete show_rewriter;
  }
  return 0;
}

static DRIZZLE_SYSVAR_BOOL(
  enable,
  sysvar_show_rewriter_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable show rewriter"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static drizzle_sys_var *show_rewriter_system_variables[]= 
{
  DRIZZLE_SYSVAR(enable),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "rewrite_show",
  "0.1",
  "Padraig O'Sullivan, Akiban Technologies Inc.",
  N_("Show Rewriter"),
  PLUGIN_LICENSE_BSD,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL, /* status variables */
  show_rewriter_system_variables, /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
