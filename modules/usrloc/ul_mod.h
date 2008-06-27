/*
 * $Id: ul_mod.h 2839 2007-10-03 17:34:47Z bogdan_iancu $
 *
 * User location module interface
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
 * ---------
 */


#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../db/db.h"
#include "../../str.h"


/*
 * Module parameters
 */


#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2
#define DB_ONLY       3

#define UL_TABLE_VERSION 1004

extern str user_col;
extern str domain_col;
extern str contact_col;
extern str expires_col;
extern str q_col;
extern str callid_col;
extern str cseq_col;
extern str flags_col;
extern str cflags_col;
extern str user_agent_col;
extern str received_col;
extern str path_col;
extern str sock_col;
extern str methods_col;
extern str last_mod_col;

extern str db_url;
extern int timer_interval;
extern int db_mode;
extern int use_domain;
extern int desc_time_order;
extern int cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;

extern db_con_t* ul_dbh;   /* Database connection handle */
extern db_func_t ul_dbf;


/*
 * Matching algorithms
 */
#define CONTACT_ONLY            (0)
#define CONTACT_CALLID          (1)

extern int matching_mode;


#endif /* UL_MOD_H */
