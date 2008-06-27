/* 
 * $Id: authrad_mod.c 3222 2007-11-28 11:34:45Z henningw $ 
 *
 * Digest Authentication - Radius support
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
 *  2003-03-09: Based on auth_mod.c from radius_auth (janakj)
 *  2003-03-11: New module interface (janakj)
 *  2003-03-16: flags export parameter added (janakj)
 *  2003-03-19: all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2006-03-01: pseudo variables support for domain name (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../pvar.h"
#include "../../radius.h"
#include "../../mem/mem.h"
#include "authrad_mod.h"
#include "authorize.h"

MODULE_VERSION

struct attr attrs[A_MAX];
struct val vals[V_MAX];
void *rh;

auth_api_t auth_api;

static int mod_init(void);         /* Module initialization function */
static int auth_fixup(void** param, int param_no); /* char* -> str* */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int service_type = -1;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_www_authorize",   radius_www_authorize,   1, auth_fixup,
			0, REQUEST_ROUTE},
	{"radius_proxy_authorize", radius_proxy_authorize_1, 1, auth_fixup,
			0, REQUEST_ROUTE},
	{"radius_proxy_authorize", radius_proxy_authorize_2, 2, auth_fixup,
			0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config",    STR_PARAM, &radius_config   },
	{"service_type",     INT_PARAM, &service_type    },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"auth_radius", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0           /* child initialization function */
};


/*
 * Module initialization function
 */
static int mod_init(void)
{
	DICT_VENDOR *vend;
	bind_auth_t bind_auth;

	LM_INFO("initializing...\n");

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n			= "Service-Type";
	attrs[A_SIP_URI_USER].n			= "Sip-URI-User";
	attrs[A_DIGEST_RESPONSE].n		= "Digest-Response";
	attrs[A_DIGEST_ALGORITHM].n		= "Digest-Algorithm";
	attrs[A_DIGEST_BODY_DIGEST].n		= "Digest-Body-Digest";
	attrs[A_DIGEST_CNONCE].n		= "Digest-CNonce";
	attrs[A_DIGEST_NONCE_COUNT].n		= "Digest-Nonce-Count";
	attrs[A_DIGEST_QOP].n			= "Digest-QOP";
	attrs[A_DIGEST_METHOD].n		= "Digest-Method";
	attrs[A_DIGEST_URI].n			= "Digest-URI";
	attrs[A_DIGEST_NONCE].n			= "Digest-Nonce";
	attrs[A_DIGEST_REALM].n			= "Digest-Realm";
	attrs[A_DIGEST_USER_NAME].n		= "Digest-User-Name";
	attrs[A_USER_NAME].n			= "User-Name";
	attrs[A_CISCO_AVPAIR].n			= "Cisco-AVPair";
	attrs[A_SIP_AVP].n				= "SIP-AVP";
	vals[V_SIP_SESSION].n			= "Sip-Session";

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LM_ERR("failed to open configuration file \n");
		return -1;
	}

	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LM_ERR("failed to open dictionary file \n");
		return -2;
	}

	vend = rc_dict_findvend(rh, "Cisco");
	if (vend == NULL) {
		LM_DBG("no `Cisco' vendor in Radius dictionary\n");
		attrs[A_CISCO_AVPAIR].n = NULL;
	}

	bind_auth = (bind_auth_t)find_export("bind_auth", 0, 0);
	if (!bind_auth) {
		LM_ERR("unable to find bind_auth function. Check if you load the auth module.\n");
		return -1;
	}

	if (bind_auth(&auth_api) < 0) {
		LM_ERR("cannot bind to auth module\n");
		return -4;
	}

	INIT_AV(rh, attrs, A_MAX, vals, V_MAX, "auth_radius", -5, -6);

	if (service_type != -1) {
		vals[V_SIP_SESSION].v = service_type;
	}

	return 0;
}


/*
 * Convert char* parameter to pv_elem_t* parameter
 */
static int auth_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;
	pv_spec_t *sp;

	if (param_no == 1) { /* realm (string that may contain pvars) */
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			model = 0;
		} else {
			s.len = strlen(s.s);
			if (pv_parse_format(&s,&model)<0) {
				LM_ERR("pv_parse_format failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)model;
	}

	if (param_no == 2) { /* URI user (a pvar) */
		sp = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if (sp == 0) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
		s.s = (char*)*param;
		s.len = strlen(s.s);
		if (pv_parse_spec(&s, sp) == 0) {
			LM_ERR("parsing of pseudo variable %s failed!\n", (char*)*param);
			pkg_free(sp);
			return -1;
		}
		if (sp->type == PVT_NULL) {
			LM_ERR("bad pseudo variable\n");
			pkg_free(sp);
			return -1;
		}
		*param = (void*)sp;
	}	

	return 0;
}
