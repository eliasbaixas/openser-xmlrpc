/*
 * $Id: sub_list.c 2 2005-06-13 16:47:24Z bogdan_iancu $
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
/* History:
 * --------
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 */

#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "sub_list.h"

struct node*   append_to_list(struct node *head, char *offset, char *name)
{
	struct node *new_node;

	new_node = pkg_malloc(sizeof(struct node));
	if (!new_node)
		return 0;
	new_node->offset = offset;
	new_node->name = name;
	new_node->next = head;

	return new_node;
}




char* search_the_list(struct node *head, char *name)
{
	struct node *n;

	n = head;
	while (n) {
		if (strcasecmp(n->name,name)==0)
			return n->offset;
		n = n->next;
	}
	return 0;
}




void delete_list(struct node* head)
{
	struct node *n;
;
	while (head) {
		n=head->next;
		pkg_free(head);
		head = n;
	}
}

