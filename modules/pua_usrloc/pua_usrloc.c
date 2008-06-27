/*
 * $Id: pua_usrloc.c 2845 2007-10-04 11:21:22Z miconda $
 *
 * pua_usrloc module - usrloc pua module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2006-11-29  initial version (anca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../script_cb.h"
#include "../../sr_module.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../pt.h"
#include "../usrloc/ul_mod.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../pua/pua_bind.h"
#include "pua_usrloc.h"

MODULE_VERSION

str default_domain= {NULL, 0};
int pua_ul_publish= 0;
pua_api_t pua;
str pres_prefix= {0, 0};

/* Structure containing pointers to usrloc functions */
usrloc_api_t ul;

/** module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);

int pua_set_publish(struct sip_msg* , char*, char*);
void ul_publish(ucontact_t* c, int type, void* param);

send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;

static cmd_export_t cmds[]=
{
	{"pua_set_publish",			pua_set_publish,   0, 0, 0, REQUEST_ROUTE}, 	
	{0,							0, 0,				   0, 0, 0} 
};

static param_export_t params[]={
	{"default_domain",	 STR_PARAM, &default_domain.s	 },
	{"entity_prefix",	 STR_PARAM, &pres_prefix.s		 },
	{0,							 0,			0            }
};

struct module_exports exports= {
	"pua_usrloc",				/* module name */
	DEFAULT_DLFLAGS,            /* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0,							/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	(response_function) 0,		/* response handling function */
	destroy,					/* destroy function */
	child_init                  /* per-child init function */
};
	
/**
 * init module function
 */
static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;
	bind_pua_t bind_pua;

	LM_DBG("initializing module ...\n");
	
	if(default_domain.s == NULL )
	{	
		LM_ERR("default domain not found\n");
		return -1;
	}
	default_domain.len= strlen(default_domain.s);
	
	if(pres_prefix.s == NULL )
	{	
		LM_DBG("No pres_prefix configured\n");
	}
	else
		pres_prefix.len= strlen(pres_prefix.s);

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc)
	{
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if (bind_usrloc(&ul) < 0)
	{
		LM_ERR("Can't bind usrloc\n");
		return -1;
	}
	if(ul.register_ulcb == NULL)
	{
		LM_ERR("Could not import ul_register_ulcb\n");
		return -1;
	}

	if(ul.register_ulcb(UL_CONTACT_INSERT, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for"
				" insert\n");
		return -1;
	}
	if(ul.register_ulcb(UL_CONTACT_EXPIRE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for"
				" expire\n");
		return -1;
	}
	
	if(ul.register_ulcb(UL_CONTACT_UPDATE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for update\n");
		return -1;
	}
	
	if(ul.register_ulcb(UL_CONTACT_DELETE, ul_publish, 0)< 0)
	{
		LM_ERR("can not register callback for delete\n");
		return -1;
	}
	
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(pua.send_publish == NULL)
	{
		LM_ERR("Could not import send_publish\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	if(pua.send_subscribe == NULL)
	{
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;
	
	/* register post-script pua_unset_publish unset function */
	if(register_script_cb(pua_unset_publish, POST_SCRIPT_CB|REQ_TYPE_CB, 0)<0)
	{
		LM_ERR("failed to register POST request callback\n");
		return -1;
	}


	return 0;
}

static int child_init(int rank)
{
	LM_DBG("child [%d]  pid [%d]\n", rank, getpid());
	return 0;
}	

static void destroy(void)
{	
	LM_DBG("destroying module ...\n");

	return ;
}



