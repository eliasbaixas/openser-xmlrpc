/*
 * $Id: presentity.c 3736 2008-02-20 13:47:08Z anca_vamanu $
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../db/db.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../alias_db/alias_db.h"
#include "../../data_lump_rpl.h"
#include "presentity.h"
#include "presence.h" 
#include "notify.h"
#include "publish.h"
#include "hash.h"
#include "utils_func.h"

extern int use_db;
extern char* presentity_table;
extern db_con_t* pa_db;
extern db_func_t pa_dbf;

static str pu_200_rpl  = str_init("OK");
static str pu_412_rpl  = str_init("Conditional request failed");

#define ETAG_LEN  128

char* generate_ETag(int publ_count)
{
	char* etag= NULL;
	int size = 0;

	etag = (char*)pkg_malloc(ETAG_LEN*sizeof(char));
	if(etag ==NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(etag, 0, ETAG_LEN*sizeof(char));
	size = sprintf (etag, "%c.%d.%d.%d.%d",prefix, startup_time, pid, counter, publ_count);
	if( size <0 )
	{
		LM_ERR("unsuccessfull sprintf\n ");
		pkg_free(etag);
		return NULL;
	}
	if(size+ 1> ETAG_LEN)
	{
		LM_ERR("buffer size overflown\n");
		pkg_free(etag);
		return NULL;
	}

	etag[size] = '\0';
	LM_DBG("etag= %s / %d\n ",etag, size);
	return etag;

error:
	return NULL;

}

int publ_send200ok(struct sip_msg *msg, int lexpire, str etag)
{
	char buf[128];
	int buf_len= 128, size;
	str hdr_append= {0, 0}, hdr_append2= {0, 0} ;

	LM_DBG("send 200OK reply\n");	
	LM_DBG("etag= %s - len= %d\n", etag.s, etag.len);
	
	hdr_append.s = buf;
	hdr_append.s[0]='\0';
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n",lexpire -
			expires_offset);
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		goto error;
	}
	if(hdr_append.len > buf_len)
	{
		LM_ERR("buffer size overflown\n");
		goto error;
	}
	hdr_append.s[hdr_append.len]= '\0';
		
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	size= sizeof(char)*(20+etag.len) ;
	hdr_append2.s = (char *)pkg_malloc(size);
	if(hdr_append2.s == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	hdr_append2.s[0]='\0';
	hdr_append2.len = sprintf(hdr_append2.s, "SIP-ETag: %s\r\n", etag.s);
	if(hdr_append2.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n ");
		goto error;
	}
	if(hdr_append2.len+1 > size)
	{
		LM_ERR("buffer size overflown\n");
		goto error;
	}

	hdr_append2.s[hdr_append2.len]= '\0';
	if (add_lump_rpl(msg, hdr_append2.s, hdr_append2.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if( slb.reply( msg, 200, &pu_200_rpl)== -1)
	{
		LM_ERR("sending reply\n");
		goto error;
	}

	pkg_free(hdr_append2.s);
	return 0;

error:

	if(hdr_append2.s)
		pkg_free(hdr_append2.s);

	return -1;
}	
presentity_t* new_presentity( str* domain,str* user,int expires, 
		pres_ev_t* event, str* etag, str* sender)
{
	presentity_t *presentity= NULL;
	int size, init_len;
	
	/* allocating memory for presentity */
	size = sizeof(presentity_t)+ (domain->len+ user->len+ etag->len + 50)
		* sizeof(char);
	if(sender)
		size+= sizeof(str)+ sender->len* sizeof(char);
	
	init_len= size;

	presentity = (presentity_t*)pkg_malloc(size);
	if(presentity == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(presentity, 0, size);
	size= sizeof(presentity_t);

	presentity->domain.s = (char*)presentity+ size;
	strncpy(presentity->domain.s, domain->s, domain->len);
	presentity->domain.len = domain->len;
	size+= domain->len;	
	
	presentity->user.s = (char*)presentity+size;
	strncpy(presentity->user.s, user->s, user->len);
	presentity->user.len = user->len;
	size+= user->len;

	presentity->etag.s = (char*)presentity+ size;
	memcpy(presentity->etag.s, etag->s, etag->len);
	presentity->etag.s[etag->len]= '\0';
	presentity->etag.len = etag->len;

	size+= etag->len+1;
	
	if(sender)
	{
		presentity->sender= (str*)((char*)presentity+ size);
		size+= sizeof(str);
		presentity->sender->s= (char*)presentity + size;
		memcpy(presentity->sender->s, sender->s, sender->len);
		presentity->sender->len= sender->len;
		size+= sender->len;
	}

	if(size> init_len)
	{
		LM_ERR("buffer size overflow init_len= %d, size= %d\n", init_len, size);
		goto error;
	}
	presentity->event= event;
	presentity->expires = expires;
	presentity->received_time= (int)time(NULL);
	return presentity;
    
error:
	if(presentity)
		pkg_free(presentity);
	return NULL;
}

int update_presentity(struct sip_msg* msg, presentity_t* presentity, str* body,
		int new_t, int* sent_reply)
{
	db_key_t query_cols[11], update_keys[7], result_cols[5];
	db_op_t  query_ops[11];
	db_val_t query_vals[11], update_vals[7];
	db_res_t *result= NULL;
	int n_query_cols = 0;
	int n_update_cols = 0;
	char* dot= NULL;
	str etag= {0, 0};
	str cur_etag= {0, 0};
	str* rules_doc= NULL;
	str pres_uri= {0, 0};

	*sent_reply= 0;
	if(presentity->event->req_auth)
	{
		/* get rules_document */
		if(presentity->event->get_rules_doc(&presentity->user,
					&presentity->domain, &rules_doc))
		{
			LM_ERR("getting rules doc\n");
			goto error;
		}
	}
	if(uandd_to_uri(presentity->user, presentity->domain, &pres_uri)< 0)
 	{
 		LM_ERR("constructing uri from user and domain\n");
 		goto error;
 	}

	query_cols[n_query_cols] = "domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->domain;
	n_query_cols++;
	
	query_cols[n_query_cols] = "username";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->user;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = "etag";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->etag;
	n_query_cols++;

	result_cols[0]= "expires"; 

	if(new_t) 
	{
		/* insert new record in hash_table */
	
		if(insert_phtable(&pres_uri, presentity->event->evp->parsed)< 0)
		{
			LM_ERR("inserting record in hash table\n");
			goto error;
		}
		
		/* insert new record into database */	
		query_cols[n_query_cols] = "expires";
		query_vals[n_query_cols].type = DB_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->expires+
				(int)time(NULL);
		n_query_cols++;

		query_cols[n_query_cols] = "body";
		query_vals[n_query_cols].type = DB_BLOB;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *body;
		n_query_cols++;
		
		query_cols[n_query_cols] = "received_time";
		query_vals[n_query_cols].type = DB_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->received_time;
		n_query_cols++;

		if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
		{
			LM_ERR("unsuccessful use_table\n");
			goto error;
		}

		LM_DBG("inserting %d cols into table\n",n_query_cols);
				
		if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) 
		{
			LM_ERR("inserting new record in database\n");
			goto error;
		}
		if( publ_send200ok(msg, presentity->expires, presentity->etag)< 0)
		{
			LM_ERR("sending 200OK\n");
			goto error;
		}
		*sent_reply= 1;
		goto send_notify;
	}
	else
	{
		if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
		{
			LM_ERR("unsuccessful sql use table\n");
			goto error;
		}

		if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			 result_cols, n_query_cols, 1, 0, &result) < 0) 
		{
			LM_ERR("unsuccessful sql query\n");
			goto error;
		}
		if(result== NULL)
			goto error;

		if (result->n > 0)
		{
			pa_dbf.free_result(pa_db, result);
			result= NULL;

			if(presentity->expires == 0) 
			{
				if( publ_send200ok(msg, presentity->expires, presentity->etag)< 0)
				{
					LM_ERR("sending 200OK reply\n");
					goto error;
				}
				*sent_reply= 1;
				if( publ_notify( presentity, pres_uri, body, &presentity->etag, rules_doc)< 0 )
				{
					LM_ERR("while sending notify\n");
					goto error;
				}
				
				if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
				{
					LM_ERR("unsuccessful sql use table\n");
					goto error;
				}

				LM_DBG("expires =0 -> deleting from database\n");
				if(pa_dbf.delete(pa_db,query_cols,0,query_vals,n_query_cols)<0)
				{
					LM_ERR("unsuccessful sql delete operation");
					goto error;
				}
				LM_DBG("deleted from db %.*s\n",	presentity->user.len,
						presentity->user.s);

				/* delete from hash table */
	
				if(delete_phtable(&pres_uri, presentity->event->evp->parsed)< 0)
				{
					LM_ERR("deleting record from hash table\n");
					goto error;
				}
				goto done;
			}

			n_update_cols= 0;
			if(presentity->event->etag_not_new== 0)
			{	
				/* generate another etag */
				unsigned int publ_nr;
				str str_publ_nr= {0, 0};

				dot= presentity->etag.s+ presentity->etag.len;
				while(*dot!= '.' && str_publ_nr.len< presentity->etag.len)
				{
					str_publ_nr.len++;
					dot--;
				}
				if(str_publ_nr.len== presentity->etag.len)
				{
					LM_ERR("wrong etag\n");
					goto error;
				}	
				str_publ_nr.s= dot+1;
				str_publ_nr.len--;
	
				if( str2int(&str_publ_nr, &publ_nr)< 0)
				{
					LM_ERR("converting string to int\n");
					goto error;
				}
				etag.s = generate_ETag(publ_nr+1);
				if(etag.s == NULL)
				{
					LM_ERR("while generating etag\n");
					goto error;
				}
				etag.len=(strlen(etag.s));
				
				cur_etag= etag;

				update_keys[n_update_cols] = "etag";
				update_vals[n_update_cols].type = DB_STR;
				update_vals[n_update_cols].nul = 0;
				update_vals[n_update_cols].val.str_val = etag;
				n_update_cols++;

			}
			else
				cur_etag= presentity->etag;
			
			update_keys[n_update_cols] = "expires";
			update_vals[n_update_cols].type = DB_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val= presentity->expires +
				(int)time(NULL);
			n_update_cols++;

			update_keys[n_update_cols] = "received_time";
			update_vals[n_update_cols].type = DB_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val= presentity->received_time;
			n_update_cols++;

			if(body && body->s)
			{
				update_keys[n_update_cols] = "body";
				update_vals[n_update_cols].type = DB_BLOB;
				update_vals[n_update_cols].nul = 0;
				update_vals[n_update_cols].val.str_val = *body;
				n_update_cols++;
			}

			if( pa_dbf.update( pa_db,query_cols, query_ops, query_vals,
			update_keys,update_vals, n_query_cols, n_update_cols )<0) 
			{
				LM_ERR("updating published info in database\n");
				goto error;
			}
			
			/* send 200OK */
			if( publ_send200ok(msg, presentity->expires, cur_etag)< 0)
			{
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			*sent_reply= 1;
			
			if(etag.s)
				pkg_free(etag.s);
			etag.s= NULL;
			
			if(!body)
				goto done;
		
			goto send_notify;
		}  
		else  /* if there isn't no registration with those 3 values */
		{
			pa_dbf.free_result(pa_db, result);
			result= NULL;
			LM_ERR("No E_Tag match\n");
			if (slb.reply(msg, 412, &pu_412_rpl) == -1)
			{
				LM_ERR("sending '412 Conditional request failed' reply\n");
				goto error;
			}
			*sent_reply= 1;
		}
	}

send_notify:
			
	/* send notify with presence information */
	if (publ_notify(presentity, pres_uri, body, NULL, rules_doc)<0)
	{
		LM_ERR("while sending Notify requests to watchers\n");
		goto error;
	}

done:
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s)
		pkg_free(pres_uri.s);

	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(etag.s)
		pkg_free(etag.s);
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s)
		pkg_free(pres_uri.s);

	return -1;

}

int pres_htable_restore(void)
{
	/* query all records from presentity table and insert records 
	 * in presentity table */
	db_key_t result_cols[5];
	db_res_t *result= NULL;
	db_row_t *row= NULL ;	
	db_val_t *row_vals;
	int  i;
	str user, domain, ev_str, uri;
	int n_result_cols= 0;
	int user_col, domain_col, event_col, expires_col;
	int event;
	event_t ev;

	result_cols[user_col= n_result_cols++]= "username";
	result_cols[domain_col= n_result_cols++]= "domain";
	result_cols[event_col= n_result_cols++]= "event";
	result_cols[expires_col= n_result_cols++]= "expires";

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	if (pa_dbf.query (pa_db, 0, 0, 0,result_cols,0, n_result_cols,
				"username", &result) < 0)
	{
		LM_ERR("querying presentity\n");
		goto error;
	}
	if(result== NULL)
		goto error;

	if(result->n<= 0)
	{
		pa_dbf.free_result(pa_db, result);
		return 0;
	}
		
	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		if(row_vals[expires_col].val.int_val< (int)time(NULL))
			continue;

		user.s= (char*)row_vals[user_col].val.string_val;
		user.len= strlen(user.s);
		domain.s= (char*)row_vals[domain_col].val.string_val;
		domain.len= strlen(domain.s);
		ev_str.s= (char*)row_vals[event_col].val.string_val;
		ev_str.len= strlen(ev_str.s);
	
		if(event_parser(ev_str.s, ev_str.len, &ev)< 0)
		{
			LM_ERR("parsing event\n");
			free_event_params(ev.params, PKG_MEM_TYPE);
			goto error;
		}
		event= ev.parsed;
		free_event_params(ev.params, PKG_MEM_TYPE);

		if(uandd_to_uri(user, domain, &uri)< 0)
		{
			LM_ERR("constructing uri\n");
			goto error;
		}
		/* insert in hash_table*/
		if(insert_phtable(&uri, event)< 0)
		{
			LM_ERR("inserting record in presentity hash table");
			pkg_free(uri.s);
			goto error;
		}
		pkg_free(uri.s);
	}
	pa_dbf.free_result(pa_db, result);

	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return -1;	
}
