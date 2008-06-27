/*
 * $Id: speeddial.h 1473 2007-01-09 15:30:28Z miconda $
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 * 
 */


#ifndef _SPEEDDIAL_H_
#define _SPEEDDIAL_H_

#include "../../db/db.h"
#include "../../parser/msg_parser.h"


/* Module parameters variables */

extern char* user_column;     /* 'username' column name */
extern char* domain_column;   /* 'domain' column name */
extern char* sd_user_column;     /* 'sd_username' column name */
extern char* sd_domain_column;   /* 'sd_domain' column name */
extern char* new_uri_column;   /* 'new_uri' column name */
extern int   use_domain;      /* use or not the domain for sd lookup */
extern str   dstrip_s;

extern db_func_t db_funcs;    /* Database functions */
extern db_con_t* db_handle;   /* Database connection handle */

#endif /* _SPEEDDIAL_H_ */
