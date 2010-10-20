/* Copyright (c) 2010 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Barry Leslie
 *
 * 2010-05-31
 *
 * PBMS daemon plugin interface.
 *
 */
#ifdef DRIZZLED
#include "config.h"
#include <drizzled/common.h>
#include <drizzled/plugin.h>
#include <drizzled/session.h>
using namespace drizzled;
using namespace drizzled::plugin;

#include "cslib/CSConfig.h"
#else
#include "cslib/CSConfig.h"
#include "mysql_priv.h"
#include <mysql/plugin.h>
#include <my_dir.h>
#endif 

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>


#include "defs_ms.h"


/////////////////////////
// Plugin Definition:
/////////////////////////
#ifdef DRIZZLED
#include "events_ms.h"
static PBMSEvents *pbms_events= NULL;


extern int pbms_init_func(module::Context &registry);
extern struct drizzled::drizzle_sys_var* pbms_system_variables[];

static int my_init(module::Context &registry)
{
	int rtc;
	
	PBMSParameters::startUp();
	rtc = pbms_init_func(registry);
	if (rtc == 0) {
		pbms_events = new PBMSEvents();
		registry.add(pbms_events);
	}
	
	return rtc;
}

DRIZZLE_DECLARE_PLUGIN
{
	DRIZZLE_VERSION_ID,
	"PBMS",
	"1.0",
	"Barry Leslie, PrimeBase Technologies GmbH",
	"The Media Stream daemon for Drizzle",
	PLUGIN_LICENSE_GPL,
	my_init, /* Plugin Init */
	pbms_system_variables,          /* system variables                */
	NULL                                            /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;

#else

extern int pbms_init_func(void *p);
extern int pbms_done_func(void *);
extern struct st_mysql_sys_var* pbms_system_variables[];

struct st_mysql_storage_engine pbms_engine_handler = {
	MYSQL_HANDLERTON_INTERFACE_VERSION
};

mysql_declare_plugin(pbms)
{
	MYSQL_STORAGE_ENGINE_PLUGIN,
	&pbms_engine_handler,
	"PBMS",
	"Barry Leslie, PrimeBase Technologies GmbH",
	"The Media Stream daemon for MySQL",
	PLUGIN_LICENSE_GPL,
	pbms_init_func, /* Plugin Init */
	pbms_done_func, /* Plugin Deinit */
	0x0001 /* 0.1 */,
	NULL, 											/* status variables								*/
#if MYSQL_VERSION_ID >= 50118
	pbms_system_variables, 							/* system variables								*/
#else
	NULL,
#endif
	NULL											/* config options								*/
}
mysql_declare_plugin_end;
#endif //DRIZZLED


