/*
 * $Id: auth_diameter.h 1576 2007-02-05 15:36:28Z miconda $ 
 *
 * Digest Authentication - Diameter support
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
 *  
 *  
 */

#ifndef AUTHDIAM_MOD_H
#define AUTHDIAM_MOD_H

#include "../../parser/msg_parser.h"
#include "../sl/sl_api.h"
#include "defs.h"
#define M_NAME "auth_diameter"

extern char *diameter_client_host;
extern int diameter_client_port;

/** SL binds */
extern struct sl_binds slb;

int sockfd;
int use_domain;
extern rd_buf_t *rb;


#endif /* AUTHDIAM_MOD_H */
 
