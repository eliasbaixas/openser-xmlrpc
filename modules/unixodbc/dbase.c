/*
 * $Id: dbase.c 2943 2007-10-19 15:15:01Z anca_vamanu $
 *
 * UNIXODBC module core functions
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
 *  2006-04-03  fixed invalid handle to extract error (sgupta)
 *  2006-04-04  removed deprecated ODBC functions, closed cursors on error
 *              (sgupta)
 *  2006-05-05  Fixed reconnect code to actually work on connection loss 
 *              (sgupta)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../db/db_pool.h"
#include "../../db/db_ut.h"
#include "../../db/db_res.h"
#include "val.h"
#include "my_con.h"
#include "res.h"
#include "db_mod.h"
#include "dbase.h"

#define SQL_BUF_LEN 65536

static char sql_buf[SQL_BUF_LEN];

/*
 * Reconnect if connection is broken
 */
static int reconnect(db_con_t* _h, const char* _s)
{
	int ret = 0;
	SQLCHAR outstr[1024];
	SQLSMALLINT outstrlen;
	char conn_str[MAX_CONN_STR_LEN];

	LM_ERR("Attempting DB reconnect\n");

	/* Disconnect */
	SQLDisconnect (CON_CONNECTION(_h));

	/* Reconnect */
	if (!build_conn_str(CON_ID(_h), conn_str)) {
		LM_ERR("failed to build connection string\n");
		return ret;
	}

	ret = SQLDriverConnect(CON_CONNECTION(_h), (void *)1,
			(SQLCHAR*)conn_str, SQL_NTS, outstr, sizeof(outstr),
			&outstrlen, SQL_DRIVER_COMPLETE);
	if (!SQL_SUCCEEDED(ret)) {
		LM_ERR("failed to connect\n");
		extract_error("SQLDriverConnect", CON_CONNECTION(_h),
			SQL_HANDLE_DBC, NULL);
		return ret;
	}

	ret = SQLAllocHandle(SQL_HANDLE_STMT, CON_CONNECTION(_h),
			&CON_RESULT(_h));
	if (!SQL_SUCCEEDED(ret)) {
		LM_ERR("Statement allocation error %d\n", (int)(long)CON_CONNECTION(_h));
		extract_error("SQLAllocStmt", CON_CONNECTION(_h), SQL_HANDLE_DBC,NULL);
		return ret;
	}

	return ret;
}

/*
 * Send an SQL query to the server
 */
static int submit_query(db_con_t* _h, const char* _s)
{
	int ret = 0;
	SQLCHAR sqlstate[7];

	/* first do some cleanup if required */
	if(CON_RESULT(_h))
	{
		SQLCloseCursor(CON_RESULT(_h));
		SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
	}

	ret = SQLAllocHandle(SQL_HANDLE_STMT, CON_CONNECTION(_h), &CON_RESULT(_h));
	if (!SQL_SUCCEEDED(ret))
	{
		LM_ERR("statement allocation error %d\n",
				(int)(long)CON_CONNECTION(_h));
		extract_error("SQLAllocStmt", CON_CONNECTION(_h), SQL_HANDLE_DBC,
			(char*)sqlstate);
		
		/* Connection broken */
		if( !strncmp((char*)sqlstate,"08003",5) ||
		!strncmp((char*)sqlstate,"08S01",5) ) {
			ret = reconnect(_h, _s);
			if( !SQL_SUCCEEDED(ret) ) return ret;
		} else {
			return ret;
		}
	}

	ret=SQLExecDirect(CON_RESULT(_h),  (SQLCHAR*)_s, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		SQLCHAR sqlstate[7];
		LM_ERR("rv=%d. Query= %s\n", ret, _s);
		extract_error("SQLExecDirect", CON_RESULT(_h), SQL_HANDLE_STMT,
			(char*)sqlstate);

		/* Connection broken */
		if( !strncmp((char*)sqlstate,"08003",5) ||
		    !strncmp((char*)sqlstate,"08S01",5) 
		    )
		{
			ret = reconnect(_h, _s);
			if( SQL_SUCCEEDED(ret) ) {
				/* Try again */
				ret=SQLExecDirect(CON_RESULT(_h),  (SQLCHAR*)_s, SQL_NTS);
				if (!SQL_SUCCEEDED(ret)) {
					LM_ERR("rv=%d. Query= %s\n",
						ret, _s);
					extract_error("SQLExecDirect", CON_RESULT(_h),
						SQL_HANDLE_STMT, (char*)sqlstate);
					/* Close the cursor */
					SQLCloseCursor(CON_RESULT(_h));
					SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
				}
			}

		}
		else {
			/* Close the cursor */ 
			SQLCloseCursor(CON_RESULT(_h));
			SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
		}
	}

	return ret;
}



/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* db_init(const char* _url)
{
	struct db_id* id;
	struct my_con* con;
	db_con_t* res;

	id = 0;
	res = 0;

	if (!_url)
	{
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	res = pkg_malloc(sizeof(db_con_t) + sizeof(struct my_con*));
	if (!res)
	{
		LM_ERR("no more pkg memory\n");
		return 0;
	}
	memset(res, 0, sizeof(db_con_t) + sizeof(struct my_con*));

	id = new_db_id(_url);
	if (!id)
	{
		LM_ERR("failed to parse URL '%s'\n", _url);
		goto err;
	}

/* Find the connection in the pool */
	con = (struct my_con*)pool_get(id);
	if (!con)
	{
		LM_DBG("Connection '%s' not found in pool\n", _url);
/* Not in the pool yet */
		con = new_connection(id);
		if (!con)
		{
			goto err;
		}
		pool_insert((struct pool_con*)con);
	}
	else
	{
		LM_DBG("Connection '%s' found in pool\n", _url);
	}

	res->tail = (unsigned long)con;
	return res;

	err:
	if (id) free_db_id(id);
	if (res) pkg_free(res);
	return 0;
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_close(db_con_t* _h)
{
	struct pool_con* con;

	if (!_h)
	{
		LM_ERR("invalid parameter value\n");
		return;
	}

	con = (struct pool_con*)_h->tail;
	if (pool_remove(con) != 0)
	{
		free_connection((struct my_con*)con);
	}

	pkg_free(_h);
}

/*
 * Retrieve result set
 */
static int store_result(db_con_t* _h, db_res_t** _r)
{
	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	*_r = db_new_result();

	if (*_r == 0)
	{
		LM_ERR("no memory left\n");
		return -2;
	}

	if (convert_result(_h, *_r) < 0)
	{
		LM_ERR("failed to convert result\n");
		pkg_free(*_r);
		*_r = 0;
		return -4;
	}
	return 0;
}

/*
 * Release a result set from memory
 */
int db_free_result(db_con_t* _h, db_res_t* _r)
{
	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (free_result(_r) < 0)
	{
		LM_ERR("failed to free result structure\n");
		return -1;
	}
	SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
	CON_RESULT(_h) = 0;
	return 0;
}

/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_query(db_con_t* _h, db_key_t* _k, db_op_t* _op,
db_val_t* _v, db_key_t* _c, int _n, int _nc,
db_key_t _o, db_res_t** _r)
{
	int off, ret;

	if (!_h)
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_c)
	{
		ret = snprintf(sql_buf, SQL_BUF_LEN, "select * from %s ", CON_TABLE(_h));
		if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
		off = ret;
	}
	else
	{
		ret = snprintf(sql_buf, SQL_BUF_LEN, "select ");
		if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
		off = ret;

		ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, _c, _nc);
		if (ret < 0) return -1;
		off += ret;

		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, "from %s ", CON_TABLE(_h));
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;
	}
	if (_n)
	{
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, "where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off, SQL_BUF_LEN - off, _k, _op, _v, _n, val2str);
		if (ret < 0) return -1;;
		off += ret;
	}
	if (_o)
	{
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " order by %s", _o);
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;
	}

	*(sql_buf + off) = '\0';
	if (submit_query(_h, sql_buf) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}

	return store_result(_h, _r);

	error:
	LM_ERR("snprintf failed\n");
	return -1;
}

/*
 * Execute a raw SQL query
 */
int db_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
	if ((!_h) || (!_s))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (submit_query(_h, _s) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}

	if(_r)
		return store_result(_h, _r);
	return 0;
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	int off, ret;

	if ((!_h) || (!_k) || (!_v) || (!_n))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "insert into %s (", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, _k, _n);
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
	off += ret;

	ret = db_print_values(_h, sql_buf + off, SQL_BUF_LEN - off, _v, _n, val2str);
	if (ret < 0) return -1;
	off += ret;

	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if (submit_query(_h, sql_buf) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}
	return 0;

	error:
	LM_ERR("snprintf failed\n");
	return -1;
}

/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	int off, ret;

	if (!_h)
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "delete from %s", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	if (_n)
	{
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off, SQL_BUF_LEN - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;
	}

	*(sql_buf + off) = '\0';
	if (submit_query(_h, sql_buf) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}
	return 0;

	error:
	LM_ERR("snprintf failed\n");
	return -1;
}

/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	int off, ret;

	if ((!_h) || (!_uk) || (!_uv) || (!_un))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "update %s set ", CON_TABLE(_h));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_set(_h, sql_buf + off, SQL_BUF_LEN - off, _uk, _uv, _un, val2str);
	if (ret < 0) return -1;
	off += ret;

	if (_n)
	{
		ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, " where ");
		if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
		off += ret;

		ret = db_print_where(_h, sql_buf + off,
				SQL_BUF_LEN - off, _k, _o, _v, _n, val2str);
		if (ret < 0) return -1;
		off += ret;
	}
	*(sql_buf + off) = '\0';

	if (submit_query(_h, sql_buf) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}
	return 0;

	error:
	LM_ERR("snprintf failed\n");
	return -1;
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_replace(db_con_t* handle, db_key_t* keys, db_val_t* vals, int n)
{
	int off, ret;

	if (!handle || !keys || !vals)
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(sql_buf, SQL_BUF_LEN, "replace %s (", CON_TABLE(handle));
	if (ret < 0 || ret >= SQL_BUF_LEN) goto error;
	off = ret;

	ret = db_print_columns(sql_buf + off, SQL_BUF_LEN - off, keys, n);
	if (ret < 0) return -1;
	off += ret;

	ret = snprintf(sql_buf + off, SQL_BUF_LEN - off, ") values (");
	if (ret < 0 || ret >= (SQL_BUF_LEN - off)) goto error;
	off += ret;

	ret = db_print_values(handle, sql_buf + off, SQL_BUF_LEN - off, vals, n, val2str);
	if (ret < 0) return -1;
	off += ret;

	*(sql_buf + off++) = ')';
	*(sql_buf + off) = '\0';

	if (submit_query(handle, sql_buf) < 0)
	{
		LM_ERR("submitting query failed\n");
		return -2;
	}
	return 0;

	error:
	LM_ERR("snprintf failed\n");
	return -1;
}
