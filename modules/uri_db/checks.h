/*
 * $Id: checks.h 2 2005-06-13 16:47:24Z bogdan_iancu $
 *
 * Various URI checks
 *
 * Copyright (C) 2001-2004 FhG FOKUS
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-03-26 created by janakj
 */


#ifndef CHECKS_H
#define CHECKS_H

#include "../../parser/msg_parser.h"


/*
 * Check if To header field contains the same username
 * as digest credentials
 */
int check_to(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Check if From header field contains the same username
 * as digest credentials
 */
int check_from(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Check if uri belongs to a local user, contributed by Juha Heinanen
 */
int does_uri_exist(struct sip_msg* _msg, char* _table, char* _s2);


int uridb_db_init(char* db_url);
int uridb_db_bind(char* db_url);
void uridb_db_close();
int uridb_db_ver(char* db_url, str* name);

#endif /* CHECKS_H */
