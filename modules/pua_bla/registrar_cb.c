/*
 * $Id: registrar_cb.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-03-30  initial version (anca)
 */

#include<stdio.h>
#include<stdlib.h>
#include "../../dprint.h"
#include "../pua/pua.h"
#include "registrar_cb.h"
#include "pua_bla.h"

extern int is_bla_aor;

void bla_cb(ucontact_t* c, int type, void* param)
{
	subs_info_t subs;
	str uri={0, 0};
	char* at;
	LM_DBG("start\n");
	if(is_bla_aor== 0)
	{
		LM_DBG("Not a recognized BLA AOR\n");
		return ;
	}	

	if(type & UL_CONTACT_INSERT)
		LM_DBG("type= UL_CONTACT_INSERT\n");
	else
	if(type & UL_CONTACT_UPDATE)
		LM_DBG("type= UL_CONTACT_UPDATE\n");
	else
	if(type & UL_CONTACT_EXPIRE)
		LM_DBG("type= UL_CONTACT_EXPIRE\n");
	else
	if(type & UL_CONTACT_DELETE)
		LM_DBG("type= UL_CONTACT_DELETE\n");

	memset(&subs, 0, sizeof(subs_info_t));
	subs.remote_target= &c->c;
	
	subs.pres_uri= &reg_from_uri;

	uri.s = (char*)pkg_malloc(sizeof(char)*(c->aor->len+default_domain.len+6));
	if(uri.s == NULL)
		goto error;

	memcpy(uri.s, "sip:", 4);
	uri.len = 4;

	memcpy(uri.s+ uri.len, c->aor->s, c->aor->len);
	uri.len+= c->aor->len;
	at = memchr(c->aor->s, '@', c->aor->len);
	if(!at)
	{
		uri.s[uri.len++]= '@';
		memcpy(uri.s+ uri.len, default_domain.s, default_domain.len);
		uri.len+= default_domain.len;		
	}
	
	subs.watcher_uri= &uri;
	if(type & UL_CONTACT_DELETE || type & UL_CONTACT_EXPIRE )
		subs.expires= 0;
	else
		subs.expires= c->expires - (int)time(NULL);
		

	subs.source_flag= BLA_SUBSCRIBE;
	subs.event= BLA_EVENT;
	subs.contact= &server_address;
	
	if(outbound_proxy.s && outbound_proxy.len)
	{
		LM_DBG("outbound_proxy= %.*s\n", outbound_proxy.len, 
				outbound_proxy.s);
		subs.outbound_proxy= &outbound_proxy;
	}
	else
		subs.outbound_proxy= NULL;

	if(type & UL_CONTACT_INSERT)
		subs.flag|= INSERT_TYPE;
	else
		subs.flag|= UPDATE_TYPE;

	if(pua_send_subscribe(&subs)< 0)
	{
		LM_ERR("while sending subscribe\n");
	}	
	pkg_free(uri.s);
error:
	is_bla_aor= 0;
	return ;
}	
