/*
 * $Id: bdb_res.c 3036 2007-11-07 00:44:14Z will_quan $
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "../../mem/mem.h"
#include "bdb_res.h"


/**
* 
*/
int bdb_get_columns(table_p _tp, db_res_t* _res, int* _lres, int _nc)
{
	int col, len;

	if (!_res) 
	{
		LM_ERR("db_res_t parameter cannot be NULL\n");
		return -1;
	}

	if (_nc < 0 ) 
	{
		LM_ERR("_nc parameter cannot be negative \n");
		return -1;
	}

        /* the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(_res) = 1;

	if (!_lres)
		_nc = _tp->ncols;

	/* Allocate storage to hold a pointer to each column name */
	RES_NAMES(_res) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * _nc);

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("%p=pkg_malloc(%lu) RES_NAMES\n", RES_NAMES(_res)
		, (unsigned long)(sizeof(db_key_t) * _nc));
#endif

	if (!RES_NAMES(_res)) 
	{
		LM_ERR("Failed to allocate %lu bytes for column names\n"
			, (unsigned long)(sizeof(db_key_t) * _nc));
		
		return -3;
	}

	/* Allocate storage to hold the type of each column */
	RES_TYPES(_res) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * _nc);

#ifdef BDB_EXTRA_DEBUG
	LM_DBG("%p=pkg_malloc(%lu) RES_TYPES\n", RES_TYPES(_res)
		, (unsigned long)(sizeof(db_type_t) * _nc));
#endif

	if (!RES_TYPES(_res)) 
	{
		LM_ERR("Failed to allocate %lu bytes for column types\n"
		, (unsigned long)(sizeof(db_type_t) * _nc));
		
		/* Free previously allocated storage that was to hold column names */
		LM_DBG("%p=pkg_free() RES_NAMES\n", RES_NAMES(_res));
		pkg_free(RES_NAMES(_res));
		return -4;
	}

	/* Save number of columns in the result structure */
	RES_COL_N(_res) = _nc;

	/* 
	 * For each column both the name and the data type are saved.
	 */
	for(col = 0; col < _nc; col++) 
	{
		column_p cp = NULL;
		cp = (_lres) ? _tp->colp[_lres[col]] : _tp->colp[col];
		len = cp->name.len;
		RES_NAMES(_res)[col] = pkg_malloc(len+1);
		
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("%p=pkg_malloc(%d) RES_NAMES[%d]\n"
			, RES_NAMES(_res)[col], len+1, col);
#endif

		if (! RES_NAMES(_res)[col]) 
		{
			LM_ERR("Failed to allocate %d bytes to hold column name\n", len+1);
			return -1;
		}
		
		memset((char *)RES_NAMES(_res)[col], 0, len+1);
		strncpy((char *)RES_NAMES(_res)[col], cp->name.s, len); 

		LM_DBG("RES_NAMES(%p)[%d]=[%s]\n", RES_NAMES(_res)[col]
			, col, RES_NAMES(_res)[col]);

		RES_TYPES(_res)[col] = cp->type;
	}
	return 0;
}



/**
 * Convert rows from Berkeley DB to db API representation
 */
int bdb_convert_row(db_res_t* _res, char *bdb_result, int* _lres)
{
        int col, len, i, j;
	char **row_buf, *s;
	db_row_t* row = NULL;
	col = len = i = j = 0;
	
	if (!_res)
	{
		LM_ERR("bdb_convert_row: db_res_t parameter cannot be NULL\n");
		return -1;
	}

	/* Allocate a single row structure */
	len = sizeof(db_row_t); 
	row = (db_row_t*)pkg_malloc(len);
	if (!row)
	{
		LM_ERR("Failed to allocate %d bytes for row structure\n", len);
		return -1;
	}
	memset(row, 0, len);
	RES_ROWS(_res) = row;
	
	/* Save the number of rows in the current fetch */
	RES_ROW_N(_res) = 1;

	/* Allocate storage to hold the bdb result values */
	len = sizeof(db_val_t) * RES_COL_N(_res);
	ROW_VALUES(row) = (db_val_t*)pkg_malloc(len);
    LM_DBG("%p=pkg_malloc(%d) ROW_VALUES for %d columns\n"
		 , ROW_VALUES(row)
		 , len
		 , RES_COL_N(_res));

	if (!ROW_VALUES(row)) 
	{	
		LM_ERR("bdb_convert_row: No memory left\n");
		return -1;
	}
	memset(ROW_VALUES(row), 0, len);

	/* Save the number of columns in the ROW structure */
	ROW_N(row) = RES_COL_N(_res);

	/*
	 * Allocate an array of pointers one per column.
	 * It that will be used to hold the address of the string representation of each column.
	 */
	len = sizeof(char *) * RES_COL_N(_res);
	row_buf = (char **)pkg_malloc(len);
	if (!row_buf)
	{
		LM_ERR("Failed to allocate %d bytes for row buffer\n", len);
		return -1;
	}
	memset(row_buf, 0, len);

	/*populate the row_buf with bdb_result*/
	/*bdb_result is memory from our callers stack so we copy here*/
	s = strtok(bdb_result, DELIM);
	while( s!=NULL)
	{

		if(_lres)
		{	
			/*only requested cols (_c was specified)*/
			for(i=0; i<ROW_N(row); i++)
			{	if (col == _lres[i])
				{
					len = strlen(s);
					row_buf[i] = pkg_malloc(len+1);
					if (!row_buf[i])
					{
						LM_ERR("Failed to allocate %d bytes for row_buf[%d]\n", len+1, col);
						return -1;
					}
					memset(row_buf[i], 0, len+1);
					strncpy(row_buf[i], s, len);
				}
				
			}
		}
		else 
		{
			len = strlen(s);
			row_buf[col] = pkg_malloc(len+1);
			if (!row_buf[col]) {
				LM_ERR("Failed to allocate %d bytes for row_buf[%d]\n", len+1, col);
				return -1;
			}
			memset(row_buf[col], 0, len+1);
			strncpy(row_buf[col], s, len);
		}

		s = strtok(NULL, DELIM);
		col++;
	}

	/*do the type conversion per col*/
        for(col = 0; col < ROW_N(row); col++) 
	{
		/*skip the unrequested cols (as already specified)*/
		if(!row_buf[col])  continue;

		LM_DBG("col[%d]\n", col);
		/* Convert the string representation into the value representation */
		if (bdb_str2val(	RES_TYPES(_res)[col]
				, &(ROW_VALUES(row)[col])
				, row_buf[col]
				, strlen(row_buf[col])) < 0) 
		{
			LM_ERR("Error while converting value\n");
			LM_DBG("%p=pkg_free() _row\n", row);
			bdb_free_row(row);
			return -3;
		}
	}

	/* pkg_free() must be done for the above allocations now that the row has been converted.
	 * During bdb_convert_row (and subsequent bdb_str2val) processing, data types that don't need to be
	 * converted (namely STRINGS) have their addresses saved.  These data types should not have
	 * their pkg_malloc() allocations freed here because they are still needed.  However, some data types
	 * (ex: INT, DOUBLE) should have their pkg_malloc() allocations freed because during the conversion
	 * process, their converted values are saved in the union portion of the db_val_t structure.
	 *
	 * Warning: when the converted row is no longer needed, the data types whose addresses
	 * were saved in the db_val_t structure must be freed or a memory leak will happen.
	 * This processing should happen in the bdb_free_row() subroutine.  The caller of
	 * this routine should ensure that bdb_free_rows(), bdb_free_row() or bdb_free_result()
	 * is eventually called.
	 */
	for (col=0; col<RES_COL_N(_res); col++) 
	{
		switch (RES_TYPES(_res)[col]) 
		{
			case DB_STRING:
			case DB_STR:
				break;
			default:
#ifdef BDB_EXTRA_DEBUG
			LM_DBG("col[%d] Col[%s] Type[%d] Freeing row_buf[%p]\n", col
				, RES_NAMES(_res)[col], RES_TYPES(_res)[col]
				, (char*) row_buf[col]);
			
			LM_DBG("%p=pkg_free() row_buf[%d]\n", (char *)row_buf[col], col);
#endif

			pkg_free((char *)row_buf[col]);
		}
		/* The following housekeeping may not be technically required, but it is a good practice
		 * to NULL pointer fields that are no longer valid.  Note that DB_STRING fields have not
		 * been pkg_free(). NULLing DB_STRING fields would normally not be good to do because a memory
		 * leak would occur.  However, the pg_convert_row() routine has saved the DB_STRING pointer
		 * in the db_val_t structure.  The db_val_t structure will eventually be used to pkg_free()
		 * the DB_STRING storage.
		 */
		row_buf[col] = (char *)NULL;
	}

	LM_DBG("%p=pkg_free() row_buf\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;

	return 0;

}

/*rx is row index*/
int bdb_append_row(db_res_t* _res, char *bdb_result, int* _lres, int _rx)
{
	int col, len, i, j;
	char **row_buf, *s;
	db_row_t* row = NULL;
	col = len = i = j = 0;
	
	if (!_res)
	{
		LM_ERR("db_res_t parameter cannot be NULL\n");
		return -1;
	}
	
	row = &(RES_ROWS(_res)[_rx]);
	
	/* Allocate storage to hold the bdb result values */
	len = sizeof(db_val_t) * RES_COL_N(_res);
	ROW_VALUES(row) = (db_val_t*)pkg_malloc(len);
	
	if (!ROW_VALUES(row)) 
	{
		LM_ERR("No private memory left\n");
		return -1;
	}
	
	memset(ROW_VALUES(row), 0, len);
	
	/* Save the number of columns in the ROW structure */
	ROW_N(row) = RES_COL_N(_res);
	
	/*
	 * Allocate an array of pointers one per column.
	 * It that will be used to hold the address of the string representation of each column.
	 */
	len = sizeof(char *) * RES_COL_N(_res);
	row_buf = (char **)pkg_malloc(len);
	if (!row_buf) 
	{
		LM_ERR("Failed to allocate %d bytes for row buffer\n", len);
		return -1;
	}
	memset(row_buf, 0, len);
	
	/*populate the row_buf with bdb_result*/
	/*bdb_result is memory from our callers stack so we copy here*/
	s = strtok(bdb_result, DELIM);
	while( s!=NULL)
	{
		
		if(_lres)
		{	
			/*only requested cols (_c was specified)*/
			for(i=0; i<ROW_N(row); i++)
			{	if (col == _lres[i])
				{
					len = strlen(s);
					row_buf[i] = pkg_malloc(len+1);
					if (!row_buf[i])
					{
						LM_ERR("Failed to allocate %d bytes for row_buf[%d]\n", len+1, col);
						return -1;
					}
					memset(row_buf[i], 0, len+1);
					strncpy(row_buf[i], s, len);
				}
				
			}
		}
		else 
		{
			len = strlen(s);

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("col[%i] = [%.*s]\n", col , len, s );
#endif

			row_buf[col] = (char*)pkg_malloc(len+1);
			if (!row_buf[col]) 
			{
				LM_ERR("Failed to allocate %d bytes for row_buf[%d]\n", len+1, col);
				return -1;
			}
			memset(row_buf[col], 0, len+1);
			strncpy(row_buf[col], s, len);
		}
		
		s = strtok(NULL, DELIM);
		col++;
	}
	
	/*do the type conversion per col*/
	for(col = 0; col < ROW_N(row); col++) 
	{
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("tc 1: col[%i] == ", col );
#endif

		/*skip the unrequested cols (as already specified)*/
		if(!row_buf[col])  continue;

#ifdef BDB_EXTRA_DEBUG
		LM_DBG("tc 2: col[%i] \n", col );
#endif

		/* Convert the string representation into the value representation */
		if (bdb_str2val(	RES_TYPES(_res)[col]
				, &(ROW_VALUES(row)[col])
				, row_buf[col]
				, strlen(row_buf[col])) < 0) 
		{
			LM_ERR("Error while converting value\n");
			LM_DBG("%p=pkg_free() _row\n", row);
			bdb_free_row(row);
			return -3;
		}
		
		LM_DBG("col[%d] : %s\n", col, row_buf[col] );
	}

	/* pkg_free() must be done for the above allocations now that the row has been converted.
	 * During bdb_convert_row (and subsequent bdb_str2val) processing, data types that don't need to be
	 * converted (namely STRINGS) have their addresses saved.  These data types should not have
	 * their pkg_malloc() allocations freed here because they are still needed.  However, some data types
	 * (ex: INT, DOUBLE) should have their pkg_malloc() allocations freed because during the conversion
	 * process, their converted values are saved in the union portion of the db_val_t structure.
	 *
	 * Warning: when the converted row is no longer needed, the data types whose addresses
	 * were saved in the db_val_t structure must be freed or a memory leak will happen.
	 * This processing should happen in the bdb_free_row() subroutine.  The caller of
	 * this routine should ensure that bdb_free_rows(), bdb_free_row() or bdb_free_result()
	 * is eventually called.
	 */
	for (col=0; col<RES_COL_N(_res); col++) 
	{
		if (RES_TYPES(_res)[col] != DB_STRING) 
		{
#ifdef BDB_EXTRA_DEBUG
			LM_DBG("[%d][%d] Col[%s] Type[%d] Freeing row_buf[%i]\n"
				, _rx, col, RES_NAMES(_res)[col], RES_TYPES(_res)[col], col);
#endif
			pkg_free((char *)row_buf[col]);
		}
		/* The following housekeeping may not be technically required, but it is a good practice
		 * to NULL pointer fields that are no longer valid.  Note that DB_STRING fields have not
		 * been pkg_free(). NULLing DB_STRING fields would normally not be good to do because a memory
		 * leak would occur.  However, the pg_convert_row() routine has saved the DB_STRING pointer
		 * in the db_val_t structure.  The db_val_t structure will eventually be used to pkg_free()
		 * the DB_STRING storage.
		 */
		row_buf[col] = (char *)NULL;
	}

	LM_DBG("%p=pkg_free() row_buf\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;
	return 0;
}



int* bdb_get_colmap(table_p _dtp, db_key_t* _k, int _n)
{
	int i, j, *_lref=NULL;
	
	if(!_dtp || !_k || _n < 0)
		return NULL;
	
	_lref = (int*)pkg_malloc(_n*sizeof(int));
	if(!_lref)
		return NULL;
	
	for(i=0; i < _n; i++)
	{
		for(j=0; j<_dtp->ncols; j++)
		{
			if(strlen(_k[i])==_dtp->colp[j]->name.len
			&& !strncasecmp(_k[i], _dtp->colp[j]->name.s,
						_dtp->colp[j]->name.len))
			{
				_lref[i] = j;
				break;
			}
		}
		
		if(i>=_dtp->ncols)
		{
			LM_DBG("ERROR column <%s> not found\n", _k[i]);
			pkg_free(_lref);
			return NULL;
		}
		
	}
	return _lref;
}


int bdb_free_result(db_res_t* _res)
{
	bdb_free_columns(_res);
	bdb_free_rows(_res);
	LM_DBG("%p=pkg_free() _res\n", _res);
	pkg_free(_res);
	_res = NULL;

	return 0;
}

/**
 * Release memory used by rows
 */
int bdb_free_rows(db_res_t* _res)
{
	int row;

	LM_DBG("Freeing %d rows\n", RES_ROW_N(_res));

	for(row = 0; row < RES_ROW_N(_res); row++) 
	{
		LM_DBG("Row[%d]=%p\n", row, &(RES_ROWS(_res)[row]));
		bdb_free_row(&(RES_ROWS(_res)[row]));
	}

	RES_ROW_N(_res) = 0;

	if (RES_ROWS(_res)) 
	{
		LM_DBG("%p=pkg_free() RES_ROWS\n", RES_ROWS(_res));
		pkg_free(RES_ROWS(_res));
		RES_ROWS(_res) = NULL;
	}

        return 0;
}

int bdb_free_row(db_row_t* _row)
{
	int	col;
	db_val_t* _val;

	/* 
	 * Loop thru each columm, then check to determine if the storage 
	 * pointed to by db_val_t structure must be freed.
	 * This is required for DB_STRING.  If this is not done, 
	 * a memory leak will happen.
	 * DB_STR types also fall in this category, however, they are 
	 * currently not being converted (or checked below).
	 */
	for (col = 0; col < ROW_N(_row); col++) 
	{
		_val = &(ROW_VALUES(_row)[col]);
		switch (VAL_TYPE(_val)) 
		{
		case DB_STRING:
			LM_DBG("%p=pkg_free() VAL_STRING[%d]\n", (char *)VAL_STRING(_val), col);
			pkg_free((char *)(VAL_STRING(_val)));
			VAL_STRING(_val) = (char *)NULL;
			break;

		case DB_STR:
			LM_DBG("%p=pkg_free() VAL_STR[%d]\n", (char *)(VAL_STR(_val).s), col);
			pkg_free((char *)(VAL_STR(_val).s));
			VAL_STR(_val).s = (char *)NULL;
			break;
		default:
			break;
		}
	}

	/* Free db_val_t structure. */
	if (ROW_VALUES(_row)) 
	{
		LM_DBG("%p=pkg_free() ROW_VALUES\n", ROW_VALUES(_row));
		pkg_free(ROW_VALUES(_row));
		ROW_VALUES(_row) = NULL;
	}
	return 0;
}

/**
 * Release memory used by columns
 */
int bdb_free_columns(db_res_t* _res)
{
	int col;

	/* Free memory previously allocated to save column names */
	for(col = 0; col < RES_COL_N(_res); col++) 
	{
#ifdef BDB_EXTRA_DEBUG
		LM_DBG("Freeing RES_NAMES(%p)[%d] -> free(%p) '%s'\n", _res
			, col, RES_NAMES(_res)[col], RES_NAMES(_res)[col]);
		LM_DBG("%p=pkg_free() RES_NAMES[%d]\n", RES_NAMES(_res)[col], col);
#endif
		
		pkg_free((char *)RES_NAMES(_res)[col]);
		RES_NAMES(_res)[col] = (char *)NULL;
	}
	
	if (RES_NAMES(_res)) 
	{
		LM_DBG("%p=pkg_free() RES_NAMES\n", RES_NAMES(_res));
		
		pkg_free(RES_NAMES(_res));
		RES_NAMES(_res) = NULL;
	}
	
	if (RES_TYPES(_res)) 
	{
		LM_DBG("%p=pkg_free() RES_TYPES\n", RES_TYPES(_res));
		
		pkg_free(RES_TYPES(_res));
		RES_TYPES(_res) = NULL;
	}

	return 0;
}

int bdb_is_neq_type(db_type_t _t0, db_type_t _t1)
{
	if(_t0 == _t1)	return 0;
	
	switch(_t1)
	{
		case DB_INT:
			if(_t0==DB_DATETIME || _t0==DB_BITMAP)
				return 0;
		case DB_DATETIME:
			if(_t0==DB_INT)
				return 0;
			if(_t0==DB_BITMAP)
				return 0;
		case DB_DOUBLE:
			break;
		case DB_STRING:
			if(_t0==DB_STR)
				return 0;
		case DB_STR:
			if(_t0==DB_STRING || _t0==DB_BLOB)
				return 0;
		case DB_BLOB:
			if(_t0==DB_STR)
				return 0;
		case DB_BITMAP:
			if (_t0==DB_INT)
				return 0;
	}
	return 1;
}


/*
*/
int bdb_row_match(db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n, db_res_t* _r, int* _lkey )
{
	int i, res;
	db_row_t* row = NULL;
	
	if(!_r || !_lkey)
		return 1;
	
	row = RES_ROWS(_r);
	
	for(i=0; i<_n; i++)
	{
		res = bdb_cmp_val(&(ROW_VALUES(row)[_lkey[i]]), &_v[i]);

		if(!_op || !strcmp(_op[i], OP_EQ))
		{
			if(res!=0)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_LT))
		{
			if(res!=-1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_GT))
		{
			if(res!=1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_LEQ))
		{
			if(res==1)
				return 0;
		}else{
		if(!strcmp(_op[i], OP_GEQ))
		{
			if(res==-1)
				return 0;
		}else{
			return res;
		}}}}}
	}
	
	return 1;
}

/*
*/
int bdb_cmp_val(db_val_t* _vp, db_val_t* _v)
{
	int _l, _n;
	
	if(!_vp && !_v)
		return 0;
	if(!_v)
		return 1;
	if(!_vp)
		return -1;
	if(_vp->nul && _v->nul)
		return 0;
	if(_v->nul)
		return 1;
	if(_vp->nul)
		return -1;
	
	switch(VAL_TYPE(_v))
	{
		case DB_INT:
			return (_vp->val.int_val<_v->val.int_val)?-1:
					(_vp->val.int_val>_v->val.int_val)?1:0;
		case DB_DOUBLE:
			return (_vp->val.double_val<_v->val.double_val)?-1:
					(_vp->val.double_val>_v->val.double_val)?1:0;
		case DB_DATETIME:
			return (_vp->val.int_val<_v->val.time_val)?-1:
					(_vp->val.int_val>_v->val.time_val)?1:0;
		case DB_STRING:
			_l = strlen(_v->val.string_val);
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.string_val, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == strlen(_v->val.string_val))
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB_STR:
			_l = _v->val.str_val.len;
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.str_val.s, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == _v->val.str_val.len)
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB_BLOB:
			_l = _v->val.blob_val.len;
			_l = (_l>_vp->val.str_val.len)?_vp->val.str_val.len:_l;
			_n = strncasecmp(_vp->val.str_val.s, _v->val.blob_val.s, _l);
			if(_n)
				return _n;
			if(_vp->val.str_val.len == _v->val.blob_val.len)
				return 0;
			if(_l==_vp->val.str_val.len)
				return -1;
			return 1;
		case DB_BITMAP:
			return (_vp->val.int_val<_v->val.bitmap_val)?-1:
				(_vp->val.int_val>_v->val.bitmap_val)?1:0;
	}
	return -2;
}
