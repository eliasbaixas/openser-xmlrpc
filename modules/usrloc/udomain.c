/* 
 * $Id: udomain.c 3277 2007-12-06 18:09:44Z bogdan_iancu $ 
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
 *
 * History:
 * ---------
 * 2003-03-11 changed to the new locking scheme: locking.h (andrei)
 * 2003-03-12 added replication mark and zombie state (nils)
 * 2004-06-07 updated to the new DB api (andrei)
 * 2004-08-23  hash function changed to process characters as unsigned
 *             -> no negative results occur (jku)
 *   
 */

#include "udomain.h"
#include <string.h>
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "utime.h"
#include "notify.h"


#ifdef STATISTICS
static char *build_stat_name( str* domain, char *var_name)
{
	int n;
	char *s;
	char *p;

	n = domain->len + 1 + strlen(var_name) + 1;
	s = (char*)shm_malloc( n );
	if (s==0) {
		LM_ERR("no more shm mem\n");
		return 0;
	}
	memcpy( s, domain->s, domain->len);
	p = s + domain->len;
	*(p++) = '-';
	memcpy( p , var_name, strlen(var_name));
	p += strlen(var_name);
	*(p++) = 0;
	return s;
}
#endif


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d)
{
	int i;
#ifdef STATISTICS
	char *name;
#endif
	
	/* Must be always in shared memory, since
	 * the cache is accessed from timer which
	 * lives in a separate process
	 */
	*_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
	if (!(*_d)) {
		LM_ERR("new_udomain(): No memory left\n");
		goto error0;
	}
	memset(*_d, 0, sizeof(udomain_t));
	
	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
	if (!(*_d)->table) {
		LM_ERR("no memory left 2\n");
		goto error1;
	}

	(*_d)->name = _n;
	
	for(i = 0; i < _s; i++) {
		if (init_slot(*_d, &((*_d)->table[i]), i) < 0) {
			LM_ERR("initializing hash table failed\n");
			goto error2;
		}
	}

	(*_d)->size = _s;

#ifdef STATISTICS
	/* register the statistics */
	if ( (name=build_stat_name(_n,"users"))==0 || register_stat("usrloc",
	name, &(*_d)->users, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"contacts"))==0 || register_stat("usrloc",
	name, &(*_d)->contacts, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"expires"))==0 || register_stat("usrloc",
	name, &(*_d)->expires, STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
#endif

	return 0;
error2:
	shm_free((*_d)->table);
error1:
	shm_free(*_d);
error0:
	return -1;
}


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d)
{
	int i;
	
	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
			lock_ulslot(_d, i);
			deinit_slot(_d->table + i);
			unlock_ulslot(_d, i);
		}
		shm_free(_d->table);
	}
	shm_free(_d);
}


/*
 * Returns a statis dummy urecord for temporary usage
 */
static inline void get_static_urecord(udomain_t* _d, str* _aor,
														struct urecord** _r)
{
	static struct urecord r;

	memset( &r, 0, sizeof(struct urecord) );
	r.aor = *_aor;
	r.domain = _d->name;
	*_r = &r;
}


/*
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d)
{
	int i;
	int max=0, slot=0, n=0;
	struct urecord* r;
	fprintf(_f, "---Domain---\n");
	fprintf(_f, "name : '%.*s'\n", _d->name->len, ZSW(_d->name->s));
	fprintf(_f, "size : %d\n", _d->size);
	fprintf(_f, "table: %p\n", _d->table);
	/*fprintf(_f, "lock : %d\n", _d->lock); -- can be a structure --andrei*/
	fprintf(_f, "\n");
	for(i=0; i<_d->size; i++)
	{
		r = _d->table[i].first;
		n += _d->table[i].n;
		if(max<_d->table[i].n){
			max= _d->table[i].n;
			slot = i;
		}
		while(r) {
			print_urecord(_f, r);
			r = r->next;
		}
	}
	fprintf(_f, "\nMax slot: %d (%d/%d)\n", max, slot, n);
	fprintf(_f, "\n---/Domain---\n");
}


/*
 * expects 12 rows (contact, expirs, q, callid, cseq, flags, 
 *   ua, received, path, socket, methods, last_modified)
 */
static inline ucontact_info_t* dbrow2info( db_val_t *vals, str *contact)
{
	static ucontact_info_t ci;
	static str callid, ua, received, host, path;
	int port, proto;
	char *p;

	memset( &ci, 0, sizeof(ucontact_info_t));

	contact->s = (char*)VAL_STRING(vals);
	if (VAL_NULL(vals) || contact->s==0 || contact->s[0]==0) {
		LM_CRIT("bad contact\n");
		return 0;
	}
	contact->len = strlen(contact->s);

	if (VAL_NULL(vals+1)) {
		LM_CRIT("empty expire\n");
		return 0;
	}
	ci.expires = VAL_TIME(vals+1);

	if (VAL_NULL(vals+2)) {
		LM_CRIT("empty q\n");
		return 0;
	}
	ci.q = double2q(VAL_DOUBLE(vals+2));

	if (VAL_NULL(vals+4)) {
		LM_CRIT("empty cseq_nr\n");
		return 0;
	}
	ci.cseq = VAL_INT(vals+4);

	callid.s = (char*)VAL_STRING(vals+3);
	if (VAL_NULL(vals+3) || !callid.s || !callid.s[0]) {
		LM_CRIT("bad callid\n");
		return 0;
	}
	callid.len  = strlen(callid.s);
	ci.callid = &callid;

	if (VAL_NULL(vals+5)) {
		LM_CRIT("empty flag\n");
		return 0;
	}
	ci.flags  = VAL_BITMAP(vals+5);

	if (VAL_NULL(vals+6)) {
		LM_CRIT("empty cflag\n");
		return 0;
	}
	ci.cflags  = VAL_BITMAP(vals+6);

	ua.s  = (char*)VAL_STRING(vals+7);
	if (VAL_NULL(vals+7) || !ua.s || !ua.s[0]) {
		ua.s = 0;
		ua.len = 0;
	} else {
		ua.len = strlen(ua.s);
	}
	ci.user_agent = &ua;

	received.s  = (char*)VAL_STRING(vals+8);
	if (VAL_NULL(vals+8) || !received.s || !received.s[0]) {
		received.len = 0;
		received.s = 0;
	} else {
		received.len = strlen(received.s);
	}
	ci.received = received;
	
	path.s  = (char*)VAL_STRING(vals+9);
		if (VAL_NULL(vals+9) || !path.s || !path.s[0]) {
			path.len = 0;
			path.s = 0;
		} else {
			path.len = strlen(path.s);
		}
	ci.path= &path;

	/* socket name */
	p  = (char*)VAL_STRING(vals+10);
	if (VAL_NULL(vals+10) || p==0 || p[0]==0){
		ci.sock = 0;
	} else {
		if (parse_phostport( p, strlen(p), &host.s, &host.len, 
		&port, &proto)!=0) {
			LM_ERR("bad socket <%s>\n", p);
			return 0;
		}
		ci.sock = grep_sock_info( &host, (unsigned short)port, proto);
		if (ci.sock==0) {
			LM_WARN("non-local socket <%s>...ignoring\n", p);
		}
	}

	/* supported methods */
	if (VAL_NULL(vals+11)) {
		ci.methods = ALL_METHODS;
	} else {
		ci.methods = VAL_BITMAP(vals+11);
	}

	/* last modified time */
	if (!VAL_NULL(vals+12)) {
		ci.last_modified = VAL_TIME(vals+12);
	}

	return &ci;
}


int preload_udomain(db_con_t* _c, udomain_t* _d)
{
	char uri[MAX_URI_SIZE];
	ucontact_info_t *ci;
	db_row_t *row;
	db_key_t columns[15];
	db_res_t* res = NULL;
	str user, contact;
	char* domain;
	int i;
	int n;

	urecord_t* r;
	ucontact_t* c;

	columns[0] = user_col.s;
	columns[1] = contact_col.s;
	columns[2] = expires_col.s;
	columns[3] = q_col.s;
	columns[4] = callid_col.s;
	columns[5] = cseq_col.s;
	columns[6] = flags_col.s;
	columns[7] = cflags_col.s;
	columns[8] = user_agent_col.s;
	columns[9] = received_col.s;
	columns[10] = path_col.s;
	columns[11] = sock_col.s;
	columns[12] = methods_col.s;
	columns[13] = last_mod_col.s;
	columns[14] = domain_col.s;

	if (ul_dbf.use_table(_c, _d->name->s) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

#ifdef EXTRA_DEBUG
	LM_ERR("load start time [%d]\n", (int)time(NULL));
#endif

	if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, (use_domain)?(15):(14), 0,
		0) < 0) {
			LM_ERR("db_query (1) failed\n");
			return -1;
		}
		if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, (use_domain)?(15):(14), 0,
		&res) < 0) {
			LM_ERR("db_query failed\n");
			return -1;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("table is empty\n");
		ul_dbf.free_result(_c, res);
		return 0;
	}


	n = 0;
	do {
		LM_DBG("loading records - cycle [%d]\n", ++n);
		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;

			user.s = (char*)VAL_STRING(ROW_VALUES(row));
			if (VAL_NULL(ROW_VALUES(row)) || user.s==0 || user.s[0]==0) {
				LM_CRIT("empty username record in table %s...skipping\n",
						_d->name->s);
				continue;
			}
			user.len = strlen(user.s);

			ci = dbrow2info( ROW_VALUES(row)+1, &contact);
			if (ci==0) {
				LM_ERR("sipping record for %.*s in table %s\n",
						user.len, user.s, _d->name->s);
				continue;
			}

			if (use_domain) {
				domain = (char*)VAL_STRING(ROW_VALUES(row) + 14);
				if (VAL_NULL(ROW_VALUES(row)+13) || domain==0 || domain[0]==0){
					LM_CRIT("empty domain record for user %.*s...skipping\n",
							user.len, user.s);
					continue;
				}
				/* user.s cannot be NULL - checked previosly */
				user.len = snprintf(uri, MAX_URI_SIZE, "%.*s@%s",
					user.len, user.s, domain);
				user.s = uri;
				if (user.s[user.len]!=0) {
					LM_CRIT("URI '%.*s@%s' longer than %d\n", user.len, user.s,
							domain,	MAX_URI_SIZE);
					continue;
				}
			}

		
			lock_udomain(_d, &user);
			if (get_urecord(_d, &user, &r) > 0) {
				if (mem_insert_urecord(_d, &user, &r) < 0) {
					LM_ERR("failed to create a record\n");
					unlock_udomain(_d, &user);
					goto error;
				}
			}

			if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
				LM_ERR("inserting contact failed\n");
				unlock_udomain(_d, &user);
				goto error1;
			}

			/* We have to do this, because insert_ucontact sets state to CS_NEW
			 * and we have the contact in the database already */
			c->state = CS_SYNC;
			unlock_udomain(_d, &user);
		}

		if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
			if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
				LM_ERR("fetching rows (1) failed\n");
				ul_dbf.free_result(_c, res);
				return -1;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res)>0);

	ul_dbf.free_result(_c, res);

#ifdef EXTRA_DEBUG
	LM_ERR("load end time [%d]\n", (int)time(NULL));
#endif

	return 0;
error1:
	free_ucontact(c);
error:
	ul_dbf.free_result(_c, res);
	return -1;
}


/*
 * loads from DB all contacts for an AOR
 */
urecord_t* db_load_urecord(db_con_t* _c, udomain_t* _d, str *_aor)
{
	ucontact_info_t *ci;
	db_key_t columns[13];
	db_key_t keys[2];
	db_key_t order;
	db_val_t vals[2];
	db_res_t* res = NULL;
	str contact;
	char *domain;
	int i;

	urecord_t* r;
	ucontact_t* c;

	keys[0] = user_col.s;
	vals[0].type = DB_STR;
	vals[0].nul = 0;
	if (use_domain) {
		keys[1] = domain_col.s;
		vals[1].type = DB_STR;
		vals[1].nul = 0;
		domain = q_memchr(_aor->s, '@', _aor->len);
		vals[0].val.str_val.s   = _aor->s;
		if (domain==0) {
			vals[0].val.str_val.len = 0;
			vals[1].val.str_val = *_aor;
		} else {
			vals[0].val.str_val.len = domain - _aor->s;
			vals[1].val.str_val.s   = domain+1;
			vals[1].val.str_val.len = _aor->s + _aor->len - domain - 1;
		}
	} else {
		vals[0].val.str_val = *_aor;
	}

	columns[0] = contact_col.s;
	columns[1] = expires_col.s;
	columns[2] = q_col.s;
	columns[3] = callid_col.s;
	columns[4] = cseq_col.s;
	columns[5] = flags_col.s;
	columns[6] = cflags_col.s;
	columns[7] = user_agent_col.s;
	columns[8] = received_col.s;
	columns[9] = path_col.s;
	columns[10] = sock_col.s;
	columns[11] = methods_col.s;
	columns[12] = last_mod_col.s;

	if (desc_time_order)
		order = last_mod_col.s;
	else
		order = q_col.s;

	if (ul_dbf.use_table(_c, _d->name->s) < 0) {
		LM_ERR("failed to use table %.*s\n", _d->name->len, _d->name->s);
		return 0;
	}

	if (ul_dbf.query(_c, keys, 0, vals, columns, (use_domain)?2:1, 13, order,
				&res) < 0) {
		LM_ERR("db_query failed\n");
		return 0;
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("aor %.*s not found in table %.*s\n",_aor->len, _aor->s, _d->name->len, _d->name->s);
		ul_dbf.free_result(_c, res);
		return 0;
	}

	r = 0;

	for(i = 0; i < RES_ROW_N(res); i++) {
		ci = dbrow2info(  ROW_VALUES(RES_ROWS(res) + i), &contact);
		if (ci==0) {
			LM_ERR("skipping record for %.*s in table %s\n",
					_aor->len, _aor->s, _d->name->s);
			continue;
		}
		
		if ( r==0 )
			get_static_urecord( _d, _aor, &r);

		if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
			LM_ERR("mem_insert failed\n");
			free_urecord(r);
			ul_dbf.free_result(_c, res);
			return 0;
		}

		/* We have to do this, because insert_ucontact sets state to CS_NEW
		 * and we have the contact in the database already */
		c->state = CS_SYNC;
	}

	ul_dbf.free_result(_c, res);
	return r;
}


int db_timer_udomain(udomain_t* _d)
{
	db_key_t keys[2];
	db_op_t  ops[2];
	db_val_t vals[2];

	keys[0] = expires_col.s;
	ops[0] = "<";
	vals[0].type = DB_DATETIME;
	vals[0].nul = 0;
	vals[0].val.time_val = act_time + 1;

	keys[1] = expires_col.s;
	ops[1] = "!=";
	vals[1].type = DB_DATETIME;
	vals[1].nul = 0;
	vals[1].val.time_val = 0;

	if (ul_dbf.use_table(ul_dbh, _d->name->s) < 0) {
		LM_ERR("use_table failed\n");
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, keys, ops, vals, 2) < 0) {
		LM_ERR("failed to delete from table %s\n",_d->name->s);
		return -1;
	}

	return 0;
}


/* performs a dummy query just to see if DB is ok */
int testdb_udomain(db_con_t* con, udomain_t* d)
{
	db_key_t key[1], col[1];
	db_val_t val[1];
	db_res_t* res = NULL;

	if (ul_dbf.use_table(con, d->name->s) < 0) {
		LM_ERR("failed to change table\n");
		return -1;
	}

	key[0] = user_col.s;

	col[0] = user_col.s;
	VAL_TYPE(val) = DB_STRING;
	VAL_NULL(val) = 0;
	VAL_STRING(val) = "dummy_user";
	
	if (ul_dbf.query( con, key, 0, val, col, 1, 1, 0, &res) < 0) {
		LM_ERR("failure in db_query\n");
		return -1;
	}

	ul_dbf.free_result( con, res);
	return 0;
}


/*
 * Insert a new record into domain
 */
int mem_insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	int sl;
	
	if (new_urecord(_d->name, _aor, _r) < 0) {
		LM_ERR("creating urecord failed\n");
		return -1;
	}

	sl = ((*_r)->aorhash)&(_d->size-1);
	slot_add(&_d->table[sl], *_r);
	update_stat( _d->users, 1);
	return 0;
}


/*
 * Remove a record from domain
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r)
{
	if (_r->watchers == 0) {
		slot_rem(_r->slot, _r);
		free_urecord(_r);
		update_stat( _d->users, -1);
	}
}


int mem_timer_udomain(udomain_t* _d)
{
	struct urecord* ptr, *t;
	int i;

	for(i=0; i<_d->size; i++)
	{
		lock_ulslot(_d, i);

		ptr = _d->table[i].first;

		while(ptr) {
			if (timer_urecord(ptr) < 0) {
				LM_ERR("timer_urecord failed\n");
				unlock_ulslot(_d, i);
				return -1;
			}
		
			/* Remove the entire record if it is empty */
			if (ptr->contacts == 0) {
				t = ptr;
				ptr = ptr->next;
				mem_delete_urecord(_d, t);
			} else {
				ptr = ptr->next;
			}
		}
		unlock_ulslot(_d, i);
	}
	return 0;
}


/*
 * Get lock
 */
void lock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	if (db_mode!=DB_ONLY)
	{
		sl = core_hash(_aor, 0, _d->size);

#ifdef GEN_LOCK_T_PREFERED
		lock_get(_d->table[sl].lock);
#else
		ul_lock_idx(_d->table[sl].lockidx);
#endif
	}
}


/*
 * Release lock
 */
void unlock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	if (db_mode!=DB_ONLY)
	{
		sl = core_hash(_aor, 0, _d->size);
#ifdef GEN_LOCK_T_PREFERED
		lock_release(_d->table[sl].lock);
#else
		ul_release_idx(_d->table[sl].lockidx);
#endif
	}
}

/*
 * Get lock
 */
void lock_ulslot(udomain_t* _d, int i)
{
	if (db_mode!=DB_ONLY)
#ifdef GEN_LOCK_T_PREFERED
		lock_get(_d->table[i].lock);
#else
		ul_lock_idx(_d->table[i].lockidx);
#endif
}


/*
 * Release lock
 */
void unlock_ulslot(udomain_t* _d, int i)
{
	if (db_mode!=DB_ONLY)
#ifdef GEN_LOCK_T_PREFERED
		lock_release(_d->table[i].lock);
#else
		ul_release_idx(_d->table[i].lockidx);
#endif
}



/*
 * Create and insert a new record
 */
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	if (db_mode!=DB_ONLY) {
		if (mem_insert_urecord(_d, _aor, _r) < 0) {
			LM_ERR("inserting record failed\n");
			return -1;
		}
	} else {
		get_static_urecord( _d, _aor, _r);
	}
	return 0;
}


/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	unsigned int sl, i, aorhash;
	urecord_t* r;

	if (db_mode!=DB_ONLY) {
		/* search in cache */
		aorhash = core_hash(_aor, 0, 0);
		sl = aorhash&(_d->size-1);
		r = _d->table[sl].first;

		for(i = 0; i < _d->table[sl].n; i++) {
			if((r->aorhash==aorhash) && (r->aor.len==_aor->len)
						&& !memcmp(r->aor.s,_aor->s,_aor->len)){
				*_r = r;
				return 0;
			}

			r = r->next;
		}
	} else {
		/* search in DB */
		r = db_load_urecord( ul_dbh, _d, _aor);
		if (r) {
			*_r = r;
			return 0;
		}
	}

	return 1;   /* Nothing found */
}


/*
 * Delete a urecord from domain
 */
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r)
{
	struct ucontact* c, *t;

	if (db_mode==DB_ONLY) {
		if (_r==0)
			get_static_urecord( _d, _aor, &_r);
		if (db_delete_urecord(_r)<0) {
			LM_ERR("DB delete failed\n");
			return -1;
		}
		free_urecord(_r);
		return 0;
	}

	if (_r==0) {
		if (get_urecord(_d, _aor, &_r) > 0) {
			return 0;
		}
	}

	c = _r->contacts;
	while(c) {
		t = c;
		c = c->next;
		if (delete_ucontact(_r, t) < 0) {
			LM_ERR("deleting contact failed\n");
			return -1;
		}
	}
	release_urecord(_r);
	return 0;
}
