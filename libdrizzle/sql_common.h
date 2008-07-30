/* Copyright (C) 2003-2004, 2006 MySQL AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


extern const char	*unknown_sqlstate;
extern const char	*cant_connect_sqlstate;
extern const char	*not_error_sqlstate;

#ifdef	__cplusplus
extern "C" {
#endif

extern CHARSET_INFO *default_client_charset_info;
DRIZZLE_FIELD *unpack_fields(DRIZZLE_DATA *data,MEM_ROOT *alloc,uint fields,
			   my_bool default_value, uint server_capabilities);
void free_rows(DRIZZLE_DATA *cur);
void free_old_query(DRIZZLE *drizzle);
void end_server(DRIZZLE *drizzle);
my_bool drizzle_reconnect(DRIZZLE *drizzle);
void drizzle_read_default_options(struct st_drizzle_options *options,
				const char *filename,const char *group);
bool
cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
		     const unsigned char *header, uint32_t header_length,
		     const unsigned char *arg, uint32_t arg_length, bool skip_check);
uint32_t cli_safe_read(DRIZZLE *drizzle);
void net_clear_error(NET *net);
void set_drizzle_error(DRIZZLE *drizzle, int errcode, const char *sqlstate);
#ifdef	__cplusplus
}
#endif

#define protocol_41(A) ((A)->server_capabilities & CLIENT_PROTOCOL_41)

