/*
 * $Id: cpl_db.h 3128 2007-11-13 21:19:05Z bogdan_iancu $
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

#ifndef _CPL_DB_H
#define _CPL_DB_H

#include "../../db/db.h"


int cpl_db_bind(char* db_url, char* db_table);
int cpl_db_init(char* db_url, char* db_table);
void cpl_db_close();

extern char *cpl_username_col;
extern char *cpl_domain_col;
extern char *cpl_xml_col;
extern char *cpl_bin_col;

/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(str *username, str*domain, str *xml, str *bin);


/* fetch from database the binary format of the cpl script for a given user
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script(str *username, str*domain, str *script, const char *key);


/* delete from database the entire record for a given user - if a user has no
 * script, he will be removed completely from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(str *username, str *domain);


#endif
