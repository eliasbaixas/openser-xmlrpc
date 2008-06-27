/*
 * $Id: $
 *
 * OpenSER H.350 Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
 *
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
 * 2007-03-12: Initial version
 */


#ifndef H350_MOD_H
#define H350_MOD_H

#include "../../str.h"
#include "../ldap/api.h"

/*
 * global pointer to ldap api
 */
extern ldap_api_t ldap_api;

/*
 * global module parameters
 */

#define H350_LDAP_SESSION ""
#define H350_BASE_DN ""
#define H350_SEARCH_SCOPE "one"

extern str h350_ldap_session;
extern str h350_base_dn;
extern str h350_search_scope;
extern int h350_search_scope_int;

#endif /* H350_MOD_H */
