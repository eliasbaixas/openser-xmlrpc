/*
 * $Id: authorize.c 2802 2007-09-21 14:11:40Z bogdan_iancu $
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
 * 2003-03-09: Based on authorize.c from radius_auth (janakj)
 * 2006-03-01: pseudo variables support for domain name (bogdan)
 */


#include <string.h>
#include <stdlib.h>
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../auth/api.h"
#include "authorize.h"
#include "sterman.h"
#include "authrad_mod.h"


/* 
 * Extract URI depending on the request from To or From header 
 */
static inline int get_uri_user(struct sip_msg* _m, str** _uri_user)
{
    struct sip_uri *puri;

    if ((REQ_LINE(_m).method.len == 8) && 
	(memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
	if ((puri=parse_to_uri(_m))==NULL) {
	    LM_ERR("failed to parse To header\n");
	    return -1;
	}
    } else {
	if ((puri=parse_from_uri(_m))==NULL) {
	    LM_ERR("parsing From header\n");
	    return -1;
	}
    }

    *_uri_user = &(puri->user);

    return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _msg, pv_elem_t* _realm,
			    pv_spec_t * _uri_user, int _hftype)
{
    int res;
    auth_result_t ret;
    struct hdr_field* h;
    auth_body_t* cred;
    str *uri_user;
    str user, domain;
    pv_value_t pv_val;

    /* get pre_auth domain from _realm pvar (if exists) */
    if (_realm) {
	if (pv_printf_s(_msg, _realm, &domain)!=0) {
	    LM_ERR("pv_printf_s failed\n");
	    return AUTH_ERROR;
	}
    } else {
	/* get pre_auth domain from To/From header */
	domain.len = 0;
	domain.s = 0;
    }

    ret = auth_api.pre_auth(_msg, &domain, _hftype, &h);

    if (ret != DO_AUTHORIZATION)
	return ret;

    cred = (auth_body_t*)h->parsed;

    /* get uri_user from _uri_user pvap (if exists) or
       from To/From URI */
    if (_uri_user) {
	if (pv_get_spec_value(_msg, _uri_user, &pv_val) == 0) {
	    if (pv_val.flags & PV_VAL_STR) {
		res = radius_authorize_sterman(_msg, &cred->digest, 
					       &_msg->first_line.u.request.method,
					       &pv_val.rs);
	    } else {
		LM_ERR("uri_user pvar value is not string\n");
		return AUTH_ERROR;
	    }
	} else {
	    LM_ERR("cannot get uri_user pvar value\n");
	    return AUTH_ERROR;
	}
    } else {
	if (get_uri_user(_msg, &uri_user) < 0) {
	    LM_ERR("To/From URI not found\n");
	    return AUTH_ERROR;
	}
	user.s = (char *)pkg_malloc(uri_user->len);
	if (user.s == NULL) {
	    LM_ERR("no pkg memory left for user\n");
	    return AUTH_ERROR;
	}
	un_escape(uri_user, &user);
	res = radius_authorize_sterman(_msg, &cred->digest, 
				       &_msg->first_line.u.request.method,
				       &user);
	pkg_free(user.s);
    }

    if (res == 1) {
	ret = auth_api.post_auth(_msg, h);
	return ret;
    }

    return AUTH_ERROR;
}


/*
 * Authorize using Proxy-Authorize header field (no URI user parameter given)
 */
int radius_proxy_authorize_1(struct sip_msg* _msg, char* _realm, char* _s2)
{
    /* realm parameter is converted to str* in str_fixup */
    return authorize(_msg, (pv_elem_t*)_realm, (pv_spec_t *)0,
		     HDR_PROXYAUTH_T);
}


/*
 * Authorize using Proxy-Authorize header field (URI user parameter given)
 */
int radius_proxy_authorize_2(struct sip_msg* _msg, char* _realm,
			     char* _uri_user)
{
    return authorize(_msg, (pv_elem_t*)_realm, (pv_spec_t *)_uri_user,
		     HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _s2)
{
	return authorize(_msg, (pv_elem_t*)_realm, (pv_spec_t *)0,
			 HDR_AUTHORIZATION_T);
}
