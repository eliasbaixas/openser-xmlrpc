/*
 * $Id: usrloc.h 3726 2008-02-18 16:00:35Z bogdan_iancu $
 *
 * Convenience usrloc header file
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
 */


#ifndef USRLOC_H
#define USRLOC_H


#include "xmlrpc_dlist.h"
#include "xmlrpc_udomain.h"
#include "xmlrpc_urecord.h"
#include "xmlrpc_ucontact.h"
#include "ul_callback.h"


typedef struct usrloc_api {
	int           use_domain;
	int           db_mode;
	unsigned int  nat_flag;

	register_udomain_t   register_udomain;
	get_all_ucontacts_t  get_all_ucontacts;

	insert_urecord_t     insert_urecord;
	delete_urecord_t     delete_urecord;
	get_urecord_t        get_urecord;
	lock_udomain_t       lock_udomain;
	unlock_udomain_t     unlock_udomain;

	release_urecord_t    release_urecord;
	insert_ucontact_t    insert_ucontact;
	delete_ucontact_t    delete_ucontact;
	get_ucontact_t       get_ucontact;

	update_ucontact_t    update_ucontact;

	register_ulcb_t      register_ulcb;
} usrloc_api_t;


typedef int (*bind_usrloc_t)(usrloc_api_t* api);


#endif /* USRLOC_H */
