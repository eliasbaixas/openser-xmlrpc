/* 
 * $Id: pg_con.c 3610 2008-02-01 16:26:16Z henningw $
 *
 * Copyright (C) 2001-2004 iptel.org
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

#include "pg_con.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include <string.h>
#include <time.h>


/*
 * Create a new connection structure,
 * open the PostgreSQL connection and set reference count to 1
 */
struct pg_con* pg_new_conn(struct db_id* id)
{
	struct pg_con* ptr;
        char *ports;

	LM_DBG("db_id = %p\n", id);
 
	if (!id) {
		LM_ERR("invalid db_id parameter value\n");
		return 0;
	}

	ptr = (struct pg_con*)pkg_malloc(sizeof(struct pg_con));
	if (!ptr) {
		LM_ERR("failed trying to allocated %lu bytes for connection structure."
				"\n", (unsigned long)sizeof(struct pg_con));
		return 0;
	}
	LM_DBG("%p=pkg_malloc(%lu)\n", ptr, (unsigned long)sizeof(struct pg_con));

	memset(ptr, 0, sizeof(struct pg_con));
	ptr->ref = 1;

	if (id->port) {
		ports = int2str(id->port, 0);
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s:%d/%s\n", ZSW(id->host),
			id->port, ZSW(id->database));
	} else {
		ports = NULL;
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s/%s\n", ZSW(id->host),
			ZSW(id->database));
	}

 	ptr->con = PQsetdbLogin(id->host, ports, NULL, NULL, id->database, id->username, id->password);
	LM_DBG("PQsetdbLogin(%p)\n", ptr->con);

	if( (ptr->con == 0) || (PQstatus(ptr->con) != CONNECTION_OK) )
	{
		LM_ERR("%s\n", PQerrorMessage(ptr->con));
		PQfinish(ptr->con);
		goto err;
	}

        ptr->pid = getpid();
        ptr->connected = 1;
	ptr->timestamp = time(0);
	ptr->id = id;

	return ptr;

 err:
	if (ptr) {
		LM_ERR("cleaning up %p=pkg_free()\n", ptr);
		pkg_free(ptr);
	}
	return 0;
}


/*
 * Close the connection and release memory
 */
void pg_free_conn(struct pg_con* con)
{

	if (!con) return;
	if (con->res) {
		LM_DBG("PQclear(%p)\n", con->res);
		PQclear(con->res);
		con->res = 0;
	}
	if (con->id) free_db_id(con->id);
	if (con->con) {
		LM_DBG("PQfinish(%p)\n", con->con);
		PQfinish(con->con);
		con->con = 0;
	}
	LM_DBG("pkg_free(%p)\n", con);
	pkg_free(con);
}
