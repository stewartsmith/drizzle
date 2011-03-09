/* Copyright (C) 2010 Brian Aker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include <config.h>

#include <plugin/archive/archive_engine.h>

using namespace std;
using namespace drizzled;

static ArchiveEngine *archive_engine= NULL;

static bool archive_use_aio= false;

/* Used by the engie to determine the state of the archive AIO state */
bool archive_aio_state(void);

bool archive_aio_state(void)
{
  return archive_use_aio;
}

static int init(drizzled::module::Context &context)
{

  archive_engine= new ArchiveEngine();
  context.registerVariable(new sys_var_bool_ptr("aio", &archive_use_aio));
  context.add(archive_engine);

  return false;
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "ARCHIVE",
  "3.5",
  "Brian Aker, MySQL AB",
  "Archive storage engine",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  NULL,   /* depends */
  NULL                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;
