/*
 * $Id: subscribe.c 3950 2008-03-25 15:02:50Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP serves.
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

#include "../../ut.h"
#include "../../usr_avp.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../parser/contact/parse_contact.h"
#include "presence.h"
#include "subscribe.h"
#include "utils_func.h"
#include "notify.h"
#include "../pua/hash.h"

int get_stored_info(struct sip_msg* msg, subs_t* subs, int* error_ret, str* reply_str);
int get_database_info(struct sip_msg* msg, subs_t* subs, int* error_ret, str* reply_str);
int refresh_watcher(str* pres_uri, str* watcher_uri, str* event, 
		int status, str* reason);

int get_db_subs_auth(subs_t* subs, int* found);
int insert_db_subs_auth(subs_t* subs);

static str su_200_rpl  = str_init("OK");
static str pu_481_rpl  = str_init("Subscription does not exist");
static str pu_400_rpl  = str_init("Bad request");
static str pu_500_rpl  = str_init("Server Internal Error");
static str pu_489_rpl  = str_init("Bad Event");


int send_2XX_reply(struct sip_msg * msg, int reply_code, int lexpire,
		str *rtag, str* local_contact)
{
	static str hdr_append;
	
	hdr_append.s = (char *)pkg_malloc( sizeof(char)*(local_contact->len+ 128));
	if(hdr_append.s == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", lexpire);	
	
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, local_contact->s, local_contact->len);
	hdr_append.len+= local_contact->len;
	strncpy(hdr_append.s+hdr_append.len, ">", 1);
	hdr_append.len += 1;
	strncpy(hdr_append.s+hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len += CRLF_LEN;

	hdr_append.s[hdr_append.len]= '\0';
	
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if( slb.reply_dlg( msg, reply_code, &su_200_rpl, rtag)== -1)
	{
		LM_ERR("sending reply\n");
		goto error;
	}
	
	pkg_free(hdr_append.s);
	return 0;

error:

	pkg_free(hdr_append.s);
	return -1;
}


int delete_db_subs(str pres_uri, str ev_stored_name, str to_tag)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	int n_query_cols= 0;

	query_cols[n_query_cols] = "presentity_uri";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres_uri;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = ev_stored_name;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = to_tag;
	n_query_cols++;
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LM_ERR("in use table sql operation\n");
		return -1;
	}

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals,
				n_query_cols)< 0 )
	{
		LM_ERR("cleaning unsubscribed messages\n");
		return -1;
	}

	return 0;
}

int update_subs_db(subs_t* subs, int type)
{
	db_key_t query_cols[22], update_keys[7];
	db_val_t query_vals[22], update_vals[7];
	int n_update_cols= 0;
	int n_query_cols = 0;

	query_cols[n_query_cols] = "presentity_uri";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->pres_uri;
	n_query_cols++;
	
	query_cols[n_query_cols] = "watcher_username";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "watcher_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	if(subs->event_id.s)
	{
		query_cols[n_query_cols] = "event_id";
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = subs->event_id;
		n_query_cols++;
	}
	query_cols[n_query_cols] = "callid";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = "from_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	if(type & REMOTE_TYPE)
	{
		update_keys[n_update_cols] = "expires";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->expires + (int)time(NULL);
		n_update_cols++;
	
		update_keys[n_update_cols] = "remote_cseq";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->remote_cseq; 
		n_update_cols++;
	}
	else
	{	
		update_keys[n_update_cols] = "local_cseq";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->local_cseq+ 1;
		n_update_cols++;
	
		update_keys[n_update_cols] = "version";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->version+ 1; 
		n_update_cols++;
	}

	update_keys[n_update_cols] = "status";
	update_vals[n_update_cols].type = DB_INT;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.int_val = subs->status;
	n_update_cols++;

	update_keys[n_update_cols] = "reason";
	update_vals[n_update_cols].type = DB_STR;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.str_val = subs->reason;
	n_update_cols++;

	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LM_ERR("in use table sql operation\n");	
		return -1;
	}
		
	if( pa_dbf.update( pa_db,query_cols, 0, query_vals,
				update_keys, update_vals, n_query_cols,n_update_cols)<0) 
	{
		LM_ERR("updating presence information\n");
		return -1;
	}
	return 0;
}

int update_subscription(struct sip_msg* msg, subs_t* subs, int to_tag_gen,
		int* sent_reply)
{	
	unsigned int hash_code;
	
	printf_subs(subs);	
	
	*sent_reply= 0;

	hash_code= core_hash(&subs->pres_uri, &subs->event->name, shtable_size);

	if( to_tag_gen ==0) /*if a SUBSCRIBE within a dialog */
	{
		if(subs->expires == 0)
		{
			LM_DBG("expires =0 -> deleting record\n");
		
			if( delete_db_subs(subs->pres_uri, 
						subs->event->name, subs->to_tag)< 0)
			{
				LM_ERR("deleting subscription record from database\n");
				goto error;
			}
			/* delete record from hash table also */

			subs->local_cseq= delete_shtable(subs_htable,hash_code,
					subs->to_tag);
		
			if(subs->event->type & PUBL_TYPE)
			{	
				if( send_2XX_reply(msg, 202, subs->expires, &subs->to_tag,
							&subs->local_contact) <0)
				{
					LM_ERR("sending 202 OK\n");
					goto error;
				}
				*sent_reply= 1;
				if(subs->event->wipeer)
				{
					if(query_db_notify(&subs->pres_uri,
								subs->event->wipeer, NULL)< 0)
					{
						LM_ERR("Could not send notify for winfo\n");
						goto error;
					}
				}

			}	
			else /* if unsubscribe for winfo */
			{
				if( send_2XX_reply(msg, 200, subs->expires, &subs->to_tag,
							&subs->local_contact) <0)
				{
					LM_ERR("sending 200 OK reply\n");
					goto error;
				}
				*sent_reply= 1;
			}
		
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LM_ERR("Could not send notify\n");
				goto error;
			}
			return 1;
		}

		if(update_shtable(subs_htable, hash_code, subs, REMOTE_TYPE)< 0)
		{
			if(fallback2db)
			{
				/* update in database table */
				if(update_subs_db(subs, REMOTE_TYPE)< 0)
				{
					LM_ERR("updating subscription in database table\n");
					goto error;
				}
			}
			else
			{
				LM_ERR("updating subscription record in hash table\n");
				goto error;
			}
		}
	}
	else
	{
		if(subs->expires!= 0)
		{	
			if(insert_shtable(subs_htable,hash_code,subs)< 0)
			{
				LM_ERR("inserting new record in subs_htable\n");
				goto error;
			}
		}
		/*otherwise there is a subscription outside a dialog with expires= 0 
		 * no update in database, but should try to send Notify */
	}

/* reply_and_notify  */

	if(subs->event->type & PUBL_TYPE)
	{	
		if(send_2XX_reply(msg, 202, subs->expires,&subs->to_tag,
					&subs->local_contact)<0)
		{
			LM_ERR("sending 202 OK reply\n");
			goto error;
		}
		*sent_reply= 1;
		
		if(subs->expires!= 0 && subs->event->wipeer)
		{	
			LM_DBG("send Notify with winfo\n");
			if(query_db_notify(&subs->pres_uri, subs->event->wipeer, subs)< 0)
			{
				LM_ERR("Could not send notify winfo\n");
				goto error;
			}	
			if(subs->send_on_cback== 0)
			{	
				if(notify(subs, NULL, NULL, 0)< 0)
				{
					LM_ERR("Could not send notify\n");
					goto error;
				}
			}
		}
		else
		{
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LM_ERR("Could not send notify\n");
				goto error;
			}
		}	
			
	}
	else 
	{
		if( send_2XX_reply(msg, 200, subs->expires, &subs->to_tag,
					&subs->local_contact)<0)
		{
			LM_ERR("sending 200 OK reply\n");
			goto error;
		}		
		*sent_reply= 1;
		
		if(notify(subs, NULL, NULL, 0 )< 0)
		{
			LM_ERR("sending notify request\n");
			goto error;
		}
	}
	return 0;
	
error:

	LM_ERR("occured\n");
	return -1;

}

void msg_watchers_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[3], result_cols[1];
	db_val_t db_vals[3];
	db_op_t  db_ops[3] ;
	db_res_t *result= NULL;

	LM_DBG("cleaning pending subscriptions\n");
	
	db_keys[0] ="inserted_time";
	db_ops[0] = OP_LT;
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL)- 24*3600 ;

	db_keys[1] = "status";
	db_ops [1] = OP_EQ;
	db_vals[1].type = DB_INT;
	db_vals[1].nul = 0;
	db_vals[1].val.int_val = PENDING_STATUS;

	result_cols[0]= "id";
	if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
	{
		LM_ERR("unsuccessful use table sql operation\n");
		return ;
	}
	
	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols, 2, 1, 0, &result )< 0)
	{
		LM_ERR("querying database for expired messages\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return;
	}
	if(result == NULL)
		return;
	if(result->n <= 0)
	{
		pa_dbf.free_result(pa_db, result);
		return;
	}
	pa_dbf.free_result(pa_db, result);

	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 2) < 0) 
		LM_ERR("cleaning pending subscriptions\n");
}

int handle_subscribe(struct sip_msg* msg, char* str1, char* str2)
{
	int  to_tag_gen = 0;
	subs_t subs;
	pres_ev_t* event= NULL;
	event_t* parsed_event= NULL;
	param_t* ev_param= NULL;
	int found;
	str reason= {0, 0};
	struct sip_uri uri;
	int reply_code;
	str reply_str;
	int sent_reply= 0;

	/* ??? rename to avoid collisions with other symbols */
	counter++;

	memset(&subs, 0, sizeof(subs_t));
	
	reply_code= 500;
	reply_str= pu_500_rpl;

	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		reply_code= 400;
		reply_str= pu_400_rpl;
		goto error;
	}
	
	/* inspecting the Event header field */
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			reply_code= 400;
			reply_str= pu_400_rpl;
			goto error;
		}
		if(((event_t*)msg->event->parsed)->parsed & EVENT_OTHER)
		{	
			goto bad_event;
		}
	}
	else
		goto bad_event;

	/* search event in the list */
	parsed_event= (event_t*)msg->event->parsed;
	event= search_event(parsed_event);
	if(event== NULL)
	{
		goto bad_event;
	}
	subs.event= event;
	
	/* extract the id if any*/
	ev_param= parsed_event->params;
	while(ev_param)
	{
		if(ev_param->name.len== 2 && strncmp(ev_param->name.s, "id", 2)== 0)
		{
			subs.event_id= ev_param->body;
			break;
		}
		ev_param= ev_param->next;
	}		
	
	if(extract_sdialog_info(&subs, msg, max_expires, &to_tag_gen)< 0)
	{
		LM_ERR("failed to extract dialog information\n");
		goto error;
	}

	/* getting presentity uri from Request-URI if initial subscribe - or else from database*/
	if(to_tag_gen)
	{	
		if(parsed_event->parsed!= EVENT_DIALOG)
		{
			if( parse_sip_msg_uri(msg)< 0)
			{
				LM_ERR("failed to parse R-URI\n");
				return -1;
			}

			if(uandd_to_uri(get_to(msg)->parsed_uri.user, msg->parsed_uri.host,
						&subs.pres_uri)< 0)
			{
				LM_ERR("failed to construct uri from user and domain\n");
				goto error;
			}
			LM_DBG("Presentity uri %.*s (%.*s)\n",subs.pres_uri.len,subs.pres_uri.s,get_to(msg)->parsed_uri.user.len,get_to(msg)->parsed_uri.user.s);
		}
	}
	else
	{
		if(get_stored_info(msg, &subs, &reply_code, &reply_str )< 0)
		{
			LM_ERR("getting stored info\n");
			goto error;
		}
		reason= subs.reason;
	}	

	/* call event specific subscription handling */
	if(event->evs_subs_handl)
	{
		if(event->evs_subs_handl(msg)< 0)
		{
			LM_ERR("in event specific subscription handling\n");
			goto error;
		}
	}	


	/* if dialog initiation Subscribe - get subscription state */
	if(to_tag_gen)
	{
		if(!event->req_auth) 
			subs.status = ACTIVE_STATUS;
		else   
		{
			/* query in watchers_table */
			if(get_db_subs_auth(&subs, &found)< 0)
			{
				LM_ERR("getting subscription status from watchers table\n");
				goto error;
			}
			if(found== 0)
			{
				/*default 'pending' status */
				subs.status= PENDING_STATUS;
				subs.reason.s= NULL;
				subs.reason.len= 0;
				/* here a query to xcap server must be done -> new process maybe */
			
				if(parse_uri(subs.pres_uri.s, subs.pres_uri.len, &uri)< 0)
				{
					LM_ERR("parsing uri\n");
					goto error;

				}
				if(subs.event->get_rules_doc(&uri.user, &uri.host, &subs.auth_rules_doc)< 0)
				{
					LM_ERR("getting rules doc\n");
					goto error;
				}
				
				if(subs.event->get_auth_status(&subs)< 0)
				{
					LM_ERR("in event specific function is_watcher_allowed\n");
					goto error;
				}
				if(get_status_str(subs.status)== NULL)
				{
					LM_ERR("wrong status= %d\n", subs.status);
					goto error;
				}
				if(insert_db_subs_auth(&subs)< 0)
				{
					LM_ERR("while inserting record in watchers table\n");
					goto error;
				}
			}
			else
			{
				reason= subs.reason;
			}
		}
	}

	/* check if correct status */
	if(get_status_str(subs.status)== NULL)
	{
		LM_ERR("wrong status\n");
		goto error;
	}
	
	if(update_subscription(msg, &subs, to_tag_gen, &sent_reply) <0)
	{	
		LM_ERR("in update_subscription\n");
		goto error;
	}
	if(subs.auth_rules_doc)
	{
		pkg_free(subs.auth_rules_doc->s);
		pkg_free(subs.auth_rules_doc);
	}
	if(reason.s)
		pkg_free(reason.s);
	
	if(parsed_event->parsed!= EVENT_DIALOG && subs.pres_uri.s)
		pkg_free(subs.pres_uri.s);
	
	if((!server_address.s) || (server_address.len== 0))
	{
		pkg_free(subs.local_contact.s);
	}
	if(subs.record_route.s)
		pkg_free(subs.record_route.s);

	return 1;

bad_event:

	LM_ERR("Missing or unsupported event header field value\n");
		
	if(parsed_event)
		LM_ERR("\tevent= %.*s\n",parsed_event->text.len,parsed_event->text.s);
	
	reply_code= BAD_EVENT_CODE;
	reply_str= pu_489_rpl;

error:
	
	if(sent_reply== 0)
	{
		if(send_error_reply(msg, reply_code, reply_str)< 0)
		{
			LM_ERR("failed to send reply on error case\n");
		}
	}

	if(parsed_event->parsed!= EVENT_DIALOG &&subs.pres_uri.s)
		pkg_free(subs.pres_uri.s);
	
	if(subs.auth_rules_doc)
	{
		if(subs.auth_rules_doc->s)
			pkg_free(subs.auth_rules_doc->s);
		pkg_free(subs.auth_rules_doc);
	}
	if(reason.s)
		pkg_free(reason.s);

	if(((!server_address.s) ||(server_address.len== 0))&& subs.local_contact.s)
	{
		pkg_free(subs.local_contact.s);
	}
	if(subs.record_route.s)
		pkg_free(subs.record_route.s);

	return -1;

}


int extract_sdialog_info(subs_t* subs,struct sip_msg* msg, int mexp, int* to_tag_gen)
{
	static char buf[50];
	str rec_route= {0, 0};
	int rt  = 0;
	str* contact= NULL;
	contact_body_t *b;
	struct to_body *pto, *pfrom = NULL, TO;
	int lexpire;
	str rtag_value;
	struct sip_uri uri;

	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LM_ERR("cannot parse Expires header\n");
			goto error;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		LM_DBG("'Expires' header found, value= %d\n", lexpire);

	}
	else 
	{
		LM_DBG("'expires' not found; default=%d\n",subs->event->default_expires);
		lexpire = subs->event->default_expires;
	}
	if(lexpire > mexp)
		lexpire = mexp;

	subs->expires = lexpire;

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		goto error;
	}
	/* examine the to header */
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",pto->uri.len,pto->uri.s);
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		if( !parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO));
		{
			LM_DBG("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}

	if( pto->parsed_uri.user.s && pto->parsed_uri.host.s &&
		pto->parsed_uri.user.len && pto->parsed_uri.host.len)
	{
		subs->to_user = pto->parsed_uri.user;
		subs->to_domain = pto->parsed_uri.host;
	}
	else
	{
		if(parse_uri(pto->uri.s, pto->uri.len, &uri)< 0)
		{
			LM_ERR("while parsing uri\n");
			goto error;
		}
		if(pto!= &TO)
		   pto->parsed_uri = uri;
		subs->to_user = uri.user;
		subs->to_domain = uri.host;
	}

	/* examine the from header */
	if (!msg->from || !msg->from->body.s)
	{
		LM_DBG("cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_DBG("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_DBG("cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	
	if( pfrom->parsed_uri.user.s && pfrom->parsed_uri.host.s && 
		pfrom->parsed_uri.user.len && pfrom->parsed_uri.host.len)
	{
		subs->from_user = pfrom->parsed_uri.user;
		subs->from_domain = pfrom->parsed_uri.host;
	}
	else
	{
		if(parse_uri(pfrom->uri.s, pfrom->uri.len, &uri)< 0)
		{
			LM_ERR("while parsing uri\n");
			goto error;
		}
		subs->from_user = uri.user;
		subs->from_domain = uri.host;
	}

	if(subs->event->evp->parsed== EVENT_DIALOG)
	{
		subs->pres_uri= pfrom->uri;
	}

	/*generate to_tag if the message does not have a to_tag*/
	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LM_DBG("generating to_tag\n");
		*to_tag_gen = 1;
		/*generate to_tag then insert it in avp*/
		
		rtag_value.s = buf;
		rtag_value.len = sprintf(rtag_value.s,"%s.%d.%d.%d", to_tag_pref,
				pid, (int)time(NULL), counter);
		if(rtag_value.len<= 0)
		{
			LM_ERR("while creating to_tag\n");
			goto error;
		}
	}
	else
	{
		*to_tag_gen = 0;
		rtag_value=pto->tag_value;
	}
	subs->to_tag = rtag_value;

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	subs->callid = msg->callid->body;

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LM_ERR("cannot parse cseq header\n");
		goto error;
	}
	if (str2int( &(get_cseq(msg)->number), &subs->remote_cseq)!=0 )
	{
		LM_ERR("cannot parse cseq number\n");
		goto error;
	}
	if( msg->contact==NULL || msg->contact->body.s==NULL)
	{
		LM_ERR("cannot parse contact header\n");
		goto error;
	}
	if( parse_contact(msg->contact) <0 )
	{
		LM_ERR(" cannot parse contact"
				" header\n");
		goto error;
	}
	b= (contact_body_t* )msg->contact->parsed;

	if(b == NULL)
	{
		LM_ERR("cannot parse contact header\n");
		goto error;
	}
	subs->contact = b->contacts->uri;
	
	LM_DBG("subs->contact= %.*s - len = %d\n",subs->contact.len,
			subs->contact.s, subs->contact.len);	

	/*process record route and add it to a string*/
	if(*to_tag_gen && msg->record_route!=NULL)
	{
		rt = print_rr_body(msg->record_route, &rec_route, 0, 0);
		if(rt != 0)
		{
			LM_ERR("processing the record route [%d]\n", rt);	
			rec_route.s=NULL;
			rec_route.len=0;
		//	goto error;
		}
	}
	subs->record_route = rec_route;
			
	subs->sockinfo_str= msg->rcv.bind_address->sock_str;

	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	subs->from_tag = pfrom->tag_value;

	subs->version = 0;
	
	if((!server_address.s) || (server_address.len== 0))
	{
		contact= get_local_contact(msg);
		if(contact== NULL)
		{
			LM_ERR("in function get_local_contact\n");
			goto error;
		}
		subs->local_contact= *contact;
	}
	else
		subs->local_contact= server_address;
	
	return 0;
	
error:

	return -1;
}


int get_stored_info(struct sip_msg* msg, subs_t* subs, int* reply_code,
		str* reply_str)
{	
	str pres_uri= {0, 0}, reason={0, 0};
	subs_t* s;
	int i;
	unsigned int hash_code;

	/* first try to_user== pres_user and to_domain== pres_domain */

	uandd_to_uri(subs->to_user, subs->to_domain, &pres_uri);
	if(pres_uri.s== NULL)
	{
		LM_ERR("creating uri from user and domain\n");
		return -1;
	}
	hash_code= core_hash(&pres_uri, &subs->event->name, shtable_size);
	lock_get(&subs_htable[hash_code].lock);
	i= hash_code;
	s= search_shtable(subs_htable, subs->callid, subs->to_tag,
			subs->from_tag, hash_code);
	if(s)
	{
		goto found_rec;
	}
	lock_release(&subs_htable[hash_code].lock);

	pkg_free(pres_uri.s);
	pres_uri.s= NULL;
	LM_DBG("record not found using R-URI search iteratively\n");
	/* take one row at a time */
	for(i= 0; i< shtable_size; i++)
	{
		lock_get(&subs_htable[i].lock);
		s= search_shtable(subs_htable, subs->callid,subs->to_tag,subs->from_tag, i);
		if(s && s->event->evp->parsed!= EVENT_DIALOG)
		{
			pres_uri.s= (char*)pkg_malloc(s->pres_uri.len* sizeof(char));
			if(pres_uri.s== NULL)
			{
				lock_release(&subs_htable[i].lock);
				ERR_MEM(PKG_MEM_STR);
			}
			memcpy(pres_uri.s, s->pres_uri.s, s->pres_uri.len);
			pres_uri.len= s->pres_uri.len;
			goto found_rec;
		}
		lock_release(&subs_htable[i].lock);
	}

	if(fallback2db)
	{
		return get_database_info(msg, subs, reply_code, reply_str);	
	}

	LM_ERR("record not found in hash_table\n");
	*reply_code= 481;
	*reply_str= pu_481_rpl;

	return -1;

found_rec:
	
	LM_DBG("Record found in hash_table\n");
	
	if(s->event->evp->parsed!= EVENT_DIALOG)
		subs->pres_uri= pres_uri;
	
	subs->status= s->status;
	if(s->reason.s && s->reason.len)
	{	
		reason.s= (char*)pkg_malloc(s->reason.len* sizeof(char));
		if(reason.s== NULL)
		{
			lock_release(&subs_htable[i].lock);
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(reason.s, s->reason.s, s->reason.len);
		reason.len= s->reason.len;
		subs->reason= reason;
	}
	if(s->record_route.s && s->record_route.len)
	{
		subs->record_route.s= (char*)pkg_malloc
			(s->record_route.len* sizeof(char));
		if(subs->record_route.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, s->record_route.s, s->record_route.len);
		subs->record_route.len= s->record_route.len;
	}

	subs->local_cseq= s->local_cseq;
	
	if(subs->remote_cseq<= s->remote_cseq)
	{
		LM_ERR("wrong sequence number;received: %d - stored: %d\n",
				subs->remote_cseq, s->remote_cseq);
		
		*reply_code= 400;
		*reply_str= pu_400_rpl;

		lock_release(&subs_htable[i].lock);
		goto error;
	}	
	lock_release(&subs_htable[i].lock);

	return 0;

error:
	if(subs->reason.s)
		pkg_free(subs->reason.s);
	subs->reason.s= NULL;
	if(subs->record_route.s)
		pkg_free(subs->record_route.s);
	subs->record_route.s= NULL;
	return -1;
}

int get_database_info(struct sip_msg* msg, subs_t* subs, int* reply_code, str* reply_str)
{	
	db_key_t query_cols[10];
	db_val_t query_vals[10];
	db_key_t result_cols[8];
	db_res_t *result= NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int remote_cseq_col= 0, local_cseq_col= 0, status_col, reason_col;
	int record_route_col;
	int pres_uri_col;
	unsigned int remote_cseq;
	str pres_uri, record_route;
	str reason;

	query_cols[n_query_cols] = "to_user";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "to_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "watcher_username";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "watcher_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = "event_id";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	if( subs->event_id.s != NULL)
	{
		query_vals[n_query_cols].val.str_val.s = subs->event_id.s;
		query_vals[n_query_cols].val.str_val.len = subs->event_id.len;
	} else {
		query_vals[n_query_cols].val.str_val.s = "";
		query_vals[n_query_cols].val.str_val.len = 0;
	}
	n_query_cols++;
	
	query_cols[n_query_cols] = "callid";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = "from_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	result_cols[pres_uri_col=n_result_cols++] = "presentity_uri";
	result_cols[remote_cseq_col=n_result_cols++] = "remote_cseq";
	result_cols[local_cseq_col=n_result_cols++] = "local_cseq";
	result_cols[status_col=n_result_cols++] = "status";
	result_cols[reason_col=n_result_cols++] = "reason";
	result_cols[record_route_col=n_result_cols++] = "record_route";
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LM_ERR("unsuccessful use_table sql operation\n");
		return -1;
	}
	
	if (pa_dbf.query (pa_db, query_cols, 0, query_vals,
		 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
	{
		LM_ERR("querying subscription dialog\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;

	if(result && result->n <=0)
	{
		LM_ERR("No matching subscription dialog found in database\n");
		
		pa_dbf.free_result(pa_db, result);
		*reply_code= 481;
		*reply_str= pu_481_rpl;

		return -1;
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	remote_cseq= row_vals[remote_cseq_col].val.int_val;
	
	if(subs->remote_cseq<= remote_cseq)
	{
		LM_ERR("wrong sequence number received: %d - stored: %d\n",
				subs->remote_cseq, remote_cseq);
		*reply_code= 400;
		*reply_str= pu_400_rpl;
		pa_dbf.free_result(pa_db, result);
		return -1;
	}
	
	subs->status= row_vals[status_col].val.int_val;
	reason.s= (char*)row_vals[reason_col].val.string_val;
	if(reason.s)
	{
		reason.len= strlen(reason.s);
		subs->reason.s= (char*)pkg_malloc(reason.len* sizeof(char));
		if(subs->reason.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->reason.s, reason.s, reason.len);
		subs->reason.len= reason.len;
	}

	subs->local_cseq= row_vals[local_cseq_col].val.int_val;
	
	if(subs->event->evp->parsed!= EVENT_DIALOG)
	{
		pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
		pres_uri.len= strlen(pres_uri.s);
		subs->pres_uri.s= (char*)pkg_malloc(pres_uri.len* sizeof(char));
		if(subs->pres_uri.s== NULL)
		{	
			if(subs->reason.s)
				pkg_free(subs->reason.s);
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->pres_uri.s, pres_uri.s, pres_uri.len);
		subs->pres_uri.len= pres_uri.len;
	}

	record_route.s= (char*)row_vals[record_route_col].val.string_val;
	if(record_route.s)
	{
		record_route.len= strlen(record_route.s);
		subs->record_route.s= (char*)pkg_malloc(record_route.len*sizeof(char));
		if(subs->record_route.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, record_route.s, record_route.len);
		subs->record_route.len= record_route.len;
	}

	pa_dbf.free_result(pa_db, result);
	result= NULL;

	return 0;
error:
	if(result)
		pa_dbf.free_result(pa_db, result);

	return -1;

}


int handle_expired_subs(subs_t* s)
{
	/* send Notify with state=terminated;reason=timeout */
	
	s->status= TERMINATED_STATUS;
	s->reason.s= "timeout";
	s->reason.len= 7;
	s->expires= 0;

	if(send_notify_request(s, NULL, NULL, 1)< 0)
	{
		LM_ERR("send Notify not successful\n");
		return -1;
	}
	
	return 0;

}

void timer_db_update(unsigned int ticks,void *param)
{	
	int no_lock=0;

	if(ticks== 0 && param == NULL)
		no_lock= 1;

	if(pa_dbf.use_table(pa_db, active_watchers_table)< 0)
	{
		LM_ERR("sql use table failed\n");
		return;
	}

	update_db_subs(pa_db, pa_dbf, subs_htable, 
			shtable_size, no_lock, handle_expired_subs);

}

void update_db_subs(db_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func)
{	
	db_key_t query_cols[22], update_cols[7], result_cols[6];
	db_val_t query_vals[22], update_vals[7];
	db_op_t update_ops[1];
	subs_t* del_s;

	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col,
		callid_col, totag_col, fromtag_col, event_col,status_col, event_id_col, 
		local_cseq_col, remote_cseq_col, expires_col, record_route_col, 
		contact_col, local_contact_col, version_col,socket_info_col,reason_col;
	int u_expires_col, u_local_cseq_col, u_remote_cseq_col, u_version_col, 
		u_reason_col, u_status_col; 
	int i;
	subs_t* s= NULL, *prev_s= NULL;
	int n_query_cols= 0, n_update_cols= 0;
	int n_query_update;

	query_cols[pres_uri_col= n_query_cols] ="presentity_uri";
	query_vals[pres_uri_col].type = DB_STR;
	query_vals[pres_uri_col].nul = 0;
	n_query_cols++;
	
	query_cols[callid_col= n_query_cols] ="callid";
	query_vals[callid_col].type = DB_STR;
	query_vals[callid_col].nul = 0;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] ="to_tag";
	query_vals[totag_col].type = DB_STR;
	query_vals[totag_col].nul = 0;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] ="from_tag";
	query_vals[fromtag_col].type = DB_STR;
	query_vals[fromtag_col].nul = 0;
	n_query_cols++;

	n_query_update= n_query_cols;

	query_cols[to_user_col= n_query_cols] ="to_user";
	query_vals[to_user_col].type = DB_STR;
	query_vals[to_user_col].nul = 0;
	n_query_cols++;

	query_cols[to_domain_col= n_query_cols] ="to_domain";
	query_vals[to_domain_col].type = DB_STR;
	query_vals[to_domain_col].nul = 0;
	n_query_cols++;
	
	query_cols[from_user_col= n_query_cols] ="watcher_username";
	query_vals[from_user_col].type = DB_STR;
	query_vals[from_user_col].nul = 0;
	n_query_cols++;

	query_cols[from_domain_col= n_query_cols] ="watcher_domain";
	query_vals[from_domain_col].type = DB_STR;
	query_vals[from_domain_col].nul = 0;
	n_query_cols++;

	query_cols[event_col= n_query_cols] ="event";
	query_vals[event_col].type = DB_STR;
	query_vals[event_col].nul = 0;
	n_query_cols++;	

	query_cols[event_id_col= n_query_cols] ="event_id";
	query_vals[event_id_col].type = DB_STR;
	query_vals[event_id_col].nul = 0;
	n_query_cols++;

	query_cols[local_cseq_col= n_query_cols]="local_cseq";
	query_vals[local_cseq_col].type = DB_INT;
	query_vals[local_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[remote_cseq_col= n_query_cols]="remote_cseq";
	query_vals[remote_cseq_col].type = DB_INT;
	query_vals[remote_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[expires_col= n_query_cols] ="expires";
	query_vals[expires_col].type = DB_INT;
	query_vals[expires_col].nul = 0;
	n_query_cols++;

	query_cols[status_col= n_query_cols] ="status";
	query_vals[status_col].type = DB_INT;
	query_vals[status_col].nul = 0;
	n_query_cols++;

	query_cols[reason_col= n_query_cols] ="reason";
	query_vals[reason_col].type = DB_STR;
	query_vals[reason_col].nul = 0;
	n_query_cols++;

	query_cols[record_route_col= n_query_cols] ="record_route";
	query_vals[record_route_col].type = DB_STR;
	query_vals[record_route_col].nul = 0;
	n_query_cols++;
	
	query_cols[contact_col= n_query_cols] ="contact";
	query_vals[contact_col].type = DB_STR;
	query_vals[contact_col].nul = 0;
	n_query_cols++;

	query_cols[local_contact_col= n_query_cols] ="local_contact";
	query_vals[local_contact_col].type = DB_STR;
	query_vals[local_contact_col].nul = 0;
	n_query_cols++;

	query_cols[socket_info_col= n_query_cols] ="socket_info";
	query_vals[socket_info_col].type = DB_STR;
	query_vals[socket_info_col].nul = 0;
	n_query_cols++;

	query_cols[version_col= n_query_cols]="version";
	query_vals[version_col].type = DB_INT;
	query_vals[version_col].nul = 0;
	n_query_cols++;

	/* cols and values used for update */
	update_cols[u_expires_col= n_update_cols]= "expires";
	update_vals[u_expires_col].type = DB_INT;
	update_vals[u_expires_col].nul = 0;
	n_update_cols++;

	update_cols[u_status_col= n_update_cols]= "status";
	update_vals[u_status_col].type = DB_INT;
	update_vals[u_status_col].nul = 0;
	n_update_cols++;

	update_cols[u_reason_col= n_update_cols]= "reason";
	update_vals[u_reason_col].type = DB_STR;
	update_vals[u_reason_col].nul = 0;
	n_update_cols++;

	update_cols[u_remote_cseq_col= n_update_cols]= "remote_cseq";
	update_vals[u_remote_cseq_col].type = DB_INT;
	update_vals[u_remote_cseq_col].nul = 0;
	n_update_cols++;

	update_cols[u_local_cseq_col= n_update_cols]= "local_cseq";
	update_vals[u_local_cseq_col].type = DB_INT;
	update_vals[u_local_cseq_col].nul = 0;
	n_update_cols++;
	
	update_cols[u_version_col= n_update_cols]= "version";
	update_vals[u_version_col].type = DB_INT;
	update_vals[u_version_col].nul = 0;
	n_update_cols++;

	result_cols[0]= "expires";

	if(db== NULL)
	{
		LM_ERR("null database connection\n");
		return;
	}
	for(i=0; i<htable_size; i++) 
	{
		if(!no_lock)
			lock_get(&hash_table[i].lock);	

		prev_s= hash_table[i].entries;
		s= prev_s->next;
	
		while(s)
		{
			printf_subs(s);
			if(s->expires < (int)time(NULL)- 50)	
			{
				LM_DBG("Found expired record\n");
				if(!no_lock)
				{
					if(handle_expired_func(s)< 0)
					{
						LM_ERR("in function handle_expired_record\n");
						if(!no_lock)
							lock_release(&hash_table[i].lock);	
						return ;
					}
				}
				del_s= s;	
				s= s->next;
				prev_s->next= s;
				
				shm_free(del_s);
				continue;
			}
			switch(s->db_flag)
			{
				case NO_UPDATEDB_FLAG:
				{
					LM_DBG("NO_UPDATEDB_FLAG\n");
					break;			  
				}
				case UPDATEDB_FLAG:
				{
					LM_DBG("UPDATEDB_FLAG\n");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
				
					update_vals[u_expires_col].val.int_val= s->expires;
					update_vals[u_local_cseq_col].val.int_val= s->local_cseq;
					update_vals[u_remote_cseq_col].val.int_val= s->remote_cseq;
					update_vals[u_version_col].val.int_val= s->version;
					update_vals[u_status_col].val.int_val= s->status;
					update_vals[u_reason_col].val.str_val= s->reason;

					if(dbf.update(db, query_cols, 0, query_vals, update_cols, 
								update_vals, n_query_update, n_update_cols)< 0)
					{
						LM_ERR("updating in database\n");
						if(!no_lock)
							lock_release(&hash_table[i].lock);	
						return ;
					}
					break;
				}
				case  INSERTDB_FLAG:
				{
					LM_DBG("INSERTDB_FLAG\n");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
					query_vals[to_user_col].val.str_val = s->to_user;
					query_vals[to_domain_col].val.str_val = s->to_domain;
					query_vals[from_user_col].val.str_val = s->from_user;
					query_vals[from_domain_col].val.str_val = s->from_domain;
					query_vals[event_col].val.str_val = s->event->name;
					query_vals[event_id_col].val.str_val = s->event_id;
					query_vals[local_cseq_col].val.int_val= s->local_cseq;
					query_vals[remote_cseq_col].val.int_val= s->remote_cseq;
					query_vals[expires_col].val.int_val = s->expires;
					query_vals[record_route_col].val.str_val = s->record_route;
					query_vals[contact_col].val.str_val = s->contact;
					query_vals[local_contact_col].val.str_val = s->local_contact;
					query_vals[version_col].val.int_val= s->version;
					query_vals[status_col].val.int_val= s->status;
					query_vals[reason_col].val.str_val= s->reason;
					query_vals[socket_info_col].val.str_val= s->sockinfo_str;
				
					if(dbf.insert(db,query_cols,query_vals,n_query_cols )<0)
					{
						LM_ERR("unsuccessful sql insert\n");
						if(!no_lock)
							lock_release(&hash_table[i].lock);
						return ;
					}
					break;										
				}

			}
			s->db_flag= NO_UPDATEDB_FLAG;	
			prev_s= s;
			s= s->next;
		}
		if(!no_lock)
			lock_release(&hash_table[i].lock);	
	}

	update_vals[0].val.int_val= (int)time(NULL)- 10;
	update_ops[0]= OP_LT;
	if(dbf.delete(db, update_cols, update_ops, update_vals, 1) < 0)
	{
		LM_ERR("deleting expired information from database\n");
	}

}

int restore_db_subs(void)
{
	db_key_t result_cols[22]; 
	db_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	int i;
	int n_result_cols= 0;
	int pres_uri_col, expires_col, from_user_col, from_domain_col,to_user_col; 
	int callid_col,totag_col,fromtag_col,to_domain_col,sockinfo_col,reason_col;
	int event_col,contact_col,record_route_col, event_id_col, status_col;
	int remote_cseq_col, local_cseq_col, local_contact_col, version_col;
	subs_t s;
	str ev_sname;
	pres_ev_t* event= NULL;
	event_t parsed_event;
	unsigned int expires;
	unsigned int hash_code;

	result_cols[pres_uri_col=n_result_cols++]	="presentity_uri";		
	result_cols[expires_col=n_result_cols++]="expires";
	result_cols[event_col=n_result_cols++]	="event";
	result_cols[event_id_col=n_result_cols++]="event_id";
	result_cols[to_user_col=n_result_cols++]	="to_user";
	result_cols[to_domain_col=n_result_cols++]	="to_domain";
	result_cols[from_user_col=n_result_cols++]	="watcher_username";
	result_cols[from_domain_col=n_result_cols++]="watcher_domain";
	result_cols[callid_col=n_result_cols++] ="callid";
	result_cols[totag_col=n_result_cols++]	="to_tag";
	result_cols[fromtag_col=n_result_cols++]="from_tag";
	result_cols[local_cseq_col= n_result_cols++]	="local_cseq";
	result_cols[remote_cseq_col= n_result_cols++]	="remote_cseq";
	result_cols[record_route_col= n_result_cols++]	="record_route";
	result_cols[sockinfo_col= n_result_cols++]	="socket_info";
	result_cols[contact_col= n_result_cols++]	="contact";
	result_cols[local_contact_col= n_result_cols++]	="local_contact";
	result_cols[version_col= n_result_cols++]	="version";
	result_cols[status_col= n_result_cols++]	="status";
	result_cols[reason_col= n_result_cols++]	="reason";
	
	if(!pa_db)
	{
		LM_ERR("null database connection\n");
		return -1;
	}
	if(pa_dbf.use_table(pa_db, active_watchers_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}

	if(pa_dbf.query(pa_db,0, 0, 0, result_cols,0, n_result_cols, 0,&res)< 0)
	{
		LM_ERR("while querrying table\n");
		if(res)
		{
			pa_dbf.free_result(pa_db, res);
			res = NULL;
		}
		return -1;
	}
	if(res== NULL)
		return -1;

	if(res->n<=0)
	{
		LM_INFO("The query returned no result\n");
		pa_dbf.free_result(pa_db, res);
		res = NULL;
		return 0;
	}

	LM_DBG("found %d db entries\n", res->n);

	for(i =0 ; i< res->n ; i++)
	{
		row = &res->rows[i];
		row_vals = ROW_VALUES(row);
		memset(&s, 0, sizeof(subs_t));

		expires= row_vals[expires_col].val.int_val;
		
		if(expires< (int)time(NULL))
			continue;
	
		s.pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
		s.pres_uri.len= strlen(s.pres_uri.s);
		
		s.to_user.s=(char*)row_vals[to_user_col].val.string_val;
		s.to_user.len= strlen(s.to_user.s);

		s.to_domain.s=(char*)row_vals[to_domain_col].val.string_val;
		s.to_domain.len= strlen(s.to_domain.s);

		s.from_user.s=(char*)row_vals[from_user_col].val.string_val;
		s.from_user.len= strlen(s.from_user.s);
		
		s.from_domain.s=(char*)row_vals[from_domain_col].val.string_val;
		s.from_domain.len= strlen(s.from_domain.s);

		s.to_tag.s=(char*)row_vals[totag_col].val.string_val;
		s.to_tag.len= strlen(s.to_tag.s);

		s.from_tag.s=(char*)row_vals[fromtag_col].val.string_val;
		s.from_tag.len= strlen(s.from_tag.s);

		s.callid.s=(char*)row_vals[callid_col].val.string_val;
		s.callid.len= strlen(s.callid.s);

		ev_sname.s= (char*)row_vals[event_col].val.string_val;
		ev_sname.len= strlen(ev_sname.s);
		
		event= contains_event(&ev_sname, &parsed_event);
		if(event== NULL)
		{
			LM_DBG("insert a new event structure in the list waiting"
					" to be filled in\n");
	
			/*insert a new event structure in the list waiting to be filled in*/
			event= (pres_ev_t*)shm_malloc(sizeof(pres_ev_t));
			if(event== NULL)
			{
				free_event_params(parsed_event.params, PKG_MEM_TYPE);
				ERR_MEM(SHM_MEM_STR);
			}
			memset(event, 0, sizeof(pres_ev_t));
			event->name.s= (char*)shm_malloc(ev_sname.len* sizeof(char));
			if(event->name.s== NULL)
			{
				free_event_params(parsed_event.params, PKG_MEM_TYPE);
				ERR_MEM(SHM_MEM_STR);
			}
			memcpy(event->name.s,ev_sname.s, ev_sname.len);
			event->name.len= ev_sname.len;
			
			event->evp= shm_copy_event(&parsed_event);
			if(event->evp== NULL)
			{
				LM_ERR("ERROR copying event_t structure\n");
				free_event_params(parsed_event.params, PKG_MEM_TYPE);
				goto error;
			}
			event->next= EvList->events;
			EvList->events= event;
		}
			
		free_event_params(parsed_event.params, PKG_MEM_TYPE);

		s.event= event;

		s.event_id.s=(char*)row_vals[event_id_col].val.string_val;
		if(s.event_id.s)
			s.event_id.len= strlen(s.event_id.s);

		s.remote_cseq= row_vals[remote_cseq_col].val.int_val;
		s.local_cseq= row_vals[local_cseq_col].val.int_val;
		s.version= row_vals[version_col].val.int_val;
		
		s.expires= expires- (int)time(NULL);
		s.status= row_vals[status_col].val.int_val;

		s.reason.s= (char*)row_vals[reason_col].val.string_val;
		if(s.reason.s)
			s.reason.len= strlen(s.reason.s);

		s.contact.s=(char*)row_vals[contact_col].val.string_val;
		s.contact.len= strlen(s.contact.s);

		s.local_contact.s=(char*)row_vals[local_contact_col].val.string_val;
		s.local_contact.len= strlen(s.local_contact.s);
	
		s.record_route.s=(char*)row_vals[record_route_col].val.string_val;
		if(s.record_route.s)
			s.record_route.len= strlen(s.record_route.s);
	
		s.sockinfo_str.s=(char*)row_vals[sockinfo_col].val.string_val;
		s.sockinfo_str.len= strlen(s.sockinfo_str.s);

		hash_code= core_hash(&s.pres_uri, &s.event->name, shtable_size);
		if(insert_shtable(subs_htable, hash_code, &s)< 0)
		{
			LM_ERR("adding new record in hash table\n");
			goto error;
		}
	}

	pa_dbf.free_result(pa_db, res);

	/* delete all records */
	if(pa_dbf.delete(pa_db, 0,0,0,0)< 0)
	{
		LM_ERR("deleting all records from database table\n");
		return -1;
	}

	return 0;

error:
	if(res)
		pa_dbf.free_result(pa_db, res);
	return -1;

}

int refresh_watcher(str* pres_uri, str* watcher_uri, str* event, 
		int status, str* reason)
{
	unsigned int hash_code;
	subs_t* s, *s_copy;
	pres_ev_t* ev;		
	struct sip_uri uri;
	str user, domain;
	/* refresh status in subs_htable and send notify */

	ev=	contains_event(event, NULL);
	if(ev== NULL)
	{
		LM_ERR("while searching event in list\n");
		return -1;
	}

	if(parse_uri(watcher_uri->s, watcher_uri->len, &uri)< 0)
	{
		LM_ERR("parsing uri\n");
		return -1;
	}
	user= uri.user;
	domain= uri.host;

	hash_code= core_hash(pres_uri, event, shtable_size);

	lock_get(&subs_htable[hash_code].lock);

	s= subs_htable[hash_code].entries->next;

	while(s)
	{
		if(s->event== ev && s->pres_uri.len== pres_uri->len &&
			strncmp(s->pres_uri.s, pres_uri->s, pres_uri->len)== 0 &&
			s->from_user.len==user.len && strncmp(s->from_user.s,user.s, user.len)==0 &&
			s->from_domain.len== domain.len && 
			strncmp(s->from_domain.s, domain.s, domain.len)== 0)
		{
			s->status= status;
			if(reason)
				s->reason= *reason;
			
			s_copy= mem_copy_subs(s, PKG_MEM_TYPE);
			if(s_copy== NULL)
			{
				LM_ERR("copying subs_t\n");
				lock_release(&subs_htable[hash_code].lock);
				return -1;
			}
			lock_release(&subs_htable[hash_code].lock);
			if(notify(s_copy, NULL, NULL, 0)< 0)
			{
				LM_ERR("in notify function\n");
				pkg_free(s_copy);
				return -1;
			}
			pkg_free(s_copy);
			lock_get(&subs_htable[hash_code].lock);
		}
		s= s->next;
	}
	return 0;
}

int get_db_subs_auth(subs_t* subs, int* found)
{
	db_key_t db_keys[5];
	db_val_t db_vals[5];
	int n_query_cols= 0; 
	db_key_t result_cols[3];
	db_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;

	db_keys[n_query_cols] ="presentity_uri";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val= subs->pres_uri;
	n_query_cols++;

	db_keys[n_query_cols] ="watcher_username";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;

	db_keys[n_query_cols] ="watcher_domain";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;
	
	db_keys[n_query_cols] ="event";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	result_cols[0] = "status";
	result_cols[1] = "reason";

	if(pa_dbf.use_table(pa_db, watchers_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}	

	if(pa_dbf.query(pa_db, db_keys, 0, db_vals, result_cols,
					n_query_cols, 2, 0, &result )< 0)
	{
		LM_ERR("while querying watchers table\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;
	
	if(result->n<= 0)
	{
		*found= 0;
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	*found= 1;
	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	subs->status= row_vals[0].val.int_val;

	if(row_vals[1].val.string_val)
	{

		subs->reason.len= strlen(row_vals[1].val.string_val);
		if(subs->reason.len== 0)
			subs->reason.s= NULL;
		else
		{
			subs->reason.s= (char*)pkg_malloc(subs->reason.len*sizeof(char));
			if(subs->reason.s== NULL)
			{
				pa_dbf.free_result(pa_db, result);
				ERR_MEM(PKG_MEM_STR);
			}		
			memcpy(subs->reason.s, row_vals[1].val.string_val, subs->reason.len);
		}
	}
	
	pa_dbf.free_result(pa_db, result);
	return 0;
error:
	return -1;
}	

int insert_db_subs_auth(subs_t* subs)
{
	db_key_t db_keys[10];
	db_val_t db_vals[10];
	int n_query_cols= 0; 

	db_keys[n_query_cols] ="presentity_uri";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val= subs->pres_uri;
	n_query_cols++;

	db_keys[n_query_cols] ="watcher_username";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;

	db_keys[n_query_cols] ="watcher_domain";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;
	
	db_keys[n_query_cols] ="event";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	db_keys[n_query_cols] ="status";
	db_vals[n_query_cols].type = DB_INT;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.int_val = subs->status;
	n_query_cols++;
								
	db_keys[n_query_cols] = "inserted_time";
	db_vals[n_query_cols].type = DB_INT;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.int_val= (int)time(NULL);
	n_query_cols++;
	
	if(subs->reason.s && subs->reason.len)
	{
		db_keys[n_query_cols] ="reason";
		db_vals[n_query_cols].type = DB_STR;
		db_vals[n_query_cols].nul = 0;
		db_vals[n_query_cols].val.str_val = subs->reason;
		n_query_cols++;	
	}	
	
	if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return -1;
	}

	if(pa_dbf.insert(pa_db, db_keys, db_vals, n_query_cols )< 0)
	{	
		LM_ERR("in sql insert\n");
		return -1;
	}

	return 0;
}
