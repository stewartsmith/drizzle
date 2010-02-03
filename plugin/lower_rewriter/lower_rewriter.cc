
#include "config.h"
#include <drizzled/plugin/registry.h>
#include <drizzled/plugin.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/query_rewrite.h>

#include "lower_rewriter.h"

#include <vector>
#include <string>

using namespace std;
using namespace drizzled;

static bool sysvar_lower_rewriter_enable= false;

bool LowerRewriter::isEnabled() const
{
  return sysvar_lower_rewriter_enable;
}

void LowerRewriter::enable()
{
  sysvar_lower_rewriter_enable= true;
}

void LowerRewriter::disable()
{
  sysvar_lower_rewriter_enable= false;
}

void LowerRewriter::rewrite(std::string &to_rewrite)
{
  /*
   * Convert the query to lower case...
   */
  std::transform(to_rewrite.begin(), to_rewrite.end(),
                 to_rewrite.begin(), ::tolower);
}

static LowerRewriter *lower_rewriter= NULL; /* The singleton rewriter */

static int init(plugin::Registry &registry)
{
  lower_rewriter= new LowerRewriter("lower_rewriter");
  registry.add(lower_rewriter);
  return 0;
}

static int deinit(plugin::Registry &registry)
{
  if (lower_rewriter)
  {
    registry.remove(lower_rewriter);
    delete lower_rewriter;
  }
  return 0;
}

static DRIZZLE_SYSVAR_BOOL(
  enable,
  sysvar_lower_rewriter_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable lower rewriter"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static drizzle_sys_var *lower_rewriter_system_variables[]= 
{
  DRIZZLE_SYSVAR(enable),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "lower_rewriter",
  "0.1",
  "Padraig O'Sullivan, Akiban Technologies Inc.",
  N_("Default Replicator"),
  PLUGIN_LICENSE_BSD,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL, /* status variables */
  lower_rewriter_system_variables, /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
