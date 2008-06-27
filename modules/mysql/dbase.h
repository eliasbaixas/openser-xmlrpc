/*
 * $Id: dbase.h 2477 2007-07-18 16:28:47Z henningw $
 *
 * MySQL module core functions
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


#ifndef DBASE_H
#define DBASE_H


#include "../../db/db_con.h"
#include "../../db/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"


/*
 * Initialize database connection
 */
db_con_t* db_mysql_init(const char* _sqlurl);


/*
 * Close a database connection
 */
void db_mysql_close(db_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
int db_mysql_free_result(db_con_t* _h, db_res_t* _r);


/*
 * Do a query
 */
int db_mysql_query(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r);


/*
 * fetch rows from a result
 */
int db_mysql_fetch_result(db_con_t* _h, db_res_t** _r, int nrows);

/*
 * Raw SQL query
 */
int db_mysql_raw_query(db_con_t* _h, char* _s, db_res_t** _r);


/*
 * Insert a row into table
 */
int db_mysql_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from table
 */
int db_mysql_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int db_mysql_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);


/*
 * Just like insert, but replace the row if it exists
 */
int db_mysql_replace(db_con_t* handle, db_key_t* keys, db_val_t* vals, int n);

/*
 * Returns the last inserted ID
 */
int db_last_inserted_id(db_con_t* _h);

/*
 * Insert a row into table, update on duplicate key
 */
int db_insert_update(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_mysql_use_table(db_con_t* _h, const char* _t);


#endif /* DBASE_H */
