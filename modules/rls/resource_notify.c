/*
 * $Id: resource_notify.c 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-09-11  initial version (anca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../pua/hash.h"
#include "rls.h"
#include "notify.h"
#include "resource_notify.h"

/* how to relate resource oriented dialogs to list_uri */
/* sol1: use the same callid in Subscribe requests 
 * sol2: include an extra header
 * sol3: put the list_uri as the id of the record stored in
 * pua and write a function to return that id
 * winner: sol3
 * */
static str su_200_rpl     = str_init("OK");

#define CONT_COPY(buf, dest, source)\
	dest.s= (char*)buf+ size;\
	memcpy(dest.s, source.s, source.len);\
	dest.len= source.len;\
	size+= source.len;


#define CONT_COPY_1 (buf, dest_s, dest_len, source_s, source_len)\
	dest_s= (char*)buf+ size;\
	memcpy(dest_s, source_s, source_len);\
	dest_len= source_len;\
	size+= source_len;

int parse_subs_state(str auth_state, str** reason, int* expires)
{
	str str_exp;
	str* res= NULL;
	char* smc= NULL;
	int len, flag= -1;


	if( strncmp(auth_state.s, "active", 6)== 0)
		flag= ACTIVE_STATE;

	if( strncmp(auth_state.s, "pending", 7)== 0)
		flag= PENDING_STATE; 

	if( strncmp(auth_state.s, "terminated", 10)== 0)
	{
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("terminated state and no reason found");
			return -1;
		}
		if(strncmp(smc+1, "reason=", 7))
		{
			LM_ERR("terminated state and no reason found");
			return -1;
		}
		res= (str*)pkg_malloc(sizeof(str));
		if(res== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		len=  auth_state.len- 10- 1- 7;
		res->s= (char*)pkg_malloc(len* sizeof(char));
		if(res->s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(res->s, smc+ 8, len);
		res->len= len;
		return TERMINATED_STATE;
	}

	if(flag> 0)
	{
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
		}
		if(strncmp(smc+1, "expires=", 8))
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
		}
		str_exp.s= smc+ 9;
		str_exp.len= auth_state.s+ auth_state.len- smc- 9;

		if( str2int(&str_exp, (unsigned int*)expires)< 0)
		{
			LM_ERR("while getting int from str\n");
			return -1;
		}
		return flag;
	
	}
	return -1;

error:
	if(res)
	{
		if(res->s)
			pkg_free(res->s);
		pkg_free(res);
	}
	return -1;
}

int rls_handle_notify(struct sip_msg* msg, char* c1, char* c2)
{
	struct to_body *pto, TO, *pfrom= NULL;
	str body= {0, 0};
	ua_pres_t dialog;
	str* res_id= NULL;
	db_key_t query_cols[9], result_cols[1];
	db_val_t query_vals[9];
	db_res_t* result= NULL;
	int n_query_cols= 0;
	str auth_state= {0, 0};
	int found= 0;
	str* reason= NULL;
	int auth_flag;
	struct hdr_field* hdr= NULL;
	int n, expires= -1;
	str content_type= {0, 0};


	LM_DBG("start\n");
	/* extract the dialog information and check if an existing dialog*/	
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		return -1;
	}
	if((!msg->event ) ||(msg->event->body.len<=0))
	{
		LM_ERR("Missing event header field value\n");
		return -1;
	}
	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return -1;
	}
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			LM_ERR(" 'To' header NOT parsed\n");
			return -1;
		}
		pto = &TO;
	}
	dialog.watcher_uri= &pto->uri;
    if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LM_ERR("to tag value not parsed\n");
		goto error;
	}
	dialog.from_tag= pto->tag_value;
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	dialog.call_id = msg->callid->body;

	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_DBG("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_ERR("cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	dialog.pres_uri= &pfrom->uri;
		
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	dialog.to_tag= pfrom->tag_value;
	dialog.flag|= RLS_SUBSCRIBE;

	dialog.event= get_event_flag(&msg->event->body);
	if(dialog.event< 0)
	{
		LM_ERR("unrecognized event package\n");
		goto error;
	}
	if(pua_get_record_id(&dialog, &res_id)< 0) // verify if within a stored dialog
	{
		LM_ERR("occured when trying to get record id\n");
		goto error;
	}
	if(res_id== 0)
	{
		LM_ERR("record not found\n");
		goto error;
	}

	/* extract the subscription state */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(strncmp(hdr->name.s, "Subscription-State", 18)==0)  
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found==0 )
	{
		LM_ERR("'Subscription-State' header not found\n");
		goto error;
	}	
	auth_state = hdr->body;

	/* extract state and reason */
	auth_flag= parse_subs_state(auth_state, &reason, &expires);
	if(auth_flag< 0)
	{
		LM_ERR("while parsing 'Subscription-State' header\n");
		goto error;
	}
	if(msg->content_type== NULL || msg->content_type->body.s== NULL)
	{
		LM_DBG("cannot find content type header header\n");
	}
	else
		content_type= msg->content_type->body;

	/*constructing the xml body*/
	if(get_content_length(msg) == 0 )
	{
		goto done;
	}	
	else
	{
		if(content_type.s== 0)
		{
			LM_ERR("content length != 0 and no content type header found\n");
			goto error;
		}
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body from msg\n");
			goto error;
		}
		body.len = get_content_length( msg );

	}
	/* update in rlpres_table where rlsusb_did= res_id and resource_uri= from_uri*/
	
	LM_DBG("body= %.*s\n", body.len, body.s);

	query_cols[n_query_cols]= "rlsubs_did";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *res_id; 
	n_query_cols++;
	
	query_cols[n_query_cols]= "resource_uri";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *dialog.pres_uri; 
	n_query_cols++;

	query_cols[n_query_cols]= "updated";
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= UPDATED_TYPE; 
	n_query_cols++;

	query_cols[n_query_cols]= "auth_state";
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= auth_flag; 
	n_query_cols++;

	if(reason)
	{
		query_cols[n_query_cols]= "reason";
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val= *reason;
		n_query_cols++;
	}
	query_cols[n_query_cols]= "content_type";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= content_type;
	n_query_cols++;

	query_cols[n_query_cols]= "presence_state";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= body;
	n_query_cols++;
	
	query_cols[n_query_cols]= "expires";
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= expires+ (int)time(NULL);
	n_query_cols++;

	if (rls_dbf.use_table(rls_db, rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	/* query-> if not present insert // else update */
	result_cols[0]= "updated";

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					2, 1, 0, &result)< 0)
	{
		LM_ERR("in sql query\n");
		if(result)
			rls_dbf.free_result(rls_db, result);
		goto error;
	}
	if(result== NULL)
		goto error;
	n= result->n;
	rls_dbf.free_result(rls_db, result);

	if(n<= 0)
	{
		if(rls_dbf.insert(rls_db, query_cols, query_vals, n_query_cols)< 0)
		{
			LM_ERR("in sql insert\n");
			goto error;
		}
		LM_DBG("Inserted in database table new record\n");
	}
	else
	{
		LM_DBG("Updated in db table already existing record\n");
		if(rls_dbf.update(rls_db, query_cols, 0, query_vals, query_cols+2,
						query_vals+2, 2, n_query_cols-2)< 0)
		{
			LM_ERR("in sql update\n");
			goto error;
		}
	}
	
	LM_DBG("Updated rlpres_table\n");	
	/* reply 200OK */
done:
	if( slb.reply(msg, 200, &su_200_rpl)== -1)
	{
		LM_ERR("while sending reply\n");
		goto error;
	}	

	return 1;

error:
	return -1;
}
/* callid, from_tag, to_tag parameters must be allocated */

int parse_rlsubs_did(char* str_did, str* callid, str* from_tag, str* to_tag)
{
	char* smc= NULL;

	smc= strstr(str_did, DID_SEP);
	if(smc== NULL)
	{
		LM_ERR("bad format for resource list Subscribe dialog"
				" indentifier[rlsubs did]= %s\n", str_did);
		return -1;
	}
	callid->s= str_did;
	callid->len= smc- str_did;
			
	from_tag->s= smc+ DID_SEP_LEN;
	smc= strstr(from_tag->s, DID_SEP);
	if(smc== NULL)
	{
		LM_ERR("bad format for resource list Subscribe dialog"
				" indentifier(rlsubs did)= %s\n", str_did);
		return -1;
	}
	from_tag->len= smc- from_tag->s;
		
	to_tag->s= smc+ DID_SEP_LEN;
	to_tag->len= strlen(str_did)- 2* DID_SEP_LEN- callid->len- from_tag->len;

	return 0;
}

void timer_send_notify(unsigned int ticks,void *param)
{
	db_key_t query_cols[2], update_cols[1], result_cols[7];
	db_val_t query_vals[2], update_vals[1];
	int did_col, resource_uri_col, auth_state_col, reason_col,
		pres_state_col, content_type_col;
	int n_result_cols= 0, i;
	db_res_t *result= NULL;
	char* prev_did= NULL, * curr_did= NULL;
	db_row_t *row;	
	db_val_t *row_vals;
	char* resource_uri, *pres_state;
	str callid, to_tag, from_tag;
	xmlDocPtr rlmi_doc= NULL;
	xmlNodePtr list_node= NULL, instance_node= NULL, resource_node;
	unsigned int hash_code= 0;
	int len;
	int size= BUF_REALLOC_SIZE, buf_len= 0;	
	char* buf= NULL, *auth_state= NULL, *boundary_string= NULL, *cid= NULL;
	int contor= 0, auth_state_flag, antet_len;
	str bstr= {0, 0};
	str rlmi_cont= {0, 0}, multi_cont;
	subs_t* s, *dialog= NULL;
	char* rl_uri= NULL;

	query_cols[0]= "updated";
	query_vals[0].type = DB_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val= UPDATED_TYPE; 

	result_cols[did_col= n_result_cols++]= "rlsubs_did";
	result_cols[resource_uri_col= n_result_cols++]= "resource_uri";
	result_cols[auth_state_col= n_result_cols++]= "auth_state";
	result_cols[content_type_col= n_result_cols++]= "content_type";
	result_cols[reason_col= n_result_cols++]= "reason";
	result_cols[pres_state_col= n_result_cols++]= "presence_state";

	/* query in alfabetical order after rlsusbs_did 
	 * (resource list Subscribe dialog indentifier)*/

	if (rls_dbf.use_table(rls_db, rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto done;
	}

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					1, n_result_cols, "rlsubs_did", &result)< 0)
	{
		LM_ERR("in sql query\n");
		goto done;
	}
	if(result== NULL || result->n<= 0)
		goto done;

	/* generate the boundary string */

	boundary_string= generate_string((int)time(NULL), BOUNDARY_STRING_LEN);
	bstr.len= strlen(boundary_string);
	bstr.s= (char*)pkg_malloc((bstr.len+ 1)* sizeof(char));
	if(bstr.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(bstr.s, boundary_string, bstr.len);
	bstr.s[bstr.len]= '\0';

	/* for the multipart body , use here also an initial allocated
	 * and reallocated on need buffer */
	buf= pkg_malloc(size* sizeof(char));
	if(buf== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	antet_len= COMPUTE_ANTET_LEN(bstr.s);
	LM_DBG("found %d records with updated state\n", result->n);
	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		
		curr_did=     (char*)row_vals[did_col].val.string_val;
		resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
		auth_state_flag=     row_vals[auth_state_col].val.int_val;
		pres_state=   (char*)row_vals[pres_state_col].val.string_val;
		
		if(prev_did!= NULL && strcmp(prev_did, curr_did)) 
		{
			xmlDocDumpFormatMemory(rlmi_doc,(xmlChar**)(void*)&rlmi_cont.s,
				&rlmi_cont.len, 1);
		
			multi_cont.s= buf;
			multi_cont.len= buf_len;

			 if(agg_body_sendn_update(&dialog->pres_uri, bstr.s, &rlmi_cont, 
						 (buf_len==0)?NULL:&multi_cont, dialog, hash_code)<0)
			 {
				 LM_ERR("in function agg_body_sendn_update\n");
				 goto error;
			 }
			xmlFree(rlmi_cont.s);
			if(buf_len)		
			{	
				pkg_free(buf);
				buf= NULL;
			}
			xmlFreeDoc(rlmi_doc);
			rlmi_doc= NULL;
			pkg_free(rl_uri);
			rl_uri= NULL;
			pkg_free(dialog);
			dialog= NULL;
		}

		if(prev_did== NULL || strcmp(prev_did, curr_did)) /*if first or different*/
		{
			/* search the subscription in rlsubs_table*/		
			if( parse_rlsubs_did(curr_did, &callid, &from_tag, &to_tag)< 0)
			{
				LM_ERR("bad format for "
					"resource list Subscribe dialog indentifier(rlsubs did)\n");
				goto done;

			}
			hash_code= core_hash(&callid, &to_tag, hash_size);
		
			lock_get(&rls_table[hash_code].lock);
			s= pres_search_shtable(rls_table,callid,to_tag,from_tag,hash_code);
			if(s== NULL)
			{
				LM_DBG("record not found in hash_table [rlsubs_did]= %s\n",
						curr_did);
				LM_DBG("callid= %.*s\tfrom_tag= %.*s\tto_tag= %.*s\n",
						callid.len, callid.s,from_tag.len,from_tag.s,
						to_tag.len,to_tag.s);
				lock_release(&rls_table[hash_code].lock);
				continue;
			}
			LM_DBG("Found rl-subs record in hash table\n");
					
			/* save dialog info and rl_uri*/
			dialog= pres_copy_subs(s, PKG_MEM_TYPE);
			if(dialog== NULL)
			{	
				LM_ERR("while copying subs_t structure\n");
				lock_release(&rls_table[hash_code].lock);
				goto done;
			}	
			lock_release(&rls_table[hash_code].lock);

			/* make new rlmi and multipart documents */
			rlmi_doc= xmlNewDoc(BAD_CAST "1.0");
			if(rlmi_doc== NULL)
			{
				LM_ERR("when creating new xml doc\n");
				goto done;
			}
			list_node= xmlNewNode(NULL, BAD_CAST "list");
			if(list_node== NULL)
			{
				LM_ERR("while creating new xml node\n");
				goto done;
			}
			rl_uri= (char*)pkg_malloc((dialog->pres_uri.len+ 1)* sizeof(char));
			if(rl_uri==  NULL)
			{
				ERR_MEM(PKG_MEM_STR);
			}
			memcpy(rl_uri, dialog->pres_uri.s, dialog->pres_uri.len);
			rl_uri[dialog->pres_uri.len]= '\0';

			xmlNewProp(list_node, BAD_CAST "uri", BAD_CAST rl_uri);
			xmlNewProp(list_node, BAD_CAST "xmlns", BAD_CAST "urn:ietf:params:xml:ns:rlmi");
			xmlNewProp(list_node, BAD_CAST "version", BAD_CAST int2str(dialog->version, &len));
			xmlNewProp(list_node, BAD_CAST "fullState", BAD_CAST "false");

			xmlDocSetRootElement(rlmi_doc, list_node);
			buf_len= 0;

			/* !!!! for now I will include the auth state without checking if 
			 * it has changed - > in future chech if it works */		
		}		

		/* add a node in rlmi_doc and if any presence state registered add 
		 * it in the buffer */
		
		resource_node= xmlNewChild(list_node,NULL,BAD_CAST "resource", NULL);
		if(resource_node== NULL)
		{
			LM_ERR("when adding resource child\n");
			goto done;
		}
		xmlNewProp(resource_node, BAD_CAST "uri", BAD_CAST resource_uri);
			
		/* there might be more records with the same uri- more instances-
		 * search and add them all */
		
		contor= 0;
		while(1)
		{
			contor++;
			cid= NULL;
			instance_node= xmlNewChild(resource_node, NULL, 
					BAD_CAST "instance", NULL);
			if(instance_node== NULL)
			{
				LM_ERR("while adding instance child\n");
				goto error;
			}
			xmlNewProp(instance_node, BAD_CAST "id", 
					BAD_CAST generate_string(contor, 8));
			
			auth_state= get_auth_string(auth_state_flag);
			if(auth_state== NULL)
			{
				LM_ERR("bad authorization status flag\n");
				goto error;
			}
			xmlNewProp(instance_node, BAD_CAST "state", BAD_CAST auth_state);
		
			if(auth_state_flag & ACTIVE_STATE)
			{
				cid= generate_cid(resource_uri, strlen(resource_uri));
				xmlNewProp(instance_node, BAD_CAST "cid", BAD_CAST cid);
			}
			else
			if(auth_state_flag & TERMINATED_STATE)
			{
				xmlNewProp(instance_node, BAD_CAST "reason",
					BAD_CAST row_vals[resource_uri_col].val.string_val);
			}
		
			/* add in the multipart buffer */
			if(cid)
			{
				if(buf_len+ antet_len+ strlen(pres_state)+ 4 > size)
				{
					REALLOC_BUF
				}
				buf_len+= sprintf(buf+ buf_len, "--%s\r\n\r\n", bstr.s);
				buf_len+= sprintf(buf+ buf_len, "Content-Transfer-Encoding: binary\r\n");
				buf_len+= sprintf(buf+ buf_len, "Content-ID: <%s>\r\n", cid);
				buf_len+= sprintf(buf+ buf_len, "Content-Type: %s\r\n\r\n",  
						row_vals[content_type_col].val.string_val);
				buf_len+= sprintf(buf+buf_len,"%s\r\n\r\n", pres_state);
			}

			i++;
			if(i== result->n)
			{
				i--;
				break;
			}

			row = &result->rows[i];
			row_vals = ROW_VALUES(row);
		
			if(strncmp(row_vals[resource_uri_col].val.string_val,
					resource_uri, strlen(resource_uri)))
			{
				i--;
				break;
			}
			resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
			auth_state_flag=     row_vals[auth_state_col].val.int_val;
			pres_state=   (char*)row_vals[pres_state_col].val.string_val;
		}

		prev_did= curr_did;
	}

	if(rlmi_doc)
	{
		xmlDocDumpFormatMemory( rlmi_doc,(xmlChar**)(void*)&rlmi_cont.s,
		&rlmi_cont.len, 1);
		
		multi_cont.s= buf;
		multi_cont.len= buf_len;
	
		 if(agg_body_sendn_update(&dialog->pres_uri, bstr.s, &rlmi_cont, 
			 (buf_len==0)?NULL:&multi_cont, dialog, hash_code)<0)
		 {
			 LM_ERR("in function agg_body_sendn_update\n");
			 goto error;
		}
		xmlFree(rlmi_cont.s);
		if(buf_len)		
		{	
			pkg_free(buf);
			buf= NULL;
		}
		pkg_free(rl_uri);
		rl_uri= NULL;
		pkg_free(dialog);
		dialog= NULL;
	}

	/* update the rlpres table */
	update_cols[0]= "updated";
	update_vals[0].type = DB_INT;
	update_vals[0].nul = 0;
	update_vals[0].val.int_val= NO_UPDATE_TYPE; 

	if (rls_dbf.use_table(rls_db, rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	if(rls_dbf.update(rls_db, query_cols, 0, query_vals, update_cols,
					update_vals, 1, 1)< 0)
	{
		LM_ERR("in sql update\n");
		goto error;
	}


error:
done:
	if(result)
		rls_dbf.free_result(rls_db, result);
	if(rlmi_doc)
		xmlFreeDoc(rlmi_doc);
	if(rl_uri)
		pkg_free(rl_uri);
	if(bstr.s)
		pkg_free(bstr.s);
		
	if(buf)
		pkg_free(buf);
	if(dialog)
		pkg_free(dialog);
	return;
}


/* function to periodicaly clean the rls_presentity table */

void rls_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t query_cols[2];
	db_op_t query_ops[2];
	db_val_t query_vals[2];

	query_cols[0]= "expires";
	query_ops[0]= OP_LT;
	query_vals[0].nul= 0;
	query_vals[0].type= DB_INT;
	query_vals[0].val.int_val= (int)time(NULL);

	if (rls_dbf.use_table(rls_db, rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return ;
	}

	if(rls_dbf.delete(rls_db, query_cols, query_ops, query_vals, 1)< 0)
	{
		LM_ERR("in sql delete\n");
		return ;
	}

}
