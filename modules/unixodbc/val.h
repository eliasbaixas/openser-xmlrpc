/* 
 * $Id: val.h 2353 2007-06-11 13:57:32Z henningw $ 
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
 */


#ifndef VAL_H
#define VAL_H

#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "../../db/db_val.h"
#include "../../db/db.h"


/*
 * Does not copy strings
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s, int _l);


/*
 * Used when converting result from a query
 */
int val2str(db_con_t* _c, db_val_t* _v, char* _s, int* _len);


#endif /* VAL_H */
