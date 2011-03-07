/*
  *  Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Barry Leslie
 *
 * 2010-06-09
 */

#ifdef DRIZZLED
#include <config.h>
#include <set>
#include <drizzled/common.h>
#include <drizzled/plugin.h>
#include <drizzled/session.h>
using namespace drizzled;
using namespace drizzled::plugin;

#define my_strdup(a,b) strdup(a)

#include "cslib/CSConfig.h"
#else
#include "cslib/CSConfig.h"
#include "mysql_priv.h"
#include <mysql/plugin.h>
#include <my_dir.h>
#endif 

#include <inttypes.h>

#include "cslib/CSDefs.h"
#include "cslib/CSObject.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSThread.h"
#include "cslib/CSStrUtil.h"

#include "defs_ms.h"
#include "mysql_ms.h"
#include "pbmsdaemon_ms.h"


char PBMSDaemon::pbd_home_dir[PATH_MAX];
PBMSDaemon::DaemonState PBMSDaemon::pbd_state = PBMSDaemon::DaemonUnknown;

void PBMSDaemon::setDaemonState(DaemonState state)
{
	pbd_state = state;
	
	if (pbd_state == DaemonStartUp) { // In theory we could allow this to be set with a commandline option.
		cs_make_absolute_path(PATH_MAX, pbd_home_dir, "pbms", ms_my_get_mysql_home_path()); 
	}
}
