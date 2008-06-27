/*
 * $Id: subscribe.h 2923 2007-10-16 11:38:54Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2006-08-15  initial version (anca)
 */

#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

//#include "presence.h"
#include "../../str.h"
#include "../../db/db.h"

struct pres_ev;

#include "event_list.h"
#include "hash.h"

#define ACTIVE_STATUS        1
#define PENDING_STATUS       2
#define TERMINATED_STATUS    3

struct subscription
{
	str pres_uri;
	str to_user;
	str to_domain;
	str from_user;
	str from_domain;
	struct pres_ev* event;
	str event_id;
	str to_tag;
	str from_tag;
	str callid;
	str sockinfo_str;
	unsigned int remote_cseq; 
	unsigned int local_cseq; 
	str contact;
	str local_contact;
	str record_route;
	unsigned int expires;
	unsigned int status;
	str reason;
	int version;
	int send_on_cback;
	int db_flag;
	str* auth_rules_doc;
	struct subscription* next;

};
typedef struct subscription subs_t;

void msg_active_watchers_clean(unsigned int ticks,void *param);

void msg_watchers_clean(unsigned int ticks,void *param);

int handle_subscribe(struct sip_msg*, char*, char*);

int delete_db_subs(str pres_uri, str ev_stored_name, str to_tag);

void timer_db_update(unsigned int ticks,void *param);

int update_subs_db(subs_t* subs, int type);

int refresh_watcher(str* pres_uri, str* watcher_uri, str* event, 
	int status, str* reason);

typedef int (*refresh_watcher_t)(str*, str* , str* ,int , str* );

int restore_db_subs(void);

typedef int (*handle_expired_func_t)(subs_t* );

void update_db_subs(db_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func);

typedef void (*update_db_subs_t)(db_con_t * ,db_func_t ,shtable_t ,int ,int ,
		handle_expired_func_t);

int extract_sdialog_info(subs_t* subs,struct sip_msg* msg, int max_expire,
		int* to_tag_gen);
typedef int (*extract_sdialog_info_t)(subs_t* subs, struct sip_msg* msg,
		int max_expire, int* to_tag_gen);

#endif
