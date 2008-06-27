/*
 * $Id:  $
 *
 * db_berkeley MI functions
 *
 * Copyright (C) 2007  Cisco Systems
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
 *  2007-11-05  created (wiquan)
 */


#include "../../dprint.h"
#include "../../db/db.h"
#include "db_berkeley.h"
#include "bdb_mi.h"


/*
 * MI function to reload db table or env
 * expects 1 node: the tablename or dbenv name to reload
 */
struct mi_root* mi_bdb_reload(struct mi_root *cmd, void *param)
{
	struct mi_node *node;
	str *db_path;
	
	node = cmd->node.kids;
	if (node && node->next)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	
	db_path = &node->value;
	
	if (!db_path || db_path->len == 0)
		return init_mi_tree( 400, "bdb_reload missing db arg", 21);

	if (bdb_reload(db_path->s) == 0) 
	{
		return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	} 
	else 
	{
		return init_mi_tree( 500, "db_berkeley Reload Failed", 26);
	}
}

