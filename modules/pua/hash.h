/*
 * $Id: hash.h 4065 2008-04-21 17:06:17Z anca_vamanu $
 *
 * pua module - presence user agent module
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
 */


#ifndef _PU_HASH_H_
#define _PU_HASH_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../str.h"
#include "../../lock_ops.h"
#include "../../dprint.h"
#include "../../parser/msg_parser.h"

#define PRESENCE_EVENT      1<<0
#define PWINFO_EVENT        1<<1
#define BLA_EVENT           1<<2
#define MSGSUM_EVENT        1<<3

#define UL_PUBLISH          1<<0
#define BLA_PUBLISH         1<<1
#define BLA_SUBSCRIBE		1<<2
#define XMPP_PUBLISH		1<<3
#define XMPP_SUBSCRIBE		1<<4
#define XMPP_INITIAL_SUBS   1<<5
#define MI_PUBLISH			1<<6
#define MI_ASYN_PUBLISH     1<<7
#define MI_SUBSCRIBE		1<<8
#define RLS_SUBSCRIBE       1<<9

#define NO_UPDATEDB_FLAG    1<<0
#define UPDATEDB_FLAG       1<<1
#define INSERTDB_FLAG       1<<2

#define MAX_FORWARD  70

typedef struct ua_pres{
 
    /* common*/
	str id;
    str* pres_uri;
	int event;
	unsigned int expires;
	unsigned int desired_expires;
	int flag;
	int db_flag;
	void* cb_param;
	struct ua_pres* next;
	int ua_flag;
	
	/* publish */
	str etag;
	str tuple_id;
	str* body;
	str content_type;

	/* subscribe */
	str* watcher_uri;
	str call_id;
	str to_tag;
    str from_tag;
	int cseq;
	int version;
    int watcher_count;
	str* outbound_proxy;
	str* extra_headers;
	str record_route;
	str contact;

	/*?? should this be long? */
}ua_pres_t;

typedef struct hash_entry
{
	ua_pres_t* entity;
	gen_lock_t lock;
}hash_entry_t;	

typedef struct htable{
    hash_entry_t* p_records;        	              
}htable_t;

htable_t* new_htable(void);

ua_pres_t* search_htable(ua_pres_t* pres, unsigned int hash_code);

void insert_htable(ua_pres_t* presentity );

void update_htable(ua_pres_t* presentity,time_t desired_expires,
		int expires, str* etag, unsigned int hash_code);

void delete_htable(ua_pres_t* presentity, unsigned int hash_code);

void destroy_htable(void);
int is_dialog(ua_pres_t* dialog);

ua_pres_t* get_dialog(ua_pres_t* dialog, unsigned int hash_code);

int get_record_id(ua_pres_t* dialog, str** rec_id);
typedef int (*get_record_id_t)(ua_pres_t* dialog, str** rec_id);

/* for degug */
void print_ua_pres(ua_pres_t* p);

typedef int  (*query_dialog_t)(ua_pres_t* presentity);

static inline int get_event_flag(str* event)
{
    switch (event->len) {
    case 8:
	if (strncmp(event->s, "presence", 8) == 0)
	    return PRESENCE_EVENT;
	break;
    case 10:
	if (strncmp(event->s, "dialog;sla", 10) == 0)
	    return BLA_EVENT;
	break;
    case 14:
	if (strncmp(event->s, "presence;winfo", 14) == 0)
	    return PWINFO_EVENT;
	break;
    case 15:
	if (strncmp(event->s, "message-summary", 15) == 0)
	    return MSGSUM_EVENT;
	break;
    default:
	break;
    }
    LM_ERR("Unknown event string\n");
    return -1;
}	


#endif
