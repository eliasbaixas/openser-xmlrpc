/* 
 * $Id: uridb_mod.c 2895 2007-10-10 13:04:30Z dan_pascu $ 
 *
 * Various URI related functions
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
 * -------
 *  2003-03-11: New module interface (janakj)
 *  2003-03-16: flags export parameter added (janakj)
 *  2003-03-19  replaces all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-05: default_uri #define used (jiri)
 *  2004-03-20: has_totag introduced (jiri)
 *  2004-06-07  updated to the new DB api (andrei)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "uridb_mod.h"
#include "checks.h"

MODULE_VERSION

/*
 * Version of domain table required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define URI_TABLE_VERSION 1
#define SUBSCRIBER_TABLE_VERSION 3

static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mod_init(void);       /* Module initialization function */


#define URI_TABLE "uri"
#define URI_TABLE_LEN (sizeof(URI_TABLE) - 1)

#define USER_COL "username"
#define USER_COL_LEN (sizeof(USER_COL) - 1)

#define DOMAIN_COL "domain"
#define DOMAIN_COL_LEN (sizeof(DOMAIN_COL) - 1)

#define URI_USER_COL "uri_user"
#define URI_USER_COL_LEN (sizeof(URI_USER_COL) - 1)

#define SUBSCRIBER_TABLE "subscriber"
#define SUBSCRIBER_TABLE_LEN (sizeof(SUBSCRIBER_TABLE) - 1)


/*
 * Module parameter variables
 */
static str db_url         = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};
/* Name of URI table */
str uri_table             = {URI_TABLE, URI_TABLE_LEN};
/* Name of username column in URI table */
str uri_user_col          = {USER_COL, USER_COL_LEN};
/* Name of domain column in URI table */
str uri_domain_col        = {DOMAIN_COL, DOMAIN_COL_LEN};
/* Name of uri_user column in URI table */
str uri_uriuser_col       = {URI_USER_COL, URI_USER_COL_LEN};
/* Name of subscriber table */
str subscriber_table      = {SUBSCRIBER_TABLE, SUBSCRIBER_TABLE_LEN};
/* Name of user column in subscriber table */
str subscriber_user_col   = {USER_COL, USER_COL_LEN};
/* Name of domain column in subscriber table */
str subscriber_domain_col = {DOMAIN_COL, DOMAIN_COL_LEN};

int use_uri_table = 0;     /* Should uri table be used */
int use_domain = 0;        /* Should does_uri_exist honor the domain part ? */

static int fixup_exist(void** param, int param_no);
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"check_to",       check_to,       0, 0, 0,           REQUEST_ROUTE},
	{"check_from",     check_from,     0, 0, 0,           REQUEST_ROUTE},
	{"does_uri_exist", does_uri_exist, 0, fixup_exist, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",                   STR_PARAM, &db_url.s               },
	{"uri_table",                STR_PARAM, &uri_table.s            },
	{"uri_user_column",          STR_PARAM, &uri_user_col.s         },
	{"uri_domain_column",        STR_PARAM, &uri_domain_col.s       },
	{"uri_uriuser_column",       STR_PARAM, &uri_uriuser_col.s      },
	{"subscriber_table",         STR_PARAM, &subscriber_table.s     },
	{"subscriber_user_column",   STR_PARAM, &subscriber_user_col.s  },
	{"subscriber_domain_column", STR_PARAM, &subscriber_domain_col.s},
	{"use_uri_table",            INT_PARAM, &use_uri_table          },
	{"use_domain",               INT_PARAM, &use_domain             },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri_db",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	0 ,        /* exported MI functions */
	0,         /* exported pseudo-variables */
	0,         /* extra processes */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	child_init /* child initialization function */
};


/*
 * Module initialization function callee in each child separately
 */
static int child_init(int rank)
{
	if (db_url.len)
		return uridb_db_init(db_url.s);
	else
		return 0;
}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
	int ver;

	LM_DBG("uri_db - initializing\n");

	db_url.len = strlen(db_url.s);
	if (db_url.len == 0 ) {
		if (use_uri_table) {
            LM_ERR("configuration error - no database URL, "
                   "but use_uri_table is set!\n");
			goto error;
		}
		return 0;
	}

	uri_table.len = strlen(uri_table.s);
	uri_user_col.len = strlen(uri_user_col.s);
	uri_domain_col.len = strlen(uri_domain_col.s);
	uri_uriuser_col.len = strlen(uri_uriuser_col.s);
	subscriber_table.len = strlen(subscriber_table.s);
	subscriber_user_col.len = strlen(subscriber_user_col.s);
	subscriber_domain_col.len = strlen(subscriber_domain_col.s);

	if (uridb_db_bind(db_url.s)) {
		LM_ERR("No database module found\n");
		goto error;
	}

	if (use_uri_table) {
		/* Check table version */
		ver = uridb_db_ver(db_url.s, &uri_table);
		if (ver < 0) {
			LM_ERR("Error while querying table version\n");
			goto error;
		} else if (ver < URI_TABLE_VERSION) {
			LM_ERR("Invalid table version of the uri table\n");
			goto error;
		}
	} else {
		/* Check table version */
		ver = uridb_db_ver(db_url.s, &subscriber_table);
		if (ver < 0) {
			LM_ERR("Error while querying table version\n");
			goto error;
		} else if (ver < SUBSCRIBER_TABLE_VERSION) {
			LM_ERR("Invalid table version of the subscriber table\n");
			goto error;
		}
	}

	return 0;
error:
	return -1;
}


static void destroy(void)
{
	uridb_db_close();
}


static int fixup_exist(void** param, int param_no)
{
	if (db_url.len == 0) {
        LM_ERR("configuration error - does_uri_exist() called with no database URL!\n");
		return E_CFG;
	}
	return 0;
}
