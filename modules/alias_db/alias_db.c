/* 
 * $Id: alias_db.c 2845 2007-10-04 11:21:22Z miconda $
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for openser, a free SIP server.
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
 * 2004-09-01: first version (ramona)
 */


#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"

#include "alookup.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);


/* Module parameter variables */
char* db_url           = DEFAULT_RODB_URL;
char* user_column      = "username";
char* domain_column    = "domain";
char* alias_user_column      = "alias_username";
char* alias_domain_column    = "alias_domain";
int   use_domain       = 0;
char* domain_prefix    = NULL;

str   dstrip_s;


db_con_t* db_handle;   /* Database connection handle */
db_func_t adbf;  /* DB functions */

/* Exported functions */
static cmd_export_t cmds[] = {
	{"alias_db_lookup", alias_db_lookup, 1, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",           STR_PARAM, &db_url          },
	{"user_column",      STR_PARAM, &user_column     },
	{"domain_column",    STR_PARAM, &domain_column   },
	{"alias_user_column",      STR_PARAM, &alias_user_column     },
	{"alias_domain_column",    STR_PARAM, &alias_domain_column   },
	{"use_domain",       INT_PARAM, &use_domain      },
	{"domain_prefix",    STR_PARAM, &domain_prefix   },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"alias_db", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};


/**
 *
 */
static int child_init(int rank)
{
	db_handle = adbf.init(db_url);
	if (!db_handle)
	{
		LM_ERR("unable to connect database\n");
		return -1;
	}
	return 0;

}


/**
 *
 */
static int mod_init(void)
{
	LM_INFO("initializing...\n");

    /* Find a database module */
	if (bind_dbmod(db_url, &adbf))
	{
		LM_ERR("unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY( adbf, DB_CAP_QUERY))
	{
		LM_CRIT("database modules does not "
			"provide all functions needed by avpops module\n");
		return -1;
	}
		

	if(domain_prefix==NULL || strlen(domain_prefix)==0)
	{
		dstrip_s.s   = 0;
		dstrip_s.len = 0;
	}
	else
	{
		dstrip_s.s   = domain_prefix;
		dstrip_s.len = strlen(domain_prefix);
	}

	return 0;
}


/**
 *
 */
static void destroy(void)
{
	if (db_handle) {
		adbf.close(db_handle);
		db_handle = 0;
	}
}

