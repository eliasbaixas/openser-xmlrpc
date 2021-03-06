/*
 * $Id: bdb_val.h 3036 2007-11-07 00:44:14Z will_quan $
 *
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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
 * 2007-09-19  genesis (wiquan)
 */


#ifndef _BDB_VAL_H_
#define _BDB_VAL_H_

#include "../../db/db_op.h"
#include "../../db/db_res.h"
#include "../../db/db_con.h"

int bdb_val2str(db_val_t* _v, char* _s, int* _len);
int bdb_str2val(db_type_t _t, db_val_t* _v, char* _s, int _l);

#endif

