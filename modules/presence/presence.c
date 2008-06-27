/*
 * $Id: presence.c 4016 2008-04-15 12:26:10Z anca_vamanu $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../db/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h" 
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../tm/tm_load.h"
#include "../sl/sl_api.h"
#include "../../pt.h"
#include "../../mi/mi.h"
#include "../pua/hash.h"
#include "publish.h"
#include "subscribe.h"
#include "event_list.h"
#include "bind_presence.h"
#include "notify.h"

MODULE_VERSION

#define S_TABLE_VERSION  3
#define P_TABLE_VERSION  2
#define ACTWATCH_TABLE_VERSION 9

char *log_buf = NULL;
static int clean_period=100;

/* database connection */
db_con_t *pa_db = NULL;
db_func_t pa_dbf;
char *presentity_table="presentity";
char *active_watchers_table = "active_watchers";
char *watchers_table= "watchers";  

int use_db=1;
str server_address= {0, 0};
evlist_t* EvList= NULL;

/* to tag prefix */
char* to_tag_pref = "10";

/* TM bind */
struct tm_binds tmb;
/* SL bind */
struct sl_binds slb;

/** module functions */

static int mod_init(void);
static int child_init(int);
void destroy(void);
int handle_publish(struct sip_msg*, char*, char*);
int handle_subscribe(struct sip_msg*, char*, char*);
int stored_pres_info(struct sip_msg* msg, char* pres_uri, char* s);
static int fixup_presence(void** param, int param_no);
struct mi_root* mi_refreshWatchers(struct mi_root* cmd, void* param);
int update_pw_dialogs(subs_t* subs, unsigned int hash_code, subs_t** subs_array);
int mi_child_init(void);

int counter =0;
int pid = 0;
char prefix='a';
int startup_time=0;
str db_url = {0, 0};
int expires_offset = 0;
int max_expires= 3600;
int shtable_size= 9;
shtable_t subs_htable= NULL;
int fallback2db= 0;

int phtable_size= 9;
phtable_t* pres_htable;

static cmd_export_t cmds[]=
{
	{"handle_publish",		handle_publish,	     0,	   0,         0, REQUEST_ROUTE},
	{"handle_publish",		handle_publish,	     1,fixup_presence,0, REQUEST_ROUTE},
	{"handle_subscribe",	handle_subscribe,	 0,	   0,         0, REQUEST_ROUTE},
	{"bind_presence",(cmd_function)bind_presence,1,    0,         0,    0         },
	{0,						0,				     0,	   0,         0,    0	      }	 
};

static param_export_t params[]={
	{ "db_url",					STR_PARAM, &db_url.s},
	{ "presentity_table",		STR_PARAM, &presentity_table},
	{ "active_watchers_table", 	STR_PARAM, &active_watchers_table},
	{ "watchers_table",			STR_PARAM, &watchers_table},
	{ "clean_period",			INT_PARAM, &clean_period },
	{ "to_tag_pref",			STR_PARAM, &to_tag_pref },
	{ "expires_offset",			INT_PARAM, &expires_offset },
	{ "max_expires",			INT_PARAM, &max_expires },
	{ "server_address",         STR_PARAM, &server_address.s},
	{ "subs_htable_size",       INT_PARAM, &shtable_size},
	{ "pres_htable_size",       INT_PARAM, &phtable_size},
	{ "fallback2db",            INT_PARAM, &fallback2db},
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "refreshWatchers", mi_refreshWatchers,    0,  0,  mi_child_init},
	{  0,                0,                     0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"presence",					/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	mi_cmds,   					/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	(response_function) 0,      /* response handling function */
	(destroy_function) destroy, /* destroy function */
	child_init                  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	str _s;
	int ver = 0;

	LM_NOTICE("initializing module ...\n");

	if(db_url.s== NULL)
	{
		use_db= 0;
		LM_DBG("presence module used for library purpose only\n");
		EvList= init_evlist();
		if(!EvList)
		{
			LM_ERR("unsuccessful initialize event list\n");
			return -1;
		}
		return 0;

	}

	if(expires_offset<0)
		expires_offset = 0;
	
	if(to_tag_pref==NULL || strlen(to_tag_pref)==0)
		to_tag_pref="10";

	if(max_expires<= 0)
		max_expires = 3600;

	if(server_address.s== NULL)
		LM_DBG("server_address parameter not set in configuration file\n");
	
	if(server_address.s)
		server_address.len= strlen(server_address.s);
	else
		server_address.len= 0;

	/* load SL API */
	if(load_sl_api(&slb)==-1)
	{
		LM_ERR("can't load sl functions\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1)
	{
		LM_ERR("can't load tm functions\n");
		return -1;
	}

	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,db_url.s);
	
	/* binding to mysql module  */
	if (bind_dbmod(db_url.s, &pa_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	

	if (!DB_CAPABILITY(pa_dbf, DB_CAP_ALL))
	{
		LM_ERR("Database module does not implement all functions"
				" needed by presence module\n");
		return -1;
	}

	pa_db = pa_dbf.init(db_url.s);
	if (!pa_db)
	{
		LM_ERR("connecting database\n");
		return -1;
	}
	// verify table version 
	_s.s = presentity_table;
	_s.len = strlen(presentity_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=P_TABLE_VERSION)
	{
		LM_ERR("Wrong version v%d for table <%s>, need v%d\n", 
				ver, _s.s, P_TABLE_VERSION);
		return -1;
	}
	
	_s.s = active_watchers_table;
	_s.len = strlen(active_watchers_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=ACTWATCH_TABLE_VERSION)
	{
		LM_ERR("Wrong version v%d for table <%s>, need v%d\n", 
				ver, _s.s, ACTWATCH_TABLE_VERSION);
		return -1;
	}

	_s.s = watchers_table;
	_s.len = strlen(watchers_table);
	 ver =  table_version(&pa_dbf, pa_db, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LM_ERR("Wrong version v%d for table <%s>, need v%d\n",
				ver, _s.s, S_TABLE_VERSION);
		return -1;
	}

	EvList= init_evlist();
	if(!EvList)
	{
		LM_ERR("initializing event list\n");
		return -1;
	}	

	if(clean_period<=0)
	{
		LM_DBG("wrong clean_period \n");
		return -1;
	}

	if(shtable_size< 1)
		shtable_size= 512;
	else
		shtable_size= 1<< shtable_size;

	subs_htable= new_shtable(shtable_size);
	if(subs_htable== NULL)
	{
		LM_ERR(" initializing subscribe hash table\n");
		return -1;
	}
	if(restore_db_subs()< 0)
	{
		LM_ERR("restoring subscribe info from database\n");
		return -1;
	}

	if(phtable_size< 1)
		phtable_size= 256;
	else
		phtable_size= 1<< phtable_size;

	pres_htable= new_phtable();
	if(pres_htable== NULL)
	{
		LM_ERR("initializing presentity hash table\n");
		return -1;
	}
	if(pres_htable_restore()< 0)
	{
		LM_ERR("filling in presentity hash table from database\n");
		return -1;
	}
	
	startup_time = (int) time(NULL);
	
	register_timer(msg_presentity_clean, 0, clean_period);
	
	register_timer(msg_watchers_clean, 0, clean_period);
	
	register_timer(timer_db_update, 0, clean_period);

	if(pa_db)
		pa_dbf.close(pa_db);
	pa_db = NULL;
	
	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	LM_NOTICE("init_child [%d]  pid [%d]\n", rank, getpid());
	
	pid = my_pid();
	
	if(use_db== 0)
		return 0;

	if (pa_dbf.init==0)
	{
		LM_CRIT("child_init: database not bound\n");
		return -1;
	}
	pa_db = pa_dbf.init(db_url.s);
	if (!pa_db)
	{
		LM_ERR("child %d: unsuccessful connecting to database\n", rank);
		return -1;
	}
		
	if (pa_dbf.use_table(pa_db, presentity_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table presentity_table\n", rank);
		return -1;
	}
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table active_watchers_table\n",
				rank);
		return -1;
	}
	if (pa_dbf.use_table(pa_db, watchers_table) < 0)  
	{
		LM_ERR( "child %d:unsuccessful use_table watchers_table\n", rank);
		return -1;
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);
	
	return 0;
}

int mi_child_init(void)
{
	if(use_db== 0)
		return 0;

	if (pa_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	pa_db = pa_dbf.init(db_url.s);
	if (!pa_db)
	{
		LM_ERR("connecting database\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, presentity_table) < 0)  
	{
		LM_ERR( "unsuccessful use_table presentity_table\n");
		return -1;
	}
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0)  
	{
		LM_ERR( "unsuccessful use_table active_watchers_table\n");
		return -1;
	}
	if (pa_dbf.use_table(pa_db, watchers_table) < 0)  
	{
		LM_ERR( "unsuccessful use_table watchers_table\n");
		return -1;
	}

	LM_DBG("Database connection opened successfully\n");
	return 0;
}


/*
 * destroy function
 */
void destroy(void)
{
	LM_NOTICE("destroy module ...\n");

	if(subs_htable && pa_db)
		timer_db_update(0, 0);

	if(subs_htable)
		destroy_shtable(subs_htable, shtable_size);
	
	if(pres_htable)
		destroy_phtable();

	if(pa_db && pa_dbf.close)
		pa_dbf.close(pa_db);
	
	destroy_evlist();
}

static int fixup_presence(void** param, int param_no)
{
 	pv_elem_t *model;
	str s;
 	if(*param)
 	{
		s.s = (char*)(*param); s.len = strlen(s.s);
 		if(pv_parse_format(&s, &model)<0)
 		{
 			LM_ERR( "wrong format[%s]\n",(char*)(*param));
 			return E_UNSPEC;
 		}
 
 		*param = (void*)model;
 		return 0;
 	}
 	LM_ERR( "null format\n");
 	return E_UNSPEC;
}

/* 
 *  mi cmd: refreshWatchers
 *			<presentity_uri> 
 *			<event>
 *          <refresh_type> // can be:  = 0 -> watchers autentification type or
 *									  != 0 -> publish type //		   
 *		* */

struct mi_root* mi_refreshWatchers(struct mi_root* cmd, void* param)
{
	struct mi_node* node= NULL;
	str pres_uri, event;
	struct sip_uri uri;
	pres_ev_t* ev;
	str* rules_doc= NULL;
	int result;
	unsigned int refresh_type;

	LM_DBG("start\n");
	
	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	/* Get presentity URI */
	pres_uri = node->value;
	if(pres_uri.s == NULL || pres_uri.len== 0)
	{
		LM_ERR( "empty uri\n");
		return init_mi_tree(404, "Empty presentity URI", 20);
	}
	
	node = node->next;
	if(node == NULL)
		return 0;
	event= node->value;
	if(event.s== NULL || event.len== 0)
	{
		LM_ERR( "empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);
	}
	LM_DBG("event '%.*s'\n",  event.len, event.s);
	
	node = node->next;
	if(node == NULL)
		return 0;
	if(node->value.s== NULL || node->value.len== 0)
	{
		LM_ERR( "empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);		
	}
	if(str2int(&node->value, &refresh_type)< 0)
	{
		LM_ERR("converting string to int\n");
		goto error;
	}

	if(node->next!= NULL)
	{
		LM_ERR( "Too many parameters\n");
		return init_mi_tree(400, "Too many parameters", 19);
	}

	ev= contains_event(&event, NULL);
	if(ev== NULL)
	{
		LM_ERR( "wrong event parameter\n");
		return 0;
	}
	
	if(refresh_type== 0) /* if a request to refresh watchers authorization*/
	{
		if(ev->get_rules_doc== NULL)
		{
			LM_ERR("wrong request for a refresh watchers authorization status"
					"for an event that does not require authorization\n");
			goto error;
		}
		
		if(parse_uri(pres_uri.s, pres_uri.len, &uri)< 0)
		{
			LM_ERR( "parsing uri\n");
			goto error;
		}

		result= ev->get_rules_doc(&uri.user,&uri.host,&rules_doc);
		if(result< 0 || rules_doc==NULL || rules_doc->s== NULL)
		{
			LM_ERR( "getting rules doc\n");
			goto error;
		}
	
		if(update_watchers_status(pres_uri, ev, rules_doc)< 0)
		{
			LM_ERR("failed to update watchers\n");
			goto error;
		}

		pkg_free(rules_doc->s);
		pkg_free(rules_doc);
		rules_doc = NULL;

	}                 
	else     /* if a request to refresh Notified info */
	{
		if(query_db_notify(&pres_uri, ev, NULL)< 0)
		{
			LM_ERR("sending Notify requests\n");
			goto error;
		}

	}
		
	return init_mi_tree(200, "OK", 2);

error:
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	return 0;
}

int pres_db_delete_status(subs_t* s)
{
    int n_query_cols= 0;
    db_key_t query_cols[5];
    db_val_t query_vals[5];

    if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
    {
        LM_ERR("sql use table failed\n");
        return -1;
    }

    query_cols[n_query_cols]= "event";
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB_STR;
    query_vals[n_query_cols].val.str_val= s->event->name ;
    n_query_cols++;

    query_cols[n_query_cols]= "presentity_uri";
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB_STR;
    query_vals[n_query_cols].val.str_val= s->pres_uri;
    n_query_cols++;

    query_cols[n_query_cols]= "watcher_username";
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB_STR;
    query_vals[n_query_cols].val.str_val= s->from_user;
    n_query_cols++;

    query_cols[n_query_cols]= "watcher_domain";
    query_vals[n_query_cols].nul= 0;
    query_vals[n_query_cols].type= DB_STR;
    query_vals[n_query_cols].val.str_val= s->from_domain;
    n_query_cols++;

    if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols)< 0)
    {
        LM_ERR("sql delete failed\n");
        return -1;
    }
    return 0;

}

int update_watchers_status(str pres_uri, pres_ev_t* ev, str* rules_doc)
{
	subs_t subs;
	db_key_t query_cols[6], result_cols[5], update_cols[5];
	db_val_t query_vals[6], update_vals[5];
	int n_update_cols= 0, n_result_cols= 0, n_query_cols = 0;
	db_res_t* result= NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int i;
	str w_user, w_domain, reason;
	unsigned int status;
	int status_col, w_user_col, w_domain_col, reason_col;
	int u_status_col, u_reason_col, q_wuser_col, q_wdomain_col;
	subs_t* subs_array= NULL,* s;
	unsigned int hash_code;
	int err_ret= -1;

	if(ev->content_type.s== NULL)
	{
		ev= contains_event(&ev->name, NULL);
		if(ev== NULL)
		{
			LM_ERR("wrong event parameter\n");
			return 0;
		}
	}

	subs.pres_uri= pres_uri;
	subs.event= ev;
	subs.auth_rules_doc= rules_doc;

	/* update in watchers_table */
	query_cols[n_query_cols]= "presentity_uri";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	query_vals[n_query_cols].val.str_val= pres_uri;
	n_query_cols++;

	query_cols[n_query_cols]= "event";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	query_vals[n_query_cols].val.str_val= ev->name;
	n_query_cols++;

	result_cols[status_col= n_result_cols++]= "status";
	result_cols[reason_col= n_result_cols++]= "reason";
	result_cols[w_user_col= n_result_cols++]= "watcher_username";
	result_cols[w_domain_col= n_result_cols++]= "watcher_domain";
	
	update_cols[u_status_col= n_update_cols]= "status";
	update_vals[u_status_col].nul= 0;
	update_vals[u_status_col].type= DB_INT;
	n_update_cols++;

	update_cols[u_reason_col= n_update_cols]= "reason";
	update_vals[u_reason_col].nul= 0;
	update_vals[u_reason_col].type= DB_STR;
	n_update_cols++;

	if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
	{
		LM_ERR( "in use_table\n");
		goto done;
	}

	if(pa_dbf.query(pa_db, query_cols, 0, query_vals, result_cols,n_query_cols,
				n_result_cols, 0, &result)< 0)
	{
		LM_ERR( "in sql query\n");
		goto done;
	}
	if(result== NULL)
		return 0;

	if(result->n<= 0)
	{
		err_ret= 0;
		goto done;
	}

	query_cols[q_wuser_col=n_query_cols]= "watcher_username";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	n_query_cols++;

	query_cols[q_wdomain_col=n_query_cols]= "watcher_domain";
	query_vals[n_query_cols].nul= 0;
	query_vals[n_query_cols].type= DB_STR;
	n_query_cols++;

	hash_code= core_hash(&pres_uri, &ev->name, shtable_size);
	lock_get(&subs_htable[hash_code].lock);

	for(i = 0; i< result->n; i++)
	{
		row= &result->rows[i];
		row_vals = ROW_VALUES(row);

		status= row_vals[status_col].val.int_val;
	
		reason.s= (char*)row_vals[reason_col].val.string_val;
		reason.len= reason.s?strlen(reason.s):0;

		w_user.s= (char*)row_vals[w_user_col].val.string_val;
		w_user.len= strlen(w_user.s);

		w_domain.s= (char*)row_vals[w_domain_col].val.string_val;
		w_domain.len= strlen(w_domain.s);

		subs.from_user= w_user;
		subs.from_domain= w_domain;
		memset(&subs.reason, 0, sizeof(str));
		if(ev->get_auth_status(&subs)< 0)
		{
			LM_ERR( "getting status from rules document\n");
			lock_release(&subs_htable[hash_code].lock);
			goto done;
		}
		LM_DBG("subs.status= %d\n", subs.status);
		if(get_status_str(subs.status)== NULL)
		{
			LM_ERR("wrong status: %d\n", subs.status);
			lock_release(&subs_htable[hash_code].lock);
			goto done;
		}
		if(subs.status!= status || reason.len!= subs.reason.len ||
			(reason.s && subs.reason.s && strncmp(reason.s, subs.reason.s,
												  reason.len)))
		{
			/* update in watchers_table */
			query_vals[q_wuser_col].val.str_val= w_user; 
			query_vals[q_wdomain_col].val.str_val= w_domain; 

			update_vals[u_status_col].val.int_val= subs.status;
			update_vals[u_reason_col].val.str_val= subs.reason;
			if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
			{
				LM_ERR( "in use_table\n");
				lock_release(&subs_htable[hash_code].lock);
				goto done;
			}

			if(pa_dbf.update(pa_db, query_cols, 0, query_vals, update_cols,
						update_vals, n_query_cols, n_update_cols)< 0)
			{
				LM_ERR( "in sql update\n");
				lock_release(&subs_htable[hash_code].lock);
				goto done;
			}
			/* save in the list all affected dialogs */
			/* if status switches to terminated -> delete dialog */
			if(update_pw_dialogs(&subs, hash_code, &subs_array)< 0)
			{
				LM_ERR( "extracting dialogs from [watcher]=%.*s@%.*s to"
					" [presentity]=%.*s\n",	w_user.len, w_user.s, w_domain.len,
					w_domain.s, pres_uri.len, pres_uri.s);
				lock_release(&subs_htable[hash_code].lock);
				goto done;
			}
		 }
			
	}
	lock_release(&subs_htable[hash_code].lock);
	pa_dbf.free_result(pa_db, result);
	result= NULL;
	s= subs_array;

	while(s)
	{
		if(notify(s, NULL, NULL, 0)< 0)
		{
			LM_ERR( "sending Notify request\n");
			goto done;
		}
        /* delete terminated dialogs from database */
        if(s->status== TERMINATED_STATUS)
        {
            if(pres_db_delete_status(s)<0)
            {
                err_ret= -1;
                LM_ERR("failed to delete terminated dialog from database\n");
                goto done;
            }
        }
        s= s->next;
	}
	
	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	return 0;

done:
	if(result)
		pa_dbf.free_result(pa_db, result);
	free_subs_list(subs_array, PKG_MEM_TYPE, 0);
	return err_ret;
}

int update_pw_dialogs(subs_t* subs, unsigned int hash_code, subs_t** subs_array)
{
	subs_t* s, *ps, *cs;
	int i= 0;

	ps= subs_htable[hash_code].entries;
	
	while(ps && ps->next)
	{
		s= ps->next;

		if(s->event== subs->event && s->pres_uri.len== subs->pres_uri.len &&
			s->from_user.len== subs->from_user.len && 
			s->from_domain.len==subs->from_domain.len &&
			strncmp(s->pres_uri.s, subs->pres_uri.s, subs->pres_uri.len)== 0 &&
			strncmp(s->from_user.s, subs->from_user.s, s->from_user.len)== 0 &&
			strncmp(s->from_domain.s,subs->from_domain.s,s->from_domain.len)==0)
		{
			i++;
			s->status= subs->status;
			s->reason= subs->reason;
			s->db_flag= UPDATEDB_FLAG;

			cs= mem_copy_subs(s, PKG_MEM_TYPE);
			if(cs== NULL)
			{
				LM_ERR( "copying subs_t stucture\n");
				return -1;
			}
			cs->expires-= (int)time(NULL);
			cs->next= (*subs_array);
			(*subs_array)= cs;
			if(s->status== TERMINATED_STATUS)
			{
				cs->expires= 0;
				ps->next= s->next;
				shm_free(s->contact.s);
                shm_free(s);
			}
			else
				ps= s;

			printf_subs(cs);
		}
		else
			ps= s;
	}
	LM_DBG("found %d matching dialogs\n", i);

	return 0;
}
