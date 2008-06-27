/* 
 * $Id: uri_mod.c 2943 2007-10-19 15:15:01Z anca_vamanu $ 
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
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
 *  2004-04-14: uri_param and add_uri_param introduced (jih)
 *  2004-05-03: tel2sip introduced (jih)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "checks.h"

MODULE_VERSION


static int uri_fixup(void** param, int param_no);
static int pvar_fixup(void** param, int param_no);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_user",        is_user,        1, str_fixup, 0, REQUEST_ROUTE},
	{"has_totag", 	   has_totag,      0, 0, 0,         REQUEST_ROUTE},
	{"uri_param",      uri_param_1,    1, str_fixup, 0, REQUEST_ROUTE},
	{"uri_param",      uri_param_2,    2, uri_fixup, 0, REQUEST_ROUTE},
	{"add_uri_param",  add_uri_param,  1, str_fixup, 0, REQUEST_ROUTE},
	{"tel2sip",        tel2sip,        0, 0,         0, REQUEST_ROUTE},
	{"is_uri_user_e164", is_uri_user_e164, 1, pvar_fixup, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	0,         /* exported MI functions */
	0,         /* exported pseudo-variables */
	0,         /* extra processes */
	0,         /* module initialization function */
	0,         /* response function */
	0,         /* destroy function */
	0          /* child initialization function */
};


/*
 * Convert both uri_param parameters to str* representation
 */
static int uri_fixup(void** param, int param_no)
{
       if (param_no == 1) {
               return str_fixup(param, 1);
       } else if (param_no == 2) {
               return str_fixup(param, 1);
       }
       return 0;
}

/*
 * Convert pvar into parsed pseudo variable specification
 */
static int pvar_fixup(void** param, int param_no)
{
    pv_spec_t *sp;
	str s;

    if (param_no == 1) { /* pseudo variable */

	sp = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
	if (sp == 0) {
	    LM_ERR("no pkg memory left\n");
	    return -1;
	}

	s.s = (char*)*param; s.len = strlen(s.s);
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
