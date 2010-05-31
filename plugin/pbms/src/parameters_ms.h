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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Barry Leslie
 *
 * 2010-05-25
 *
 * PBMS daemon global parameters.
 *
 */

#ifndef __PARAMETERS_MS_H__
#define __PARAMETERS_MS_H__

class PBMSParameters {
	static uint32_t server_id;
	
	public:
	
	static uint32_t getServerID() { return server_id;}
	static void setServerID(uint32_t id) { server_id = id;}
	
	static bool isBLOBDatabase(const char *) { return true;}

};

#endif // __PARAMETERS_MS_H__