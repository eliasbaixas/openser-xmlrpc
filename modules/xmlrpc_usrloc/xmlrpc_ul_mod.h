/*
 * $Id: ul_mod.h 2839 2007-10-03 17:34:47Z bogdan_iancu $
 *
 * User location module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 */


#ifndef UL_MOD_H
#define UL_MOD_H

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "../../str.h"
#include "usrloc.h"

extern int ul_hash_size;
extern unsigned int init_flag;
extern xmlrpc_env env;
extern char* server_url;
extern char *secret;
extern char secret_sha1[];
extern unsigned char secret_sha1_bin[];
extern int desc_time_order;

int xmlrpc_bind_usrloc(usrloc_api_t* api);
struct mi_root* xmlrpc_loc_get(struct mi_root* cmd_tree, void *param);
struct mi_root* xmlrpc_loc_put(struct mi_root* cmd_tree, void *param);
void die_if_fault_occurred (xmlrpc_env *env);

enum xmlrpc_result { Success=0, Capacity, Again };

/*
 * Module parameters
 */

#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2
#define DB_ONLY       3
extern int db_mode;
extern int use_domain;
extern int desc_time_order;
extern int cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;

#define CONTACT_ONLY            (0)
#define CONTACT_CALLID          (1)
extern int matching_mode;

#endif /* UL_MOD_H */
