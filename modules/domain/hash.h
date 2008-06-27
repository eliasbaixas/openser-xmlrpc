/*
 * $Id: hash.h 1506 2007-01-15 10:22:40Z bogdan_iancu $
 *
 * Header file for hash table functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#ifndef _DOM_HASH_H_
#define _DOM_HASH_H_

#include <stdio.h>
#include "domain_mod.h"
#include "../../mi/mi.h"

int hash_table_install (struct domain_list **hash_table, char *domain);
int hash_table_lookup (str *domain);
int hash_table_mi_print(struct domain_list **hash_table, struct mi_node* rpl);
void hash_table_free (struct domain_list **hash_table);

#endif
