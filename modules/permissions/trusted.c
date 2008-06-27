/*
 * $Id: trusted.c 3342 2007-12-12 20:34:19Z bogdan_iancu $
 *
 * allow_trusted related functions
 *
 * Copyright (C) 2003 Juha Heinanen
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
 *  2004-06-07  updated to the new DB api, moved reload_trusted_table (andrei)
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include "permissions.h"
#include "hash.h"
#include "../../config.h"
#include "../../db/db.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"

#define TABLE_VERSION 3

struct trusted_list ***hash_table;     /* Pointer to current hash table pointer */
struct trusted_list **hash_table_1;   /* Pointer to hash table 1 */
struct trusted_list **hash_table_2;   /* Pointer to hash table 2 */


static db_con_t* db_handle = 0;
static db_func_t perm_dbf;


/*
 * Reload trusted table to new hash table and when done, make new hash table
 * current one.
 */
int reload_trusted_table(void)
{
	db_key_t cols[4];
	db_res_t* res = NULL;
	db_row_t* row;
	db_val_t* val;

	struct trusted_list **new_hash_table;
	int i;

	char *pattern, *tag;

	cols[0] = source_col;
	cols[1] = proto_col;
	cols[2] = from_col;
	cols[3] = tag_col;

	if (perm_dbf.use_table(db_handle, trusted_table) < 0) {
		LM_ERR("failed to use trusted table\n");
		return -1;
	}

	if (perm_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 4, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*hash_table == hash_table_1) {
		empty_hash_table(hash_table_2);
		new_hash_table = hash_table_2;
	} else {
		empty_hash_table(hash_table_1);
		new_hash_table = hash_table_1;
	}

	row = RES_ROWS(res);

	LM_DBG("number of rows in trusted table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
	    val = ROW_VALUES(row + i);
	    if ((ROW_N(row + i) == 4) &&
		(VAL_TYPE(val) == DB_STRING) && !VAL_NULL(val) &&
		(VAL_TYPE(val + 1) == DB_STRING) && !VAL_NULL(val + 1) &&
		(VAL_NULL(val + 2) ||
		 ((VAL_TYPE(val + 2) == DB_STRING) && !VAL_NULL(val + 2))) &&
		(VAL_NULL(val + 3) ||
		 ((VAL_TYPE(val + 3) == DB_STRING) && !VAL_NULL(val + 3)))) {
		if (VAL_NULL(val + 2)) {
		    pattern = 0;
		} else {
		    pattern = (char *)VAL_STRING(val + 2);
		}
		if (VAL_NULL(val + 3)) {
		    tag = 0;
		} else {
		    tag = (char *)VAL_STRING(val + 3);
		}
		if (hash_table_insert(new_hash_table,
				      (char *)VAL_STRING(val),
				      (char *)VAL_STRING(val + 1),
				      pattern, tag) == -1) {
		    LM_ERR("hash table problem\n");
		    perm_dbf.free_result(db_handle, res);
		    return -1;
		}
		LM_DBG("tuple <%s, %s, %s, %s> inserted into trusted hash "
		    "table\n", VAL_STRING(val), VAL_STRING(val + 1),
		    pattern, tag);
	    } else {
		LM_ERR("database problem\n");
		perm_dbf.free_result(db_handle, res);
		return -1;
	    }
	}

	perm_dbf.free_result(db_handle, res);

	*hash_table = new_hash_table;

	LM_DBG("trusted table reloaded successfully.\n");
	
	return 1;
}


/*
 * Initialize data structures
 */
int init_trusted(void)
{
	int ver;
	str name;
	/* Check if hash table needs to be loaded from trusted table */

	if (!db_url) {
		LM_INFO("db_url parameter of permissions module not set, "
			"disabling allow_trusted\n");
		return 0;
	} else {
		if (bind_dbmod(db_url, &perm_dbf) < 0) {
			LM_ERR("load a database support module\n");
			return -1;
		}

		if (!DB_CAPABILITY(perm_dbf, DB_CAP_QUERY)) {
			LM_ERR("database module does not implement 'query' function\n");
			return -1;
		}
	}

	hash_table_1 = hash_table_2 = 0;
	hash_table = 0;

	if (db_mode == ENABLE_CACHE) {
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}

		name.s = trusted_table;
		name.len = strlen(trusted_table);
		ver = table_version(&perm_dbf, db_handle, &name);

		if (ver < 0) {
			LM_ERR("failed to query table version\n");
			perm_dbf.close(db_handle);
			return -1;
		} else if (ver < TABLE_VERSION) {
			LM_ERR("invalid table version %d - expected %d "
					"(use openser_mysql.sh reinstall)\n", ver,TABLE_VERSION);
			perm_dbf.close(db_handle);
			return -1;
		}

		hash_table_1 = new_hash_table();
		if (!hash_table_1) return -1;
		
		hash_table_2  = new_hash_table();
		if (!hash_table_2) goto error;
		
		hash_table = (struct trusted_list ***)shm_malloc
			(sizeof(struct trusted_list **));
		if (!hash_table) goto error;

		*hash_table = hash_table_1;

		if (reload_trusted_table() == -1) {
			LM_CRIT("reload of trusted table failed\n");
			goto error;
		}

		perm_dbf.close(db_handle);
		db_handle = 0;
	}
	return 0;

error:
	if (hash_table_1) {
		free_hash_table(hash_table_1);
		hash_table_1 = 0;
	}
	if (hash_table_2) {
		free_hash_table(hash_table_2);
		hash_table_2 = 0;
	}
	if (hash_table) {
		shm_free(hash_table);
		hash_table = 0;
	}
	perm_dbf.close(db_handle);
	db_handle = 0;
	return -1;
}


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank)
{
	str name;
	int ver;

	if (!db_url) {
		return 0;
	}
	
	/* Check if database is needed by child */
	if (db_mode==DISABLE_CACHE && rank>0) {
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}

		name.s = trusted_table;
		name.len = strlen(trusted_table);
		ver = table_version(&perm_dbf, db_handle, &name);

		if (ver < 0) {
			LM_ERR("failed to query table version\n");
			perm_dbf.close(db_handle);
			return -1;
		} else if (ver < TABLE_VERSION) {
			LM_ERR("invalid table version (use openser_mysql.sh reinstall)\n");
			perm_dbf.close(db_handle);
			return -1;
		}		

	}

	return 0;
}


/*
 * Open database connection if necessary
 */
int mi_init_trusted(void)
{
    if (!db_url || db_handle) return 0;
    db_handle = perm_dbf.init(db_url);
    if (!db_handle) {
	LM_ERR("unable to connect database\n");
	return -1;
    }
    return 0;
}


/*
 * Close connections and release memory
 */
void clean_trusted(void)
{
	if (hash_table_1) free_hash_table(hash_table_1);
	if (hash_table_2) free_hash_table(hash_table_2);
	if (hash_table) shm_free(hash_table);
}


/*
 * Matches protocol string against the protocol of the request.  Returns 1 on
 * success and 0 on failure.
 */
static inline int match_proto(const char *proto_string, int proto_int)
{
	if (strcasecmp(proto_string, "any") == 0) return 1;
	
	if (proto_int == PROTO_UDP) {
		if (strcasecmp(proto_string, "udp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_TCP) {
		if (strcasecmp(proto_string, "tcp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_TLS) {
		if (strcasecmp(proto_string, "tls") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_SCTP) {
		if (strcasecmp(proto_string, "sctp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}

	LM_ERR("unknown request protocol\n");

	return 0;
}

/*
 * Matches from uri against patterns returned from database.  Returns 1 when
 * first pattern matches and 0 if none of the patterns match.
 */
static int match_res(struct sip_msg* msg, db_res_t* _r)
{
        int i, tag_avp_type;
	str uri;
	char uri_string[MAX_URI_SIZE+1];
	db_row_t* row;
	db_val_t* val;
	regex_t preg;
	int_str tag_avp, avp_val;

	if (parse_from_header(msg) < 0) return -1;
	uri = get_from(msg)->uri;
	if (uri.len > MAX_URI_SIZE) {
		LM_ERR("message has From URI too large\n");
		return -1;
	}
	memcpy(uri_string, uri.s, uri.len);
	uri_string[uri.len] = (char)0;

	row = RES_ROWS(_r);
		
	for(i = 0; i < RES_ROW_N(_r); i++) {
	    val = ROW_VALUES(row + i);
	    if ((ROW_N(row + i) == 3) &&
		(VAL_TYPE(val) == DB_STRING) && !VAL_NULL(val) &&
		match_proto(VAL_STRING(val), msg->rcv.proto) &&
		(VAL_NULL(val + 1) ||
		 ((VAL_TYPE(val + 1) == DB_STRING) && !VAL_NULL(val + 1))) &&
		(VAL_NULL(val + 2) ||
		 ((VAL_TYPE(val + 2) == DB_STRING) && !VAL_NULL(val + 2))))
	    {
		if (VAL_NULL(val + 1)) goto found;
		if (regcomp(&preg, (char *)VAL_STRING(val + 1), REG_NOSUB)) {
		    LM_ERR("invalid regular expression\n");
		    continue;
		}
		if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
		    regfree(&preg);
		    continue;
		} else {
		    regfree(&preg);
		    goto found;
		}
	    }
	}
	return -1;

found:
	get_tag_avp(&tag_avp, &tag_avp_type);
	if (tag_avp.n && !VAL_NULL(val + 2)) {
	    avp_val.s.s = (char *)VAL_STRING(val + 2);
	    avp_val.s.len = strlen(avp_val.s.s);
	    if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, avp_val) != 0) {
		LM_ERR("failed to set of tag_avp failed\n");
		return -1;
	    }
	}
	return 1;
}


/*
 * Checks based on request's source address, protocol, and from field
 * if request can be trusted without authentication.  Possible protocol
 * values are "any" (that matches any protocol), "tcp", "udp", "tls",
 * and "sctp".
 */
int allow_trusted(struct sip_msg* _msg, char* str1, char* str2) 
{
	int result;
	db_res_t* res = NULL;
	
	db_key_t keys[1];
	db_val_t vals[1];
	db_key_t cols[3];

	if (!db_url) {
		LM_ERR("set db_mode parameter of permissions module first !\n");
		return -1;
	}

	if (db_mode == DISABLE_CACHE) {
		keys[0] = source_col;
		cols[0] = proto_col;
		cols[1] = from_col;
		cols[2] = tag_col;

		if (perm_dbf.use_table(db_handle, trusted_table) < 0) {
			LM_ERR("failed to use trusted table\n");
			return -1;
		}
		
		VAL_TYPE(vals) = DB_STRING;
		VAL_NULL(vals) = 0;
		VAL_STRING(vals) = ip_addr2a(&(_msg->rcv.src_ip));

		if (perm_dbf.query(db_handle, keys, 0, vals, cols, 1, 3, 0, &res) < 0){
			LM_ERR("failed to query database\n");
			perm_dbf.close(db_handle);
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			perm_dbf.free_result(db_handle, res);
			return -1;
		}
		
		result = match_res(_msg, res);
		perm_dbf.free_result(db_handle, res);
		return result;
	} else if (db_mode == ENABLE_CACHE) {
		return match_hash_table(*hash_table, _msg);
	} else {
		LM_ERR("set db_mode parameter of permissions module properly\n");
		return -1;
	}
}
