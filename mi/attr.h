/*
 * $Id: attr.h 1129 2006-10-02 15:48:57Z bogdan_iancu $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 *
 * History:
 * ---------
 *  2006-09-08  first version (bogdan)
 */


#ifndef _MI_ATTR_H
#define _MI_ATTR_H

#include <stdarg.h>
#include "../str.h"
#include "tree.h"

struct mi_attr{
	str name;
	str value;
	struct mi_attr *next;
};


struct mi_attr *add_mi_attr(struct mi_node *node, int flags,
	char *name, int name_len, char *value, int value_len);

struct mi_attr *addf_mi_attr(struct mi_node *node, int flags,
	char *name, int name_len, char *fmt_val, ...);

struct mi_attr *get_mi_attr_by_name(struct mi_node *node,
	char *name, int len);

void del_mi_attr_list(struct mi_node *node);


#endif

