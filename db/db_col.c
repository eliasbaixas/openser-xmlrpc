/* 
 * $Id: db_col.c 2871 2007-10-05 14:32:16Z henningw $ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007 1und1 Internet AG
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

#include "db_col.h"

#include "../dprint.h"
#include "../mem/mem.h"

/*
 * Release memory used by columns
 */
inline int db_free_columns(db_res_t* _r)
{
	if (!_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (RES_NAMES(_r)) pkg_free(RES_NAMES(_r));
	if (RES_TYPES(_r)) pkg_free(RES_TYPES(_r));
	return 0;
}
