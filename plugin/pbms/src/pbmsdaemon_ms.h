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

#pragma once

class PBMSDaemon {
public:
	typedef enum {DaemonUnknown, DaemonStartUp, DaemonRunning, DaemonShuttingDown, DaemonError} DaemonState;
	
private:
	static DaemonState pbd_state;
	static char pbd_home_dir[PATH_MAX];

public:
	static void setDaemonState(DaemonState state);
	static bool isDaemonState(DaemonState state) { return (pbd_state == state);}
	static const char *getPBMSDir() { return pbd_home_dir;}
	
};

