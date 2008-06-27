/* 
 * $Id: db_con.h 2 2005-06-13 16:47:24Z bogdan_iancu $ 
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
 */



#ifndef DB_CON_H
#define DB_CON_H


/*
 * This structure represents a database connection
 * and pointer to this structure is used as a connection
 * handle
 */
typedef struct {
	const char* table;     /* Default table to use */
	unsigned long tail;    /* Variable length tail
				* database module specific */    
} db_con_t;


#define CON_TABLE(cn)      ((cn)->table)
#define CON_TAIL(cn)       ((cn)->tail)


#endif /* DB_CON_H */