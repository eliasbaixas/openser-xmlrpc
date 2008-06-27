/* 
 * $Id: val.c 2733 2007-09-10 15:25:12Z henningw $ 
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


#include "../../dprint.h"
#include "../../db/db_ut.h"
#include "val.h"
#include "my_con.h"

#include <string.h>
#include <stdio.h>


/*
 * Convert str to db value, does not copy strings
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s, int _l)
{
	static str dummy_string = {"", 0};
	
	if (!_v) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_s) {
		memset(_v, 0, sizeof(db_val_t));
			/* Initialize the string pointers to a dummy empty
			 * string so that we do not crash when the NULL flag
			 * is set but the module does not check it properly
			 */
		VAL_STRING(_v) = dummy_string.s;
		VAL_STR(_v) = dummy_string;
		VAL_BLOB(_v) = dummy_string;
		VAL_TYPE(_v) = _t;
		VAL_NULL(_v) = 1;
		return 0;
	}
	VAL_NULL(_v) = 0;

	switch(_t) {
	case DB_INT:
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("error while converting integer value from string\n");
			return -2;
		} else {
			VAL_TYPE(_v) = DB_INT;
			return 0;
		}
		break;

	case DB_BITMAP:
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("error while converting bitmap value from string\n");
			return -3;
		} else {
			VAL_TYPE(_v) = DB_BITMAP;
			return 0;
		}
		break;
	
	case DB_DOUBLE:
		if (db_str2double(_s, &VAL_DOUBLE(_v)) < 0) {
			LM_ERR("error while converting double value from string\n");
			return -4;
		} else {
			VAL_TYPE(_v) = DB_DOUBLE;
			return 0;
		}
		break;

	case DB_STRING:
		VAL_STRING(_v) = _s;
		VAL_TYPE(_v) = DB_STRING;
		return 0;

	case DB_STR:
		VAL_STR(_v).s = (char*)_s;
		VAL_STR(_v).len = _l;
		VAL_TYPE(_v) = DB_STR;
		return 0;

	case DB_DATETIME:
		if (db_str2time(_s, &VAL_TIME(_v)) < 0) {
			LM_ERR("error while converting datetime value from string\n");
			return -5;
		} else {
			VAL_TYPE(_v) = DB_DATETIME;
			return 0;
		}
		break;

	case DB_BLOB:
		VAL_BLOB(_v).s = (char*)_s;
		VAL_BLOB(_v).len = _l;
		VAL_TYPE(_v) = DB_BLOB;
		return 0;
	}
	return -6;
}


/*
 * Used when converting result from a query
 */
int val2str(db_con_t* _c, db_val_t* _v, char* _s, int* _len)
{
	int l;
	char* old_s;

	if (!_c || !_v || !_s || !_len || !*_len) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (VAL_NULL(_v)) {
		if (*_len < sizeof("NULL")) {
			LM_ERR("buffer too small\n");
			return -1;
		}
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB_INT:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to int\n");
			return -2;
		} else {
			return 0;
		}
		break;

	case DB_BITMAP:
		if (db_int2str(VAL_BITMAP(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to int\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB_DOUBLE:
		if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to double\n");
			return -4;
		} else {
			return 0;
		}
		break;

	case DB_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -5;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STRING(_v), l);
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB_STR:
		l = VAL_STR(_v).len;
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -6;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STR(_v).s, l);
			*_s++ = '\'';
			*_s = '\0';
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB_DATETIME:
		if (db_time2str(VAL_TIME(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to time_t\n");
			return -7;
		} else {
			return 0;
		}
		break;

	case DB_BLOB:
		l = VAL_BLOB(_v).len;
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -8;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STR(_v).s, l);
			*_s++ = '\'';
			*_s = '\0';
			*_len = _s - old_s;
			return 0;
		}			
		break;

	default:
		LM_DBG("unknown data type\n");
		return -9;
	}
	/*return -8; --not reached*/
}
