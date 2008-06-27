/*
 * $Id: avpops_db.h 2725 2007-09-09 20:55:56Z miconda $
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 *  2004-11-11  added support for db schemes for avp_db_load (ramona)
 */



#ifndef _AVP_OPS_DB_H_
#define _AVP_OPS_DB_H_

#include "../../db/db.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../pvar.h"


/* definition of a DB scheme*/
struct db_scheme
{
	char *name;
	char *uuid_col;
	char *username_col;
	char *domain_col;
	char *value_col;
	char *table;
	int  db_flags;
	struct db_scheme *next;
};


int avpops_db_bind(char* db_url);

int avpops_db_init(char* db_url, char* db_table, char **db_columns);

db_res_t *db_load_avp( str *uuid, str *username, str *domain,
		char *attr, char *table, struct db_scheme *scheme);

void db_close_query( db_res_t *res );

int db_store_avp( db_key_t *keys, db_val_t *vals, int n, char *table);

int db_delete_avp( str *uuid, str *username, str *domain,
		char *attr, char *table);

int db_query_avp(struct sip_msg* msg, char *query, pvname_list_t* dest);

int avp_add_db_scheme( modparam_t type, void* val);

struct db_scheme *avp_get_db_scheme( char *name );

#endif
