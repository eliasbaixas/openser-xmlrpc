/*
 * $Id: uridb_mod.h 2 2005-06-13 16:47:24Z bogdan_iancu $
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 * 2003-02-26: created by janakj
 */


#ifndef URIDB_MOD_H
#define URIDB_MOD_H

#include "../../db/db.h"
#include "../../str.h"

/*
 * Module parameters variables
 */
extern str uri_table;                 /* Name of URI table */
extern str uri_user_col;              /* Name of username column in URI table */
extern str uri_domain_col;            /* Name of domain column in URI table */
extern str uri_uriuser_col;           /* Name of uri_user column in URI table */
extern str subscriber_table;          /* Name of subscriber table */
extern str subscriber_user_col;       /* Name of user column in subscriber table */
extern str subscriber_domain_col;     /* Name of domain column in subscriber table */
extern int use_uri_table;             /* Whether or not should be uri table used */
extern int use_domain;                /* Should does_uri_exist honor the domain part ? */

#endif /* URI_MOD_H */
