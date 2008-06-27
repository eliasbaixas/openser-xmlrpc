/*
 * $Id
 *
 * XML-RPC Usrloc interface
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
 * ========
 *
 * 2006-11-28 Added a new function to the usrloc_api, to retrieve the number
 *            of registered users.  (Jeffrey Magder - SOMA Networks)
 */

 
#include "../../sr_module.h"
#include "xmlrpc_ul_mod.h"

int xmlrpc_bind_usrloc(usrloc_api_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if (init_flag==0) {
		LM_ERR("configuration error - trying to bind to XMLRPC-usrloc module"
				" before being initialized\n");
		return -1;
	}

	api->register_udomain   = register_udomain;
	api->get_all_ucontacts  = get_all_ucontacts;
	api->insert_urecord     = insert_urecord;
	api->delete_urecord     = delete_urecord;
	api->get_urecord        = get_urecord;
	api->lock_udomain       = lock_udomain;/*noop*/
	api->unlock_udomain     = unlock_udomain;/*noop*/
	api->release_urecord    = release_urecord;
	api->insert_ucontact    = insert_ucontact;
	api->delete_ucontact    = delete_ucontact;
	api->get_ucontact       = get_ucontact;
	api->update_ucontact    = update_ucontact;
	api->register_ulcb      = register_ulcb;

	api->use_domain = 1; /* force to always use domain when registering AoR */
#define DB_ONLY       3
	api->db_mode    = DB_ONLY; /* will be using XML-RPC as the database ! */
	api->nat_flag   = 0; /* as if it were not set */

	return 0;
}