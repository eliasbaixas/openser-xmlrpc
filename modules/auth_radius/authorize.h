/*
 * $Id: authorize.h 1453 2006-12-25 19:30:00Z juhe $
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
 * 2003-03-09: Based on authorize.h from radius_auth (janakj)
 */

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include "../../parser/msg_parser.h"


/*
 * Authorize using Proxy-Authorize header field (no from parameter given)
 */
int radius_proxy_authorize_1(struct sip_msg* _msg, char* _realm, char* _s2);


/*
 * Authorize using Proxy-Authorize header field (from parameter given)
 */
int radius_proxy_authorize_2(struct sip_msg* _msg, char* _realm, char* _from);


/*
 * Authorize using WWW-Authorization header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _s2);


#endif /* AUTHORIZE_H */
