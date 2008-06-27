/* 
 * $Id: list.c 2943 2007-10-19 15:15:01Z anca_vamanu $ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-04-04  simplified link list (sgupta)
 *  2006-05-05  removed static allocation of 1k per column data (sgupta)
 */

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "list.h"

int insert(list** start, list** link, int n, strn* value)
{
	int i = 0;

	if(!(*start)) {
		*link = (list*)pkg_malloc(sizeof(list));
		if(!(*link)) {
			LM_ERR("no more pkg memory (1)\n");
			return -1;
		}
		(*link)->rownum = n;
		(*link)->next = NULL;

		(*link)->lengths = (unsigned long*)pkg_malloc(sizeof(unsigned long)*n);
		if(!(*link)->lengths) {
			LM_ERR("no more pkg memory (2)\n");
			pkg_free(*link);
			*link = NULL;
			return -1;
		}
		for(i=0; i<n; i++)
			(*link)->lengths[i] = strlen(value[i].s) + 1;

		(*link)->data = (char**)pkg_malloc(sizeof(char*)*n);
		if(!(*link)->data) {
			LM_ERR("no more pkg memory (3)\n");
			pkg_free( (*link)->lengths );
			pkg_free(*link);
			*link = NULL;
			return -1;
		}

		for(i=0; i<n; i++) {
			(*link)->data[i] = pkg_malloc(sizeof(char) * (*link)->lengths[i]);
			if(!(*link)->data[i]) {
				LM_ERR("no more pkg memory (4)\n");
				pkg_free( (*link)->lengths );
				pkg_free( (*link)->data );
				pkg_free(*link);
				*link = NULL;
				return -1;
			}
			strncpy((*link)->data[i], value[i].s, (*link)->lengths[i]);
		}
	
		*start = *link;
		return 0;
	}
	else
	{
		list* nlink;
		nlink=(list*)pkg_malloc(sizeof(list));
		if(!nlink) {
			LM_ERR("no more pkg memory (5)\n");
			return -1;
		}
		nlink->rownum = n;

		nlink->lengths = (unsigned long*)pkg_malloc(sizeof(unsigned long)*n);
		if(!nlink->lengths) {
			LM_ERR("no more pkg memory (6)\n");
			pkg_free(nlink);
			nlink = NULL;
			return -1;
		}
		for(i=0; i<n; i++)
			nlink->lengths[i] = strlen(value[i].s) + 1;

		nlink->data = (char**)pkg_malloc(sizeof(char*)*n);
		if(!nlink->data) {
			LM_ERR("no more pkg memory (7)\n");
			pkg_free( nlink->lengths );
			pkg_free(nlink);
			nlink = NULL;
			return -1;
		}

		for(i=0; i<n; i++) {
			nlink->data[i] = pkg_malloc(sizeof(char) * nlink->lengths[i]);
			if(!nlink->data[i]) {
				LM_ERR("no more pkg memory (8)\n");
				pkg_free( nlink->lengths );
				pkg_free( nlink->data );
				pkg_free(nlink);
				nlink = NULL;
				return -1;
			}
			strncpy(nlink->data[i], value[i].s, nlink->lengths[i]);
		}

		nlink->next = NULL;
		(*link)->next = nlink;
		*link = (*link)->next;

		return 0;
	}
}


void destroy(list *start)
{
	int i = 0;

	while(start) {
		list* temp = start;
		start = start->next;
		for(i = 0; i < temp->rownum; i++)
			pkg_free( temp->data[i] );
		pkg_free(temp->data);
		pkg_free(temp->lengths);
		pkg_free(temp);
	}
}

