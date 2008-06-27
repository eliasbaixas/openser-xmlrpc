/**
 * $Id: items.c 2715 2007-09-05 15:31:50Z miconda $
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 Voice Sistem SRL
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
 * 2004-10-20 - added header name specifier (ramona)
 * 2005-06-14 - added avp name specifier (ramona)
 * 2005-06-18 - added color printing support via escape sequesnces
 *              contributed by Ingo Flaschberger (daniel)
 * 2005-06-22 - created this file from modules/xlog/pv_lib.c (daniel)
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "dprint.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "ut.h" 
#include "trim.h" 
#include "dset.h"
#include "action.h"
#include "socket_info.h"
#include "route_struct.h"
#include "usr_avp.h"
#include "errinfo.h"
#include "transformations.h"
#include "script_var.h"
#include "pvar.h"

#include "parser/parse_from.h"
#include "parser/parse_uri.h"
#include "parser/parse_hname2.h"
#include "parser/parse_content.h"
#include "parser/parse_refer_to.h"
#include "parser/parse_rpid.h"
#include "parser/parse_diversion.h"
#include "parser/parse_ppi.h"
#include "parser/parse_pai.h"
#include "parser/digest/digest.h"

#define is_in_str(p, in) (p<in->s+in->len && *p)

typedef struct _pv_extra
{
	pv_export_t pve;
	struct _pv_extra *next;
} pv_extra_t, *pv_extra_p;

pv_extra_p  *_pv_extra_list=0;

static str str_marker = { PV_MARKER_STR, 1 };
static str str_null   = { "<null>", 6 };
static str str_empty  = { "", 0 };
static str str_udp    = { "UDP", 3 };
static str str_5060   = { "5060", 4 };

unsigned int _pv_msg_id = 0;
time_t _pv_msg_tm = 0;
int _pv_pid = 0;

#define PV_FIELD_DELIM ", "
#define PV_FIELD_DELIM_LEN (sizeof(PV_FIELD_DELIM) - 1)

#define PV_LOCAL_BUF_SIZE	511
static char pv_local_buf[PV_LOCAL_BUF_SIZE+1];


static int pv_get_marker(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->rs = str_marker;
	res->ri = (int)str_marker.s[0];
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

int pv_get_null(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->rs = str_empty;
	res->ri = 0;
	res->flags = PV_VAL_NULL;
	return 0;
}
static int pv_get_udp(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->rs = str_udp;
	res->ri = PROTO_UDP;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_5060(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->rs = str_5060;
	res->ri = 5060;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}


/************************************************************/
static int pv_get_pid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if(_pv_pid == 0)
		_pv_pid = (int)getpid();
	ch = int2str(_pv_pid, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = _pv_pid;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}


extern int return_code;
static int pv_get_return_code(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *s = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	s = sint2str(return_code, &l);

	res->rs.s = s;
	res->rs.len = l;

	res->ri = return_code;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

static int pv_get_times(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;
		
	if(msg==NULL || res==NULL)
		return -1;

	if(_pv_msg_id != msg->id || _pv_msg_tm==0)
	{
		_pv_msg_tm = time(NULL);
		_pv_msg_id = msg->id;
	}
	ch = int2str(_pv_msg_tm, &l);
	
	res->rs.s = ch;
	res->rs.len = l;

	res->ri = (int)_pv_msg_tm;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

static int pv_get_timef(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	char *ch = NULL;
	
	if(msg==NULL || res==NULL)
		return -1;
	if(_pv_msg_id != msg->id || _pv_msg_tm==0)
	{
		_pv_msg_tm = time(NULL);
		_pv_msg_id = msg->id;
	}
	
	ch = ctime(&_pv_msg_tm);
	
	res->rs.s = ch;
	res->rs.len = strlen(ch)-1;

	res->ri = (int)_pv_msg_tm;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_msgid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->id, &l);
	res->rs.s = ch;
	res->rs.len = l;

	res->ri = (int)msg->id;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

static int pv_get_method(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST)
	{
		res->rs.s = msg->first_line.u.request.method.s;
		res->rs.len = msg->first_line.u.request.method.len;
		res->ri = (int)msg->first_line.u.request.method_value;
	} else {
		if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) || 
				(msg->cseq==NULL)))
		{
			LM_DBG("no CSEQ header\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s = get_cseq(msg)->method.s;
		res->rs.len = get_cseq(msg)->method.len;
		res->ri = get_cseq(msg)->method_id;
	}
	
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_status(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->rs.s = msg->first_line.u.reply.status.s;
		res->rs.len = msg->first_line.u.reply.status.len;		
	}
	else
		return pv_get_null(msg, param, res);
	
	res->ri = (int)msg->first_line.u.reply.statuscode;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

static int pv_get_reason(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->rs.s = msg->first_line.u.reply.reason.s;
		res->rs.len = msg->first_line.u.reply.reason.len;		
	}
	else
		return pv_get_null(msg, param, res);
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_ruri(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return pv_get_null(msg, param, res);

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse the R-URI\n");
		return pv_get_null(msg, param, res);
	}
	
	if (msg->new_uri.s!=NULL)
	{
		res->rs.s   = msg->new_uri.s;
		res->rs.len = msg->new_uri.len;
	} else {
		res->rs.s   = msg->first_line.u.request.uri.s;
		res->rs.len = msg->first_line.u.request.uri.len;
	}
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_ouri(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return pv_get_null(msg, param, res);

	if(msg->parsed_orig_ruri_ok==0
			/* orig R-URI not parsed*/ && parse_orig_ruri(msg)<0)
	{
		LM_ERR("failed to parse the R-URI\n");
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s   = msg->first_line.u.request.uri.s;
	res->rs.len = msg->first_line.u.request.uri.len;
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_xuri_attr(struct sip_msg *msg, struct sip_uri *parsed_uri,
		pv_param_t *param, pv_value_t *res)
{
	if(param->pvn.u.isname.name.n==1) /* username */
	{
		if(parsed_uri->user.s==NULL || parsed_uri->user.len<=0)
			return pv_get_null(msg, param, res);
		res->rs.s   = parsed_uri->user.s;
		res->rs.len = parsed_uri->user.len;
		res->flags = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==2) /* domain */ {
		if(parsed_uri->host.s==NULL || parsed_uri->host.len<=0)
			return pv_get_null(msg, param, res);
		res->rs.s   = parsed_uri->host.s;
		res->rs.len = parsed_uri->host.len;
		res->flags  = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==3) /* port */ {
		if(parsed_uri->port.s==NULL)
			return pv_get_5060(msg, param, res);
		res->rs.s   = parsed_uri->port.s;
		res->rs.len = parsed_uri->port.len;
		res->ri     = (int)parsed_uri->port_no;
		res->flags  = PV_VAL_STR|PV_VAL_INT;
	} else if(param->pvn.u.isname.name.n==4) /* protocol */ {
		if(parsed_uri->transport_val.s==NULL)
			return pv_get_udp(msg, param, res);
		res->rs.s   = parsed_uri->transport_val.s;
		res->rs.len = parsed_uri->transport_val.len;
		res->ri     = (int)parsed_uri->proto;
		res->flags  = PV_VAL_STR|PV_VAL_INT;
	} else {
		LM_ERR("unknown specifier\n");
		return pv_get_null(msg, param, res);
	}
	
	return 0;
}

static int pv_get_ruri_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return pv_get_null(msg, param, res);

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse the R-URI\n");
		return pv_get_null(msg, param, res);
	}
	return pv_get_xuri_attr(msg, &(msg->parsed_uri), param, res);
}	

static int pv_get_ouri_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return pv_get_null(msg, param, res);

	if(msg->parsed_orig_ruri_ok==0
			/* orig R-URI not parsed*/ && parse_orig_ruri(msg)<0)
	{
		LM_ERR("failed to parse the R-URI\n");
		return pv_get_null(msg, param, res);
	}
	return pv_get_xuri_attr(msg, &(msg->parsed_orig_ruri), param, res);
}	

	
static int pv_get_contact(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->contact==NULL && parse_headers(msg, HDR_CONTACT_F, 0)==-1) 
	{
		LM_DBG("no contact header\n");
		return pv_get_null(msg, param, res);
	}
	
	if(!msg->contact || !msg->contact->body.s || msg->contact->body.len<=0)
    {
		LM_DBG("no contact header!\n");
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s = msg->contact->body.s;
	res->rs.len = msg->contact->body.len;

//	res->s = ((struct to_body*)msg->contact->parsed)->uri.s;
//	res->len = ((struct to_body*)msg->contact->parsed)->uri.len;

	res->flags = PV_VAL_STR;
	return 0;
}

extern err_info_t _oser_err_info;
static int pv_get_errinfo_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if(param->pvn.u.isname.name.n==0) /* class */ {
		ch = int2str(_oser_err_info.eclass, &l);
		res->rs.s = ch;
		res->rs.len = l;
		res->ri = (int)_oser_err_info.eclass;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	} else if(param->pvn.u.isname.name.n==1) /* level */ {
		ch = int2str(_oser_err_info.level, &l);
		res->rs.s = ch;
		res->rs.len = l;
		res->ri = (int)_oser_err_info.level;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	} else if(param->pvn.u.isname.name.n==2) /* info */ {
		if(_oser_err_info.info.s==NULL)
			pv_get_null(msg, param, res);
		res->rs.s = _oser_err_info.info.s;
		res->rs.len = _oser_err_info.info.len;
		res->flags = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==3) /* rcode */ {
		ch = int2str(_oser_err_info.rcode, &l);
		res->rs.s = ch;
		res->rs.len = l;
		res->ri = (int)_oser_err_info.rcode;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	} else if(param->pvn.u.isname.name.n==4) /* rreason */ {
		if(_oser_err_info.rreason.s==NULL)
			pv_get_null(msg, param, res);
		res->rs.s   = _oser_err_info.rreason.s;
		res->rs.len = _oser_err_info.rreason.len;
		res->flags = PV_VAL_STR;
	} else {
		LM_DBG("invalid attribute!\n");
		return pv_get_null(msg, param, res);
	}
	return 0;
}

static int pv_get_from_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct sip_uri *uri;
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse From header\n");
		return pv_get_null(msg, param, res);
	}
	
	if(msg->from==NULL || get_from(msg)==NULL) {
		LM_DBG("no From header\n");
		return pv_get_null(msg, param, res);
	}

	if(param->pvn.u.isname.name.n==1) /* uri */
	{
		res->rs.s = get_from(msg)->uri.s;
		res->rs.len = get_from(msg)->uri.len; 
		res->flags = PV_VAL_STR;
		return 0;
	}
	
	if(param->pvn.u.isname.name.n==4) /* tag */ {
	    if(get_from(msg)->tag_value.s==NULL||get_from(msg)->tag_value.len<=0) {
		        LM_DBG("no From tag\n");
			return pv_get_null(msg, param, res);
	    }
		res->rs.s = get_from(msg)->tag_value.s;
		res->rs.len = get_from(msg)->tag_value.len; 
		res->flags = PV_VAL_STR;
		return 0;
	}

	if(param->pvn.u.isname.name.n==5) /* display name */ {
	        if(get_from(msg)->display.s==NULL ||
		   get_from(msg)->display.len<=0) {
		        LM_DBG("no From display name\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s = get_from(msg)->display.s;
		res->rs.len = get_from(msg)->display.len; 
		res->flags = PV_VAL_STR;
		return 0;
	}

	if((uri=parse_from_uri(msg))==NULL) {
		LM_ERR("cannot parse From URI\n");
		return pv_get_null(msg, param, res);
	}
	if(param->pvn.u.isname.name.n==2) /* username */ {
	    if(uri->user.s==NULL || uri->user.len<=0) {
		    LM_DBG("no From username\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s   = uri->user.s;
		res->rs.len = uri->user.len; 
		res->flags = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==3) /* domain */ {
	    if(uri->host.s==NULL || uri->host.len<=0) {
		    LM_DBG("no From domain\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s   = uri->host.s;
		res->rs.len = uri->host.len; 
		res->flags = PV_VAL_STR;
	} else {
		LM_ERR("unknown specifier\n");
		return pv_get_null(msg, param, res);
	}
	return 0;
	}


static int pv_get_to_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct sip_uri *uri;
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->to==NULL && parse_headers(msg, HDR_TO_F, 0)==-1)
	{
		LM_ERR("cannot parse To header\n");
		return pv_get_null(msg, param, res);
	}
	if(msg->to==NULL || get_to(msg)==NULL) {
		LM_DBG("no To header\n");
		return pv_get_null(msg, param, res);
	}

	if(param->pvn.u.isname.name.n==1) /* uri */
	{
		res->rs.s = get_to(msg)->uri.s;
		res->rs.len = get_to(msg)->uri.len; 
		res->flags = PV_VAL_STR;
		return 0;
	}
	
	if(param->pvn.u.isname.name.n==4) /* tag */
	{
		if (get_to(msg)->tag_value.s==NULL ||
		    get_to(msg)->tag_value.len<=0) {
		        LM_DBG("no To tag\n");
		        return pv_get_null(msg, param, res);
		}
		res->rs.s = get_to(msg)->tag_value.s;
		res->rs.len = get_to(msg)->tag_value.len;
		res->flags = PV_VAL_STR;
		return 0;
	}

	if(param->pvn.u.isname.name.n==5) /* display name */ {
		if(get_to(msg)->display.s==NULL || 
		   get_to(msg)->display.len<=0) {
		        LM_DBG("no To display name\n");
		        return pv_get_null(msg, param, res);
		}
		res->rs.s = get_to(msg)->display.s;
		res->rs.len = get_to(msg)->display.len; 
		res->flags = PV_VAL_STR;
		return 0;
	}

	if((uri=parse_to_uri(msg))==NULL) {
		LM_ERR("cannot parse To URI\n");
		return pv_get_null(msg, param, res);
	}
	if(param->pvn.u.isname.name.n==2) /* username */ {
	    if(uri->user.s==NULL || uri->user.len<=0) {
		    LM_DBG("no To username\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s   = uri->user.s;
		res->rs.len = uri->user.len; 
		res->flags = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==3) /* domain */ {
	    if(uri->host.s==NULL || uri->host.len<=0) {
		    LM_DBG("no To domain\n");
			return pv_get_null(msg, param, res);
		}
		res->rs.s   = uri->host.s;
		res->rs.len = uri->host.len; 
		res->flags = PV_VAL_STR;
	} else {
		LM_ERR("unknown specifier\n");
		return pv_get_null(msg, param, res);
	}
	return 0;
}

static int pv_get_cseq(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) || 
				(msg->cseq==NULL)) )
	{
		LM_ERR("cannot parse CSEQ header\n");
		return pv_get_null(msg, param, res);
	}

	res->rs.s = get_cseq(msg)->number.s;
	res->rs.len = get_cseq(msg)->number.len;

	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_msg_buf(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->rs.s = msg->buf;
	res->rs.len = msg->len;

	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_msg_len(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->rs.s = int2str(msg->len, &res->rs.len);

	res->ri = (int)msg->len;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_flags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->rs.s = int2str(msg->flags, &res->rs.len);

	res->ri = (int)msg->flags;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static inline char* int_to_8hex(int val)
{
	unsigned short digit;
	int i;
	static char outbuf[9];
	
	outbuf[8] = '\0';
	for(i=0; i<8; i++)
	{
		if(val!=0)
		{
			digit =  val & 0x0f;
			outbuf[7-i] = digit >= 10 ? digit + 'a' - 10 : digit + '0';
			val >>= 4;
		}
		else
			outbuf[7-i] = '0';
	}
	return outbuf;
}

static int pv_get_hexflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->rs.s = int_to_8hex(msg->flags);
	res->rs.len = 8;

	res->ri = (int)msg->flags;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_bflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(res==NULL)
		return -1;

	res->ri = (int)getb0flags();
	res->rs.s = int2str( res->ri, &res->rs.len);

	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_hexbflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(res==NULL)
		return -1;

	res->ri = (int)getb0flags();
	res->rs.s = int_to_8hex(res->ri);
	res->rs.len = 8;

	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_sflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(res==NULL)
		return -1;

	res->ri = (int)getsflags();
	res->rs.s = int2str( res->ri, &res->rs.len);

	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_hexsflags(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(res==NULL)
		return -1;

	res->ri = (int)getsflags();
	res->rs.s = int_to_8hex(res->ri);
	res->rs.len = 8;

	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_callid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LM_ERR("cannot parse Call-Id header\n");
		return pv_get_null(msg, param, res);
	}

	res->rs.s = msg->callid->body.s;
	res->rs.len = msg->callid->body.len;
	trim(&res->rs);

	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_srcip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->rs.s = ip_addr2a(&msg->rcv.src_ip);
	res->rs.len = strlen(res->rs.s);
   
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_srcport(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->rcv.src_port, &l);
	res->rs.s = ch;
	res->rs.len = l;
   
	res->ri = (int)msg->rcv.src_port;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_rcvip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->rcv.bind_address==NULL 
			|| msg->rcv.bind_address->address_str.s==NULL)
		return pv_get_null(msg, param, res);
	
	res->rs.s   = msg->rcv.bind_address->address_str.s;
	res->rs.len = msg->rcv.bind_address->address_str.len;
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_rcvport(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->rcv.bind_address==NULL 
			|| msg->rcv.bind_address->port_no_str.s==NULL)
		return pv_get_null(msg, param, res);
	
	res->rs.s   = msg->rcv.bind_address->port_no_str.s;
	res->rs.len = msg->rcv.bind_address->port_no_str.len;
	
	res->ri = (int)msg->rcv.bind_address->port_no;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

static int pv_get_force_sock(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if (msg->force_send_socket==0)
		return pv_get_null(msg, param, res);

	res->rs = msg->force_send_socket->sock_str;
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_useragent(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL) 
		return -1;
	if(msg->user_agent==NULL && ((parse_headers(msg, HDR_USERAGENT_F, 0)==-1)
			 || (msg->user_agent==NULL)))
	{
		LM_DBG("no User-Agent header\n");
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s = msg->user_agent->body.s;
	res->rs.len = msg->user_agent->body.len;
	trim(&res->rs);
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_refer_to(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_refer_to_header(msg)==-1)
	{
		LM_DBG("no Refer-To header\n");
		return pv_get_null(msg, param, res);
	}
	
	if(msg->refer_to==NULL || get_refer_to(msg)==NULL)
		return pv_get_null(msg, param, res);

	res->rs.s = get_refer_to(msg)->uri.s;
	res->rs.len = get_refer_to(msg)->uri.len; 
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_diversion(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_diversion_header(msg)==-1)
	{
		LM_DBG("no Diversion header\n");
		return pv_get_null(msg, param, res);
	}
	
	if(msg->diversion==NULL || get_diversion(msg)==NULL) {
		LM_DBG("no Diversion header\n");
		return pv_get_null(msg, param, res);
	}

	res->rs.s = get_diversion(msg)->uri.s;
	res->rs.len = get_diversion(msg)->uri.len; 
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_rpid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_rpid_header(msg)==-1)
	{
		LM_DBG("no RPID header\n");
		return pv_get_null(msg, param, res);
	}
	
	if(msg->rpid==NULL || get_rpid(msg)==NULL)
		return pv_get_null(msg, param, res);

	res->rs.s = get_rpid(msg)->uri.s;
	res->rs.len = get_rpid(msg)->uri.len; 
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_ppi_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
    struct sip_uri *uri;
    
    if(msg==NULL || res==NULL)
	return -1;

    if(parse_ppi_header(msg) < 0) {
	LM_DBG("no P-Preferred-Identity header\n");
	return pv_get_null(msg, param, res);
    }
	
    if(msg->ppi == NULL || get_ppi(msg) == NULL) {
	       LM_DBG("no P-Preferred-Identity header\n");
		return pv_get_null(msg, param, res);
    }
    
    if(param->pvn.u.isname.name.n == 1) { /* uri */
		res->rs.s = get_ppi(msg)->uri.s;
		res->rs.len = get_ppi(msg)->uri.len; 
		res->flags = PV_VAL_STR;
		return 0;
    }
	
    if(param->pvn.u.isname.name.n==4) { /* display name */
	if(get_ppi(msg)->display.s == NULL ||
	   get_ppi(msg)->display.len <= 0) {
	    LM_DBG("no P-Preferred-Identity display name\n");
	    return pv_get_null(msg, param, res);
	}
	res->rs.s = get_ppi(msg)->display.s;
	res->rs.len = get_ppi(msg)->display.len; 
	res->flags = PV_VAL_STR;
	return 0;
    }

    if((uri=parse_ppi_uri(msg))==NULL) {
	LM_ERR("cannot parse P-Preferred-Identity URI\n");
	return pv_get_null(msg, param, res);
    }

    if(param->pvn.u.isname.name.n==2) { /* username */
	if(uri->user.s==NULL || uri->user.len<=0) {
	    LM_DBG("no P-Preferred-Identity username\n");
	    return pv_get_null(msg, param, res);
	}
	res->rs.s   = uri->user.s;
	res->rs.len = uri->user.len; 
	res->flags = PV_VAL_STR;
    } else if(param->pvn.u.isname.name.n==3) { /* domain */
	if(uri->host.s==NULL || uri->host.len<=0) {
	    LM_DBG("no P-Preferred-Identity domain\n");
	    return pv_get_null(msg, param, res);
	}
	res->rs.s   = uri->host.s;
	res->rs.len = uri->host.len; 
	res->flags = PV_VAL_STR;
    } else {
	LM_ERR("unknown specifier\n");
	return pv_get_null(msg, param, res);
    }

    return 0;
}

static int pv_get_pai(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
    if(msg==NULL || res==NULL)
	return -1;
    
    if(parse_pai_header(msg)==-1)
    {
	LM_DBG("no P-Asserted-Identity header\n");
	return pv_get_null(msg, param, res);
    }
	
    if(msg->pai==NULL || get_pai(msg)==NULL) {
	LM_DBG("no P-Asserted-Identity header\n");
	return pv_get_null(msg, param, res);
    }
    
    res->rs.s = get_pai(msg)->uri.s;
    res->rs.len = get_pai(msg)->uri.len; 
    
    res->flags = PV_VAL_STR;
    return 0;
}

/* proto of received message: $pr or $proto*/
static int pv_get_proto(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->ri = msg->rcv.proto;
	
	switch(msg->rcv.proto)
	{
		case PROTO_UDP:
			res->rs.s = "UDP";
			res->rs.len = 3;
		break;
		case PROTO_TCP:
			res->rs.s = "TCP";
			res->rs.len = 3;
		break;
		case PROTO_TLS:
			res->rs.s = "TLS";
			res->rs.len = 3;
		break;
		case PROTO_SCTP:
			res->rs.s = "SCTP";
			res->rs.len = 4;
		break;
		default:
			res->rs.s = "NONE";
			res->rs.len = 4;
	}

	
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}


static int pv_get_dset(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
    if(msg==NULL || res==NULL)
	return -1;
    
    res->rs.s = print_dset(msg, &res->rs.len);

    if ((res->rs.s) == NULL) return pv_get_null(msg, param, res);
    
    res->rs.len -= CRLF_LEN;

	res->flags = PV_VAL_STR;
    return 0;
}


static int pv_get_dsturi(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
    if(msg==NULL || res==NULL)
		return -1;
    
    if (msg->dst_uri.s == NULL) {
		LM_DBG("no destination URI\n");
		return pv_get_null(msg, param, res);
    }

    res->rs.s = msg->dst_uri.s;
    res->rs.len = msg->dst_uri.len;

    res->flags = PV_VAL_STR;
    return 0;
}

static int pv_get_dsturi_attr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct sip_uri uri;

        if(msg==NULL || res==NULL)
		return -1;
    
        if (msg->dst_uri.s == NULL) {
		LM_DBG("no destination URI\n");
		return pv_get_null(msg, param, res);
	}

	if(parse_uri(msg->dst_uri.s, msg->dst_uri.len, &uri)!=0)
	{
		LM_ERR("failed to parse dst uri\n");
		return pv_get_null(msg, param, res);
	}
	
	if(param->pvn.u.isname.name.n==1) /* domain */
	{
		if(uri.host.s==NULL || uri.host.len<=0)
			return pv_get_null(msg, param, res);
		res->rs.s = uri.host.s;
		res->rs.len = uri.host.len;
		res->flags = PV_VAL_STR;
	} else if(param->pvn.u.isname.name.n==2) /* port */ {
		if(uri.port.s==NULL)
			return pv_get_5060(msg, param, res);
		res->rs.s   = uri.port.s;
		res->rs.len = uri.port.len;
		res->ri     = (int)uri.port_no;
		res->flags  = PV_VAL_STR|PV_VAL_INT;
		return 0;
	} else if(param->pvn.u.isname.name.n==3) /* proto */ {
		if(uri.transport_val.s==NULL)
			return pv_get_udp(msg, param, res);
		res->rs.s   = uri.transport_val.s;
		res->rs.len = uri.transport_val.len;
		res->ri     = (int)uri.proto;
		res->flags  = PV_VAL_STR|PV_VAL_INT;
	} else {
		LM_ERR("invalid specifier\n");
		return pv_get_null(msg, param, res);
	}

    return 0;
}

static int pv_get_content_type(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL) 
		return -1;

	if(msg->content_type==NULL
			&& ((parse_headers(msg, HDR_CONTENTTYPE_F, 0)==-1)
			 || (msg->content_type==NULL)))
	{
		LM_DBG("no Content-Type header\n");
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s = msg->content_type->body.s;
	res->rs.len = msg->content_type->body.len;
	trim(&res->rs);
	
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_content_length(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL || res==NULL) 
		return -1;
	if(msg->content_length==NULL
			&& ((parse_headers(msg, HDR_CONTENTLENGTH_F, 0)==-1)
			 || (msg->content_length==NULL)))
	{
		LM_DBG("no Content-Length header\n");
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s = msg->content_length->body.s;
	res->rs.len = msg->content_length->body.len;
	trim(&res->rs);

	res->ri = (int)(long)msg->content_length->parsed;
	res->flags = PV_VAL_STR | PV_VAL_INT;

	return 0;
}

static int pv_get_msg_body(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
    if(msg==NULL || res==NULL)
	return -1;
    
    res->rs.s = get_body( msg );

    if ((res->rs.s) == NULL) {
		LM_DBG("no message body\n");
		return pv_get_null(msg, param, res);
    }    
    if (!msg->content_length) 
    {
	LM_ERR("no Content-Length header found\n");
	return pv_get_null(msg, param, res);
    }
    res->rs.len = get_content_length(msg);

    res->flags = PV_VAL_STR;
    return 0;
}

static int pv_get_authattr(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct hdr_field *hdr;
	
    if(msg==NULL || res==NULL)
		return -1;
    
	if ((msg->REQ_METHOD == METHOD_ACK) || 
	    (msg->REQ_METHOD == METHOD_CANCEL)) {
		LM_DBG("no [Proxy-]Authorization header\n");
		return pv_get_null(msg, param, res);
	}

	if ((parse_headers(msg, HDR_PROXYAUTH_F|HDR_AUTHORIZATION_F, 0)==-1)
			|| (msg->proxy_auth==0 && msg->authorization==0))
	{
		LM_DBG("no [Proxy-]Authorization header\n");
		return pv_get_null(msg, param, res);
	}

	hdr = (msg->proxy_auth==0)?msg->authorization:msg->proxy_auth;
	
	if(parse_credentials(hdr)!=0) {
	        LM_ERR("failed to parse credentials\n");
		return pv_get_null(msg, param, res);
	}
	switch(param->pvn.u.isname.name.n)
	{
		case 3:
			if(((auth_body_t*)(hdr->parsed))->digest.uri.len==0)
				return pv_get_null(msg, param, res);
			res->rs.s  =((auth_body_t*)(hdr->parsed))->digest.uri.s;
			res->rs.len=((auth_body_t*)(hdr->parsed))->digest.uri.len;
		break;
		case 2:
			res->rs.s  =((auth_body_t*)(hdr->parsed))->digest.realm.s;
			res->rs.len=((auth_body_t*)(hdr->parsed))->digest.realm.len;
		break;
		case 1:
		    res->rs.s  =((auth_body_t*)(hdr->parsed))->digest.username.user.s;
		    res->rs.len=((auth_body_t*)(hdr->parsed))->digest.username.user.len;
		break;
		default:
		    res->rs.s  =((auth_body_t*)(hdr->parsed))->digest.username.whole.s;
			res->rs.len=
				((auth_body_t*)(hdr->parsed))->digest.username.whole.len;
	}	    
	
	res->flags = PV_VAL_STR;
    return 0;
}

static inline str *cred_user(struct sip_msg *rq)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len) 
			return 0;
	return &cred->digest.username.user;
}


static inline str *cred_realm(struct sip_msg *rq)
{
	str* realm;
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred) return 0;
	realm = GET_REALM(&cred->digest);
	if (!realm->len || !realm->s) {
		return 0;
	}
	return realm;
}

static int pv_get_acc_username(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[MAX_URI_SIZE];
	str* user;
	str* realm;
	struct sip_uri puri;
	struct to_body* from;

	/* try to take it from credentials */
	user = cred_user(msg);
	if (user) {
		realm = cred_realm(msg);
		if (realm) {
			res->rs.len = user->len+1+realm->len;
			if (res->rs.len > MAX_URI_SIZE) {
				LM_ERR("uri too long\n");
				return pv_get_null(msg, param, res);
			}
			res->rs.s = buf;
			memcpy(res->rs.s, user->s, user->len);
			(res->rs.s)[user->len] = '@';
			memcpy(res->rs.s+user->len+1, realm->s, realm->len);
		} else {
			res->rs.len = user->len;
			res->rs.s = user->s;
		}
	} else {
		/* from from uri */
	    if(parse_from_header(msg)<0)
		{
		    LM_ERR("cannot parse FROM header\n");
		    return pv_get_null(msg, param, res);
		}
		if (msg->from && (from=get_from(msg)) && from->uri.len) {
			if (parse_uri(from->uri.s, from->uri.len, &puri) < 0 ) {
				LM_ERR("bad From URI\n");
				return pv_get_null(msg, param, res);
			}
			res->rs.len = puri.user.len + 1 + puri.host.len;
			if (res->rs.len > MAX_URI_SIZE) {
				LM_ERR("from URI too long\n");
				return pv_get_null(msg, param, res);
			}
			res->rs.s = buf;
			memcpy(res->rs.s, puri.user.s, puri.user.len);
			(res->rs.s)[puri.user.len] = '@';
			memcpy(res->rs.s + puri.user.len + 1, puri.host.s,
			       puri.host.len);
		} else {
			res->rs.len = 0;
			res->rs.s = 0;
		}
	}
	res->flags = PV_VAL_STR;
	return 0;
}

static int pv_get_branch(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str branch;
	qvalue_t q;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return pv_get_null(msg, param, res);


	branch.s = get_branch(0, &branch.len, &q, 0, 0, 0, 0);
	if (!branch.s) {
		return pv_get_null(msg, param, res);
	}
	
	res->rs.s = branch.s;
	res->rs.len = branch.len;

	res->flags = PV_VAL_STR;
	return 0;
}

#define Q_PARAM ">;q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)

static int pv_get_branches(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str uri;
	qvalue_t q;
	int len, cnt, i;
	unsigned int qlen;
	char *p, *qbuf;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return pv_get_null(msg, param, res);
  
	cnt = len = 0;

	while ((uri.s = get_branch(cnt, &uri.len, &q, 0, 0, 0, 0)))
	{
		cnt++;
		len += uri.len;
		if (q != Q_UNSPECIFIED)
		{
			len += 1 + Q_PARAM_LEN + len_q(q);
		}
	}

	if (cnt == 0)
		return pv_get_null(msg, param, res);   

	len += (cnt - 1) * PV_FIELD_DELIM_LEN;

	if (len + 1 > PV_LOCAL_BUF_SIZE)
	{
		LM_ERR("local buffer length exceeded\n");
		return pv_get_null(msg, param, res);
	}

	i = 0;
	p = pv_local_buf;

	while ((uri.s = get_branch(i, &uri.len, &q, 0, 0, 0, 0)))
	{
		if (i)
		{
			memcpy(p, PV_FIELD_DELIM, PV_FIELD_DELIM_LEN);
			p += PV_FIELD_DELIM_LEN;
		}

		if (q != Q_UNSPECIFIED)
		{
			*p++ = '<';
		}

		memcpy(p, uri.s, uri.len);
		p += uri.len;
		if (q != Q_UNSPECIFIED)
		{
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		}
		i++;
	}

	res->rs.s = &(pv_local_buf[0]);
	res->rs.len = len;

	res->flags = PV_VAL_STR;
	return 0;
}


/************************************************************/

/**
 *
 */
static int pv_get_avp(struct sip_msg *msg,  pv_param_t *param, pv_value_t *res)
{
	unsigned short name_type;
	int_str avp_name;
	int_str avp_value;
	struct usr_avp *avp;
	int_str avp_value0;
	struct usr_avp *avp0;
	int idx;
	int idxf;
	char *p;
	int n=0;

	if(msg==NULL || res==NULL || param==NULL)
		return -1;

	/* get the name */
	if(pv_get_avp_name(msg, param, &avp_name, &name_type)!=0)
	{
		LM_ERR("invalid name\n");
		return -1;
	}
	/* get the index */
	if(pv_get_spec_index(msg, param, &idx, &idxf)!=0)
	{
		LM_ERR("invalid index\n");
		return -1;
	}
	
	if ((avp=search_first_avp(name_type, avp_name, &avp_value, 0))==0)
		return pv_get_null(msg, param, res);
	res->flags = PV_VAL_STR;
	if(idxf==0 && idx==0)
	{
		if(avp->flags & AVP_VAL_STR)
		{
			res->rs = avp_value.s;
		} else {
			res->rs.s = int2str(avp_value.n, &res->rs.len);
			res->ri = avp_value.n;
			res->flags |= PV_VAL_INT|PV_TYPE_INT;
		}
		return 0;
	}
	if(idxf==PV_IDX_ALL)
	{
		p = pv_local_buf;
		do {
			if(p!=pv_local_buf)
			{
				if(p-pv_local_buf+PV_FIELD_DELIM_LEN+1>PV_LOCAL_BUF_SIZE)
				{
					LM_ERR("local buffer length exceeded\n");
					return pv_get_null(msg, param, res);
				}
				memcpy(p, PV_FIELD_DELIM, PV_FIELD_DELIM_LEN);
				p += PV_FIELD_DELIM_LEN;
			}
			if(avp->flags & AVP_VAL_STR)
			{
				res->rs = avp_value.s;
			} else {
				res->rs.s = int2str(avp_value.n, &res->rs.len);
			}
			
			if(p-pv_local_buf+res->rs.len+1>PV_LOCAL_BUF_SIZE)
			{
				LM_ERR("local buffer length exceeded!\n");
				return pv_get_null(msg, param, res);
			}
			memcpy(p, res->rs.s, res->rs.len);
			p += res->rs.len;
		} while ((avp=search_first_avp(name_type, avp_name,
						&avp_value, avp))!=0);
		*p = 0;
		res->rs.s = pv_local_buf;
		res->rs.len = p - pv_local_buf;
		return 0;

	}

	/* we have a numeric index */
	if(idx<0)
	{
		n = 1;
		avp0 = avp;
		while ((avp0=search_first_avp(name_type, avp_name,
						&avp_value0, avp0))!=0) n++;
		idx = -idx;
		if(idx>n)
		{
			LM_DBG("index out of range\n");
			return pv_get_null(msg, param, res);
		}
		idx = n - idx;
		if(idx==0)
		{
			if(avp->flags & AVP_VAL_STR)
			{
				res->rs = avp_value.s;
			} else {
				res->rs.s = int2str(avp_value.n, &res->rs.len);
				res->ri = avp_value.n;
				res->flags |= PV_VAL_INT|PV_TYPE_INT;
			}
			return 0;
		}
	}
	n=0;
	while(n<idx 
			&& (avp=search_first_avp(name_type, avp_name, &avp_value, avp))!=0)
		n++;

	if(avp!=0)
	{
		if(avp->flags & AVP_VAL_STR)
		{
			res->rs = avp_value.s;
		} else {
			res->rs.s = int2str(avp_value.n, &res->rs.len);
			res->ri = avp_value.n;
			res->flags |= PV_VAL_INT|PV_TYPE_INT;
		}
		return 0;
	}

	LM_DBG("index out of range\n");
	return pv_get_null(msg, param, res);
}

static int pv_get_hdr(struct sip_msg *msg,  pv_param_t *param, pv_value_t *res)
{
	int idx;
	int idxf;
	pv_value_t tv;
	struct hdr_field *hf;
	struct hdr_field *hf0;
	char *p;
	int n;

	if(msg==NULL || res==NULL || param==NULL)
		return -1;

	/* get the name */
	if(param->pvn.type == PV_NAME_PVAR)
	{
		if(pv_get_spec_name(msg, param, &tv)!=0 || (!(tv.flags&PV_VAL_STR)))
		{
			LM_ERR("invalid name\n");
			return -1;
		}
	} else {
		if(param->pvn.u.isname.type == AVP_NAME_STR)
		{
			tv.flags = PV_VAL_STR;
			tv.rs = param->pvn.u.isname.name.s;
		} else {
			tv.flags = 0;
			tv.ri = param->pvn.u.isname.name.n;
		}
	}
	/* we need to be sure we have parsed all headers */
	if(parse_headers(msg, HDR_EOH_F, 0)<0)
	{
		LM_ERR("error parsing headers\n");
		return pv_get_null(msg, param, res);
	}

	for (hf=msg->headers; hf; hf=hf->next)
	{
		if(tv.flags == 0)
		{
			if (tv.ri==hf->type)
				break;
		} else {
			if (hf->name.len==tv.rs.len 
					&& strncasecmp(hf->name.s, tv.rs.s, hf->name.len)==0)
				break;
		}
	}
	if(hf==NULL)
		return pv_get_null(msg, param, res);
	/* get the index */
	if(pv_get_spec_index(msg, param, &idx, &idxf)!=0)
	{
		LM_ERR("invalid index\n");
		return -1;
	}

	/* get the value */
	res->flags = PV_VAL_STR;
	if(idxf==0 && idx==0)
	{
		res->rs  = hf->body;
		return 0;
	}
	if(idxf==PV_IDX_ALL)
	{
		p = pv_local_buf;
		do {
			if(p!=pv_local_buf)
			{
				if(p-pv_local_buf+PV_FIELD_DELIM_LEN+1>PV_LOCAL_BUF_SIZE)
				{
					LM_ERR("local buffer length exceeded\n");
					return pv_get_null(msg, param, res);
				}
				memcpy(p, PV_FIELD_DELIM, PV_FIELD_DELIM_LEN);
				p += PV_FIELD_DELIM_LEN;
			}
			
			if(p-pv_local_buf+hf->body.len+1>PV_LOCAL_BUF_SIZE)
			{
				LM_ERR("local buffer length exceeded!\n");
				return pv_get_null(msg, param, res);
			}
			memcpy(p, hf->body.s, hf->body.len);
			p += hf->body.len;
			/* next hf */
			for (; hf; hf=hf->next)
			{
				if(tv.flags == 0)
				{
					if (tv.ri==hf->type)
						break;
				} else {
					if (hf->name.len==tv.rs.len 
						&& strncasecmp(hf->name.s, tv.rs.s, hf->name.len)==0)
					break;
				}
			}
		} while (hf);
		*p = 0;
		res->rs.s = pv_local_buf;
		res->rs.len = p - pv_local_buf;
		return 0;
	}

	/* we have a numeric index */
	hf0 = 0;
	if(idx<0)
	{
		n = 1;
		/* count headers */
		for (hf0=hf->next; hf0; hf0=hf0->next)
		{
			if(tv.flags == 0)
			{
				if (tv.ri==hf0->type)
					n++;
			} else {
				if (hf0->name.len==tv.rs.len 
					&& strncasecmp(hf0->name.s, tv.rs.s, hf0->name.len)==0)
				n++;
			}
		}
		idx = -idx;
		if(idx>n)
		{
			LM_DBG("index out of range\n");
			return pv_get_null(msg, param, res);
		}
		idx = n - idx;
		if(idx==0)
		{
			res->rs  = hf->body;
			return 0;
		}
	}
	n=0;
	while(n<idx)
	{
		for (hf0=hf->next; hf0; hf0=hf0->next)
		{
			if(tv.flags == 0)
			{
				if (tv.ri==hf0->type)
					n++;
			} else {
				if (hf0->name.len==tv.rs.len 
					&& strncasecmp(hf0->name.s, tv.rs.s, hf0->name.len)==0)
				n++;
			}
			if(n==idx)
				break;
		}
		if(hf0==NULL)
			break;
	}

	if(hf0!=0)
	{
		res->rs  = hf0->body;
		return 0;
	}

	LM_DBG("index out of range\n");
	return pv_get_null(msg, param, res);

}

static int pv_get_scriptvar(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	int ival = 0;
	char *sval = NULL;
	script_var_t *sv=NULL;
	
	if(msg==NULL || res==NULL)
		return -1;

	if(param==NULL || param->pvn.u.dname==0)
		return pv_get_null(msg, param, res);
	
	sv= (script_var_t*)param->pvn.u.dname;

	if(sv->v.flags&VAR_VAL_STR)
	{
		res->rs = sv->v.value.s;
		res->flags = PV_VAL_STR;
	} else {
		sval = sint2str(sv->v.value.n, &ival);

		res->rs.s = sval;
		res->rs.len = ival;

		res->ri = sv->v.value.n;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	}
	return 0;
}

/********* end PV get functions *********/

/********* start PV set functions *********/
int pv_set_avp(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int_str avp_name;
	int_str avp_val;
	int flags;
	unsigned short name_type;
	
	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(pv_get_avp_name(msg, param, &avp_name, &name_type)!=0)
	{
		LM_ALERT("BUG in getting dst AVP name\n");
		goto error;
	}
	if(val == NULL)
	{
		if(op == COLONEQ_T)
			destroy_avps(name_type, avp_name, 1);
		else
			destroy_avps(name_type, avp_name, 0);
		return 0;
	}
	if(op == COLONEQ_T)
		destroy_avps(name_type, avp_name, 1);
	flags = name_type;
	if(val->flags&PV_TYPE_INT)
	{
		avp_val.n = val->ri;
	} else {
		avp_val.s = val->rs;
		flags |= AVP_VAL_STR;
	}
	if (add_avp(flags, avp_name, avp_val)<0)
	{
		LM_ERR("error - cannot add AVP\n");
		goto error;
	}
	return 0;
error:
	return -1;
}

int pv_set_scriptvar(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int_str avp_val;
	int flags;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(param->pvn.u.dname==0)
	{
		LM_ERR("error - cannot find svar\n");
		goto error;
	}
	if(val == NULL)
	{
		avp_val.n = 0;
		set_var_value((script_var_t*)param->pvn.u.dname, &avp_val, 0);
		return 0;
	}
	flags = 0;
	if(val->flags&PV_TYPE_INT)
	{
		avp_val.n = val->ri;
	} else {
		avp_val.s = val->rs;
		flags |= VAR_VAL_STR;
	}
	if(set_var_value((script_var_t*)param->pvn.u.dname, &avp_val, flags)==NULL)
	{
		LM_ERR("error - cannot set svar [%.*s] \n",
				((script_var_t*)param->pvn.u.dname)->name.len,
				((script_var_t*)param->pvn.u.dname)->name.s);
		goto error;
	}
	return 0;
error:
	return -1;
}

int pv_set_dsturi(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	struct action  act;

	if(msg==NULL || param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
					
	if(val == NULL)
	{
		memset(&act, 0, sizeof(act));
		act.type = RESET_DSTURI_T;
		if (do_action(&act, msg)<0)
		{
			LM_ERR("error - do action failed)\n");
			goto error;
		}
		return 1;
	}
	if(!(val->flags&PV_VAL_STR))
	{
		LM_ERR("error - str value requred to set dst uri\n");
		goto error;
	}
	
	if(set_dst_uri(msg, &val->rs)!=0)
		goto error;

	return 0;
error:
	return -1;
}

int pv_set_ruri(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	struct action  act;
	char backup;

	if(msg==NULL || param==NULL || val==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(!(val->flags&PV_VAL_STR))
	{
		LM_ERR("str value required to set R-URI\n");
		goto error;
	}
	
	memset(&act, 0, sizeof(act));
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = val->rs.s;
	backup = val->rs.s[val->rs.len];
	val->rs.s[val->rs.len] = '\0';
	act.type = SET_URI_T;
	if (do_action(&act, msg)<0)
	{
		LM_ERR("do action failed\n");
		val->rs.s[val->rs.len] = backup;
		goto error;
	}
	val->rs.s[val->rs.len] = backup;

	return 0;
error:
	return -1;
}

int pv_set_ruri_user(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	struct action  act;
	char backup;

	if(msg==NULL || param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
					
	if(val == NULL)
	{
		memset(&act, 0, sizeof(act));
		act.type = SET_USER_T;
		act.elem[0].type = STRING_ST;
		act.elem[0].u.string = "";
		if (do_action(&act, msg)<0)
		{
			LM_ERR("do action failed)\n");
			goto error;
		}
		return 0;
	}

	if(!(val->flags&PV_VAL_STR))
	{
		LM_ERR("str value required to set R-URI user\n");
		goto error;
	}
	
	memset(&act, 0, sizeof(act));
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = val->rs.s;
	backup = val->rs.s[val->rs.len];
	val->rs.s[val->rs.len] = '\0';
	act.type = SET_USER_T;
	if (do_action(&act, msg)<0)
	{
		LM_ERR("do action failed\n");
		val->rs.s[val->rs.len] = backup;
		goto error;
	}
	val->rs.s[val->rs.len] = backup;

	return 0;
error:
	return -1;
}

int pv_set_ruri_host(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	struct action  act;
	char backup;

	if(msg==NULL || param==NULL || val==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(!(val->flags&PV_VAL_STR))
	{
		LM_ERR("str value required to set R-URI hostname\n");
		goto error;
	}
	
	memset(&act, 0, sizeof(act));
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = val->rs.s;
	backup = val->rs.s[val->rs.len];
	val->rs.s[val->rs.len] = '\0';
	act.type = SET_HOST_T;
	if (do_action(&act, msg)<0)
	{
		LM_ERR("do action failed\n");
		val->rs.s[val->rs.len] = backup;
		goto error;
	}
	val->rs.s[val->rs.len] = backup;

	return 0;
error:
	return -1;
}

int pv_set_branch(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	if(msg==NULL || param==NULL || val==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(!(val->flags&PV_VAL_STR) || val->rs.len<=0)
	{
		LM_ERR("str value required to set the branch\n");
		goto error;
	}
	
	if (append_branch( msg, &val->rs, 0, 0, Q_UNSPECIFIED, 0,
			msg->force_send_socket)!=1 )
	{
		LM_ERR("append_branch action failed\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

int pv_set_force_sock(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	struct socket_info *si;
	int port, proto;
	str host;
	
	if(msg==NULL || param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(val==NULL)
	{
		msg->force_send_socket = NULL;
		return 0;
	}

	if(!(val->flags&PV_VAL_STR) || val->rs.len<=0)
	{
		LM_ERR("str value required to set the force send sock\n");
		goto error;
	}
	
	if (parse_phostport(val->rs.s, val->rs.len, &host.s, &host.len, &port, &proto) < 0)
	{
		LM_ERR("invalid socket specification\n");
		goto error;
	}
	si = grep_sock_info(&host, (unsigned short)port, (unsigned short)proto);
	if (si!=NULL)
	{
		msg->force_send_socket = si;
	} else {
		LM_WARN("no socket found to match [%.*s]\n",
				val->rs.len, val->rs.s);
	}

	return 0;
error:
	return -1;
}

/********* end PV set functions *********/

int pv_parse_scriptvar_name(pv_spec_p sp, str *in)
{
	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	
	sp->pvp.pvn.type = PV_NAME_PVAR;
	sp->pvp.pvn.u.dname = (void*)add_var(in);
	if(sp->pvp.pvn.u.dname==NULL)
	{
		LM_ERR("cannot register var [%.*s]\n", in->len, in->s);
		return -1;
	}
	return 0;
}

int pv_parse_hdr_name(pv_spec_p sp, str *in)
{
	str s;
	char *p;
	pv_spec_p nsp = 0;
	struct hdr_field hdr;

	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
				
	p = in->s;
	if(*p==PV_MARKER)
	{
		nsp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
		if(nsp==NULL)
		{
			LM_ERR("no more memory\n");
			return -1;
		}
		p = pv_parse_spec(in, nsp);
		if(p==NULL)
		{
			LM_ERR("invalid name [%.*s]\n", in->len, in->s);
			return -1;
		}
		//LM_ERR("dynamic name [%.*s]\n", in->len, in->s);
		//pv_print_spec(nsp);
		sp->pvp.pvn.type = PV_NAME_PVAR;
		sp->pvp.pvn.u.dname = (void*)nsp;
		return 0;
	}

	if(in->len>=PV_LOCAL_BUF_SIZE-1)
	{
		LM_ERR("name too long\n");
		return -1;
	}
	memcpy(pv_local_buf, in->s, in->len);
	pv_local_buf[in->len] = ':';
	s.s = pv_local_buf;
	s.len = in->len+1;

	if (parse_hname2(s.s, s.s + ((s.len<4)?4:s.len), &hdr)==0)
	{
		LM_ERR("error parsing header name [%.*s]\n", s.len, s.s);
		goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	if (hdr.type!=HDR_OTHER_T && hdr.type!=HDR_ERROR_T)
	{
		LM_DBG("using hdr type (%d) instead of <%.*s>\n",
			hdr.type, in->len, in->s);
		sp->pvp.pvn.u.isname.type = 0;
		sp->pvp.pvn.u.isname.name.n = hdr.type;
	} else {
		sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
		sp->pvp.pvn.u.isname.name.s = *in;
	}
	return 0;
error:
	return -1;
}

int pv_parse_avp_name(pv_spec_p sp, str *in)
{
	char *p;
	char *s;
	pv_spec_p nsp = 0;

	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	p = in->s;
	if(*p==PV_MARKER)
	{
		nsp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
		if(nsp==NULL)
		{
			LM_ERR("no more memory\n");
			return -1;
		}
		s = pv_parse_spec(in, nsp);
		if(s==NULL)
		{
			LM_ERR("invalid name [%.*s]\n", in->len, in->s);
			return -1;
		}
		//LM_ERR("dynamic name [%.*s]\n", in->len, in->s);
		//pv_print_spec(nsp);
		sp->pvp.pvn.type = PV_NAME_PVAR;
		sp->pvp.pvn.u.dname = (void*)nsp;
		return 0;
	}
	/*LM_DBG("static name [%.*s]\n", in->len, in->s);*/
	if(parse_avp_spec(in, &sp->pvp.pvn.u.isname.type,
				&sp->pvp.pvn.u.isname.name)!=0)
	{
		LM_ERR("bad avp name [%.*s]\n", in->len, in->s);
		return -1;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	return 0;
}

int pv_parse_index(pv_spec_p sp, str *in)
{
	char *p;
	char *s;
	int sign;
	pv_spec_p nsp = 0;

	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	p = in->s;
	if(*p==PV_MARKER)
	{
		nsp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
		if(nsp==NULL)
		{
			LM_ERR("no more memory\n");
			return -1;
		}
		s = pv_parse_spec(in, nsp);
		if(s==NULL)
		{
			LM_ERR("invalid index [%.*s]\n", in->len, in->s);
			return -1;
		}
		sp->pvp.pvi.type = PV_IDX_PVAR;
		sp->pvp.pvi.u.dval = (void*)nsp;
		return 0;
	}
	if(*p=='*' && in->len==1)
	{
		sp->pvp.pvi.type = PV_IDX_ALL;
		return 0;
	}
	sign = 1;
	if(*p=='-')
	{
		sign = -1;
		p++;
	}
	sp->pvp.pvi.u.ival = 0;
	while(p<in->s+in->len && *p>='0' && *p<='9')
	{
		sp->pvp.pvi.u.ival = sp->pvp.pvi.u.ival * 10 + *p - '0';
		p++;
	}
	if(p!=in->s+in->len)
	{
		LM_ERR("invalid index [%.*s]\n", in->len, in->s);
		return -1;
	}
	sp->pvp.pvi.u.ival *= sign;
	sp->pvp.pvi.type = PV_IDX_INT;
	return 0;
}

int pv_init_iname(pv_spec_p sp, int param)
{
	if(sp==NULL)
		return -1;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.name.n = param;
	return 0;
}

/**
 * the table with core pseudo-variables
 */
static pv_export_t _pv_names_table[] = {
	{{"avp", (sizeof("avp")-1)}, PVT_AVP, pv_get_avp, pv_set_avp,
		pv_parse_avp_name, pv_parse_index, 0, 0},
	{{"hdr", (sizeof("hdr")-1)}, PVT_HDR, pv_get_hdr, 0, pv_parse_hdr_name,
		pv_parse_index, 0, 0},
	{{"var", (sizeof("var")-1)}, PVT_SCRIPTVAR, pv_get_scriptvar,
		pv_set_scriptvar, pv_parse_scriptvar_name, 0, 0, 0},

	{{"ai", (sizeof("ai")-1)}, /* */
		PVT_PAI_URI, pv_get_pai, 0,
		0, 0, 0, 0},
	{{"adu", (sizeof("adu")-1)}, /* auth digest uri */
		PVT_AUTH_DURI, pv_get_authattr, 0,
		0, 0, pv_init_iname, 3},
	{{"ar", (sizeof("ar")-1)}, /* auth realm */
		PVT_AUTH_REALM, pv_get_authattr, 0,
		0, 0, pv_init_iname, 2},
	{{"au", (sizeof("au")-1)}, /* */
		PVT_AUTH_USERNAME, pv_get_authattr, 0,
		0, 0, pv_init_iname, 1},
	{{"aU", (sizeof("aU")-1)}, /* */
		PVT_AUTH_USERNAME_WHOLE, pv_get_authattr, 0,
		0, 0, pv_init_iname, 4},
	{{"Au", (sizeof("Au")-1)}, /* */
		PVT_ACC_USERNAME, pv_get_acc_username, 0,
		0, 0, pv_init_iname, 1},
	{{"bf", (sizeof("bf")-1)}, /* */
		PVT_BFLAGS, pv_get_bflags, 0,
		0, 0, 0, 0},
	{{"bF", (sizeof("bF")-1)}, /* */
		PVT_HEXBFLAGS, pv_get_hexbflags, 0,
		0, 0, 0, 0},
	{{"br", (sizeof("br")-1)}, /* */
		PVT_BRANCH, pv_get_branch, pv_set_branch,
		0, 0, 0, 0},
	{{"bR", (sizeof("bR")-1)}, /* */
		PVT_BRANCHES, pv_get_branches, 0,
		0, 0, 0, 0},
	{{"ci", (sizeof("ci")-1)}, /* */
		PVT_CALLID, pv_get_callid, 0,
		0, 0, 0, 0},
	{{"cl", (sizeof("cl")-1)}, /* */
		PVT_CONTENT_LENGTH, pv_get_content_length, 0,
		0, 0, 0, 0},
	{{"cs", (sizeof("cs")-1)}, /* */
		PVT_CSEQ, pv_get_cseq, 0,
		0, 0, 0, 0},
	{{"ct", (sizeof("ct")-1)}, /* */
		PVT_CONTACT, pv_get_contact, 0,
		0, 0, 0, 0},
	{{"cT", (sizeof("cT")-1)}, /* */
		PVT_CONTENT_TYPE, pv_get_content_type, 0,
		0, 0, 0, 0},
	{{"dd", (sizeof("dd")-1)}, /* */
		PVT_DSTURI_DOMAIN, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"di", (sizeof("di")-1)}, /* */
		PVT_DIVERSION_URI, pv_get_diversion, 0,
		0, 0, 0, 0},
	{{"dp", (sizeof("dp")-1)}, /* */
		PVT_DSTURI_PORT, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"dP", (sizeof("dP")-1)}, /* */
		PVT_DSTURI_PROTOCOL, pv_get_dsturi_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"ds", (sizeof("ds")-1)}, /* */
		PVT_DSET, pv_get_dset, 0,
		0, 0, 0, 0},
	{{"du", (sizeof("du")-1)}, /* */
		PVT_DSTURI, pv_get_dsturi, pv_set_dsturi,
		0, 0, 0, 0},
	{{"duri", (sizeof("duri")-1)}, /* */
		PVT_DSTURI, pv_get_dsturi, pv_set_dsturi,
		0, 0, 0, 0},
	{{"err.class", (sizeof("err.class")-1)}, /* */
		PVT_ERR_CLASS, pv_get_errinfo_attr, 0,
		0, 0, 0, 0},
	{{"err.level", (sizeof("err.level")-1)}, /* */
		PVT_ERR_LEVEL, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"err.info", (sizeof("err.info")-1)}, /* */
		PVT_ERR_INFO, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"err.rcode", (sizeof("err.rcode")-1)}, /* */
		PVT_ERR_RCODE, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"err.rreason", (sizeof("err.rreason")-1)}, /* */
		PVT_ERR_RREASON, pv_get_errinfo_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"fd", (sizeof("fd")-1)}, /* */
		PVT_FROM_DOMAIN, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"from.domain", (sizeof("from.domain")-1)}, /* */
		PVT_FROM_DOMAIN, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"fn", (sizeof("fn")-1)}, /* */
		PVT_FROM_DISPLAYNAME, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 5},
	{{"fs", (sizeof("fs")-1)}, /* */
		PVT_FORCE_SOCK, pv_get_force_sock, pv_set_force_sock,
		0, 0, 0, 0},
	{{"ft", (sizeof("ft")-1)}, /* */
		PVT_FROM_TAG, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"fu", (sizeof("fu")-1)}, /* */
		PVT_FROM, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"from", (sizeof("from")-1)}, /* */
		PVT_FROM, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"fU", (sizeof("fU")-1)}, /* */
		PVT_FROM_USERNAME, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"from.user", (sizeof("from.user")-1)}, /* */
		PVT_FROM_USERNAME, pv_get_from_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"mb", (sizeof("mb")-1)}, /* */
		PVT_MSG_BUF, pv_get_msg_buf, 0,
		0, 0, 0, 0},
	{{"mf", (sizeof("mf")-1)}, /* */
		PVT_FLAGS, pv_get_flags, 0,
		0, 0, 0, 0},
	{{"mF", (sizeof("mF")-1)}, /* */
		PVT_HEXFLAGS, pv_get_hexflags, 0,
		0, 0, 0, 0},
	{{"mi", (sizeof("mi")-1)}, /* */
		PVT_MSGID, pv_get_msgid, 0,
		0, 0, 0, 0},
	{{"ml", (sizeof("ml")-1)}, /* */
		PVT_MSG_LEN, pv_get_msg_len, 0,
		0, 0, 0, 0},
	{{"od", (sizeof("od")-1)}, /* */
		PVT_OURI_DOMAIN, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"op", (sizeof("op")-1)}, /* */
		PVT_OURI_PORT, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"oP", (sizeof("oP")-1)}, /* */
		PVT_OURI_PROTOCOL, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"ou", (sizeof("ou")-1)}, /* */
		PVT_OURI, pv_get_ouri, 0,
		0, 0, 0, 0},
	{{"ouri", (sizeof("ouri")-1)}, /* */
		PVT_OURI, pv_get_ouri, 0,
		0, 0, 0, 0},
	{{"oU", (sizeof("oU")-1)}, /* */
		PVT_OURI_USERNAME, pv_get_ouri_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"pd", (sizeof("pd")-1)}, /* */
		PVT_PPI_DOMAIN, pv_get_ppi_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"pn", (sizeof("pn")-1)}, /* */
		PVT_PPI_DISPLAYNAME, pv_get_ppi_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"pp", (sizeof("pp")-1)}, /* */
		PVT_PID, pv_get_pid, 0,
		0, 0, 0, 0},
	{{"pr", (sizeof("pr")-1)}, /* */
		PVT_PROTO, pv_get_proto, 0,
		0, 0, 0, 0},
	{{"proto", (sizeof("proto")-1)}, /* */
		PVT_PROTO, pv_get_proto, 0,
		0, 0, 0, 0},
	{{"pu", (sizeof("pu")-1)}, /* */
		PVT_PPI, pv_get_ppi_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"pU", (sizeof("pU")-1)}, /* */
		PVT_PPI_USERNAME, pv_get_ppi_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"rb", (sizeof("rb")-1)}, /* */
		PVT_MSG_BODY, pv_get_msg_body, 0,
		0, 0, 0, 0},
	{{"rc", (sizeof("rc")-1)}, /* */
		PVT_RETURN_CODE, pv_get_return_code, 0,
		0, 0, 0, 0},
	{{"retcode", (sizeof("retcode")-1)}, /* */
		PVT_RETURN_CODE, pv_get_return_code, 0,
		0, 0, 0, 0},
	{{"rd", (sizeof("rd")-1)}, /* */
		PVT_RURI_DOMAIN, pv_get_ruri_attr, pv_set_ruri_host,
		0, 0, pv_init_iname, 2},
	{{"ruri.domain", (sizeof("ruri.domain")-1)}, /* */
		PVT_RURI_DOMAIN, pv_get_ruri_attr, pv_set_ruri_host,
		0, 0, pv_init_iname, 2},
	{{"re", (sizeof("re")-1)}, /* */
		PVT_RPID_URI, pv_get_rpid, 0,
		0, 0, 0, 0},
	{{"rm", (sizeof("rm")-1)}, /* */
		PVT_METHOD, pv_get_method, 0,
		0, 0, 0, 0},
	{{"rp", (sizeof("rp")-1)}, /* */
		PVT_RURI_PORT, pv_get_ruri_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"rP", (sizeof("rP")-1)}, /* */
		PVT_RURI_PROTOCOL, pv_get_ruri_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"rr", (sizeof("rr")-1)}, /* */
		PVT_REASON, pv_get_reason, 0,
		0, 0, 0, 0},
	{{"rs", (sizeof("rs")-1)}, /* */
		PVT_STATUS, pv_get_status, 0,
		0, 0, 0, 0},
	{{"rt", (sizeof("rt")-1)}, /* */
		PVT_REFER_TO, pv_get_refer_to, 0,
		0, 0, 0, 0},
	{{"ru", (sizeof("ru")-1)}, /* */
		PVT_RURI, pv_get_ruri, pv_set_ruri,
		0, 0, 0, 0},
	{{"ruri", (sizeof("ruri")-1)}, /* */
		PVT_RURI, pv_get_ruri, pv_set_ruri,
		0, 0, 0, 0},
	{{"rU", (sizeof("rU")-1)}, /* */
		PVT_RURI_USERNAME, pv_get_ruri_attr, pv_set_ruri_user,
		0, 0, pv_init_iname, 1},
	{{"ruri.user", (sizeof("ruri.user")-1)}, /* */
		PVT_RURI_USERNAME, pv_get_ruri_attr, pv_set_ruri_user,
		0, 0, pv_init_iname, 1},
	{{"Ri", (sizeof("Ri")-1)}, /* */
		PVT_RCVIP, pv_get_rcvip, 0,
		0, 0, 0, 0},
	{{"Rp", (sizeof("Rp")-1)}, /* */
		PVT_RCVPORT, pv_get_rcvport, 0,
		0, 0, 0, 0},
	{{"sf", (sizeof("sf")-1)}, /* */
		PVT_SFLAGS, pv_get_sflags, 0,
		0, 0, 0, 0},
	{{"sF", (sizeof("sF")-1)}, /* */
		PVT_HEXSFLAGS, pv_get_hexsflags, 0,
		0, 0, 0, 0},
	{{"src_ip", (sizeof("src_ip")-1)}, /* */
		PVT_SRCIP, pv_get_srcip, 0,
		0, 0, 0, 0},
	{{"si", (sizeof("si")-1)}, /* */
		PVT_SRCIP, pv_get_srcip, 0,
		0, 0, 0, 0},
	{{"sp", (sizeof("sp")-1)}, /* */
		PVT_SRCPORT, pv_get_srcport, 0,
		0, 0, 0, 0},
	{{"td", (sizeof("td")-1)}, /* */
		PVT_TO_DOMAIN, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"to.domain", (sizeof("to.domain")-1)}, /* */
		PVT_TO_DOMAIN, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 3},
	{{"tn", (sizeof("tn")-1)}, /* */
		PVT_TO_DISPLAYNAME, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 5},
	{{"tt", (sizeof("tt")-1)}, /* */
		PVT_TO_TAG, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 4},
	{{"tu", (sizeof("tu")-1)}, /* */
		PVT_TO, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"to", (sizeof("to")-1)}, /* */
		PVT_TO, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 1},
	{{"tU", (sizeof("tU")-1)}, /* */
		PVT_TO_USERNAME, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"to.user", (sizeof("to.user")-1)}, /* */
		PVT_TO_USERNAME, pv_get_to_attr, 0,
		0, 0, pv_init_iname, 2},
	{{"Tf", (sizeof("tf")-1)}, /* */
		PVT_TIMEF, pv_get_timef, 0,
		0, 0, 0, 0},
	{{"Ts", (sizeof("ts")-1)}, /* */
		PVT_TIMES, pv_get_times, 0,
		0, 0, 0, 0},
	{{"ua", (sizeof("ua")-1)}, /* */
		PVT_USERAGENT, pv_get_useragent, 0,
		0, 0, 0, 0},

	{{0,0}, 0, 0, 0, 0, 0, 0, 0}
};

pv_export_t* pv_lookup_spec_name(str *pvname, pv_spec_p e)
{
	int i;
	pv_extra_p pvi;
	int found;

	if(pvname==0 || e==0)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}
	/* search in main table */
	for(i=0; _pv_names_table[i].name.s!=0; i++)
	{
		if(_pv_names_table[i].name.len==pvname->len
			&& memcmp(_pv_names_table[i].name.s, pvname->s, pvname->len)==0)
		{
			/*LM_DBG("found [%.*s] [%d]\n", pvname->len, pvname->s,
					_pv_names_table[i].type);*/
			/* copy data from table to spec */
			e->type = _pv_names_table[i].type;
			e->getf = _pv_names_table[i].getf;
			e->setf = _pv_names_table[i].setf;
			return &_pv_names_table[i];
		}
	}
	/* search in extra list */
	if(_pv_extra_list==0)
	{
		LM_DBG("extra items list is empty\n");
		return NULL;
	}
	pvi = *_pv_extra_list;
	while(pvi)
	{
		if(pvi->pve.name.len>pvname->len)
			break;
		if(pvi->pve.name.len==pvname->len)
		{
			found = strncmp(pvi->pve.name.s, pvname->s, pvname->len);
			if(found>0)
				break;
			if(found==0)
			{
				LM_DBG("found in extra list [%.*s]\n", pvname->len, pvname->s);
				/* copy data from export to spec */
				e->type = pvi->pve.type;
				e->getf = pvi->pve.getf;
				e->setf = pvi->pve.setf;
				return &(pvi->pve);
			}
		}
		pvi = pvi->next;
	}

	return NULL;
}

static int is_pv_valid_char(char c)
{
	if((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z')
			|| (c=='_') || (c=='.'))
		return 1;
	return 0;
}

char* pv_parse_spec(str *in, pv_spec_p e)
{
	char *p;
	str s;
	str pvname;
	int pvstate;
	trans_t *tr = NULL;
	pv_export_t *pte = NULL;
	int n=0;

	if(in==NULL || in->s==NULL || e==NULL || *in->s!=PV_MARKER)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}
	
	/*LM_DBG("input [%.*s]\n", in->len, in->s);*/
	tr = 0;
	pvstate = 0;
	memset(e, 0, sizeof(pv_spec_t));
	p = in->s;
	p++;
	if(*p==PV_LNBRACKET)
	{
		p++;
		pvstate = 1;
	}
	pvname.s = p;
	if(*p == PV_MARKER) {
		p++;
		if(pvstate==1)
		{
			if(*p!=PV_RNBRACKET)
				goto error;
			p++;
		}
		e->getf = pv_get_marker;
		e->type = PVT_MARKER;
		pvname.len = 1;
		goto done_all;
	}
	while(is_in_str(p,in) && is_pv_valid_char(*p))
		p++;
	pvname.len = p - pvname.s;
	if(pvstate==1)
	{
		if(*p==PV_RNBRACKET)
		{ /* full pv name ended here*/
			goto done_inm;
		} else if(*p==PV_LNBRACKET) {
			p++;
			pvstate = 2;
		} else if(*p==PV_LIBRACKET) {
			p++;
			pvstate = 3;
		} else if(*p==TR_LBRACKET) {
			p++;
			pvstate = 4;
		} else {
			LM_ERR("invalid char '%c' in [%.*s] (%d)\n", *p, in->len, in->s,
					pvstate);
			goto error;
		}
	} else { 
		if(!is_in_str(p, in)) {
			p--;
			goto done_inm;
		} else if(*p==PV_LNBRACKET) {
			p++;
			pvstate = 5;
		} else {
			/* still in input str, but end of PV */
			/* p is increased at the end, so decrement here */
			p--;
			goto done_inm;
		}
	}

done_inm:
	if((pte = pv_lookup_spec_name(&pvname, e))==NULL)
	{
		LM_ERR("error searching pvar \"%.*s\"\n", pvname.len, pvname.s);
		goto error;
	}
	if(pte->parse_name!=NULL && pvstate!=2 && pvstate!=5)
	{
		LM_ERR("pvar \"%.*s\" expects an inner name\n",
				pvname.len, pvname.s);
		goto error;
	}
	if(pvstate==2 || pvstate==5)
	{
		if(pte->parse_name==NULL)
		{
			LM_ERR("pvar \"%.*s\" does not get name param\n",
					pvname.len, pvname.s);
			goto error;
		}
		s.s = p;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==PV_RNBRACKET)
			{
				if(n==0)
					break;
				n--;
			}
			if(*p == PV_LNBRACKET)
				n++;
			p++;
		}

		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			LM_ERR("pvar \"%.*s\" does not get empty name param\n",
					pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s;
		if(pte->parse_name(e, &s)!=0)
		{
			LM_ERR("pvar \"%.*s\" has an invalid name param [%.*s]\n",
					pvname.len, pvname.s, s.len, s.s);
			goto error;
		}
		if(pvstate==2)
		{
			p++;
			if(*p==PV_RNBRACKET)
			{ /* full pv name ended here*/
				goto done_vnm;
			} else if(*p==PV_LIBRACKET) {
				p++;
				pvstate = 3;
			} else if(*p==TR_LBRACKET) {
				p++;
				pvstate = 4;
			} else {
				LM_ERR("invalid char '%c' in [%.*s] (%d)\n", *p, in->len, in->s,
					pvstate);
				goto error;
			}
		} else {
			if(*p==PV_RNBRACKET)
			{ /* full pv name ended here*/
				p++;
				goto done_all;
			} else {
				LM_ERR("invalid char '%c' in [%.*s] (%d)\n", *p, in->len, in->s,
					pvstate);
				goto error;
			}
		}
	}
done_vnm:
	if(pvstate==3)
	{
		if(pte->parse_index==NULL)
		{
			LM_ERR("pvar \"%.*s\" does not get index param\n",
					pvname.len, pvname.s);
			goto error;
		}
		s.s = p;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==PV_RIBRACKET)
			{
				if(n==0)
					break;
				n--;
			}
			if(*p == PV_LIBRACKET)
				n++;
			p++;
		}
		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			LM_ERR("pvar \"%.*s\" does not get empty index param\n",
					pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s;
		if(pte->parse_index(e, &s)!=0)
		{
			LM_ERR("pvar \"%.*s\" has an invalid index param [%.*s]\n",
					pvname.len, pvname.s, s.len, s.s);
			goto error;
		}
		p++;
		if(*p==PV_RNBRACKET)
		{ /* full pv name ended here*/
			goto done_idx;
		} else if(*p==TR_LBRACKET) {
			p++;
			pvstate = 4;
		} else {
			LM_ERR("invalid char '%c' in [%.*s] (%d)\n", *p, in->len, in->s,
					pvstate);
			goto error;
		}
	}
done_idx:
	if(pvstate==4)
	{
		s.s = p-1;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==TR_RBRACKET)
			{
				if(n==0)
				{
					/* yet another transformation */
					p++;
					while(is_in_str(p, in) && (*p==' ' || *p=='\t')) p++;

					if(!is_in_str(p, in) || *p != TR_LBRACKET)
					{
						p--;
						break;
					}
				}
				n--;
			}
			if(*p == TR_LBRACKET)
				n++;
			p++;
		}
		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			LM_ERR("pvar \"%.*s\" does not get empty index param\n",
					pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s + 1;

		p = parse_transformation(&s, &tr);
		if(p==NULL)
		{
			LM_ERR("ERROR:bad tr in pvar name \"%.*s\"\n",
					pvname.len, pvname.s);
			goto error;
		}
		if(*p!=PV_RNBRACKET)
		{
			LM_ERR("bad pvar name \"%.*s\" (%c)!\n", in->len, in->s, *p);
			goto error;
		}
		e->trans = (void*)tr;
	}
	p++;

done_all:
	if(pte!=NULL && pte->init_param)
		pte->init_param(e, pte->iparam);
	return p;

error:
	if(p!=NULL)
		LM_ERR("wrong char [%c/%d] in [%.*s] at [%d (%d)]\n", *p, (int)*p,
			in->len, in->s, (int)(p-in->s), pvstate);
	else
		LM_ERR("invalid parsing in [%.*s] at (%d)\n", in->len, in->s, pvstate);
	return NULL;

} /* end: pv_parse_spec */

/**
 *
 */
int pv_parse_format(str *in, pv_elem_p *el)
{
	char *p, *p0;
	int n = 0;
	pv_elem_p e, e0;
	str s;

	if(in==NULL || in->s==NULL || el==NULL)
		return -1;

	/*LM_DBG("parsing [%.*s]\n", in->len, in->s);*/
	
	p = in->s;
	*el = NULL;
	e = e0 = NULL;

	while(is_in_str(p,in))
	{
		e0 = e;
		e = pkg_malloc(sizeof(pv_elem_t));
		if(!e)
			goto error;
		memset(e, 0, sizeof(pv_elem_t));
		n++;
		if(*el == NULL)
			*el = e;
		if(e0)
			e0->next = e;
	
		e->text.s = p;
		while(is_in_str(p,in) && *p!=PV_MARKER)
			p++;
		e->text.len = p - e->text.s;
		
		if(*p == '\0')
			break;
		s.s = p;
		s.len = in->s+in->len-p;
		p0 = pv_parse_spec(&s, &e->spec);
		
		if(p0==NULL)
			goto error;
		if(*p0 == '\0')
			break;
		p = p0;
	}
	/*LM_DBG("format parsed OK: [%d] items\n", n);*/

	return 0;

error:
	pv_elem_free_all(*el);
	*el = NULL;
	return -1;
}

int pv_get_spec_name(struct sip_msg* msg, pv_param_p ip, pv_value_t *name)
{
	if(msg==NULL || ip==NULL || name==NULL)
		return -1;
	memset(name, 0, sizeof(pv_value_t));

	if(ip->pvn.type==PV_NAME_INTSTR)
	{
		if(ip->pvn.u.isname.type&AVP_NAME_STR)
		{
			name->rs = ip->pvn.u.isname.name.s;
			name->flags = PV_VAL_STR;
		} else {
			name->ri = ip->pvn.u.isname.name.n;
			name->flags = PV_VAL_INT|PV_TYPE_INT;
		}
		return 0;
	}
	/* pvar */
	if(pv_get_spec_value(msg, (pv_spec_p)(ip->pvn.u.dname), name)!=0)
	{
		LM_ERR("cannot get name value\n");
		return -1;
	}
	if(name->flags&PV_VAL_NULL || name->flags&PV_VAL_EMPTY)
	{
		LM_ERR("null or empty name\n");
		return -1;
	}
	return 0;
}

int pv_get_avp_name(struct sip_msg* msg, pv_param_p ip, int_str *avp_name,
		unsigned short *name_type)
{
	pv_value_t tv;
	if(ip==NULL || avp_name==NULL || name_type==NULL)
		return -1;
	memset(avp_name, 0, sizeof(int_str));
	*name_type = 0;

	if(ip->pvn.type==PV_NAME_INTSTR)
	{
		*name_type = ip->pvn.u.isname.type;
		if(ip->pvn.u.isname.type&AVP_NAME_STR)
		{
			avp_name->s = ip->pvn.u.isname.name.s;
			*name_type |= AVP_NAME_STR;
		} else {
			avp_name->n = ip->pvn.u.isname.name.n;
			*name_type &= AVP_SCRIPT_MASK;
		}
		return 0;
	}
	/* pvar */
	if(pv_get_spec_value(msg, (pv_spec_p)(ip->pvn.u.dname), &tv)!=0)
	{
		LM_ERR("cannot get avp value\n");
		return -1;
	}
	if(tv.flags&PV_VAL_NULL || tv.flags&PV_VAL_EMPTY)
	{
		LM_ERR("null or empty name\n");
		return -1;
	}
		
	if((tv.flags&PV_TYPE_INT) && (tv.flags&PV_VAL_INT))
	{
		avp_name->n = tv.ri;
	} else {
		avp_name->s = tv.rs;
		*name_type = AVP_NAME_STR;
	}
	return 0;
}


int pv_get_spec_index(struct sip_msg* msg, pv_param_p ip, int *idx, int *flags)
{
	pv_value_t tv;
	if(ip==NULL || idx==NULL || flags==NULL)
		return -1;

	*idx = 0;
	*flags = 0;

	if(ip->pvi.type == PV_IDX_ALL) {
		*flags = PV_IDX_ALL;
		return 0;
	}
	
	if(ip->pvi.type == PV_IDX_INT)
	{
		*idx = ip->pvi.u.ival;
		return 0;
	}

	/* pvar */
	if(pv_get_spec_value(msg, (pv_spec_p)ip->pvi.u.dval, &tv)!=0)
	{
		LM_ERR("cannot get index value\n");
		return -1;
	}
	if(!(tv.flags&PV_VAL_INT))
	{
		LM_ERR("invalid index value\n");
		return -1;
	}
	*idx = tv.ri;
	return 0;
}


int pv_get_spec_value(struct sip_msg* msg, pv_spec_p sp, pv_value_t *value)
{
	int ret = 0;

	if(msg==NULL || sp==NULL || sp->getf==NULL || value==NULL
			|| sp->type==PVT_NONE)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	memset(value, 0, sizeof(pv_value_t));

	ret = (*sp->getf)(msg, &(sp->pvp), value);
	if(ret!=0)
		return ret;
	if(sp->trans)
		return run_transformations(msg, (trans_t*)sp->trans, value);
	return ret;
}

int pv_print_spec(struct sip_msg* msg, pv_spec_p sp, char *buf, int *len)
{
	pv_value_t tok;
	if(msg==NULL || sp==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;
	
	memset(&tok, 0, sizeof(pv_value_t));
	
	/* put the value of the specifier */
	if(pv_get_spec_value(msg, sp, &tok)==0)
	{
		if(tok.flags&PV_VAL_NULL)
			tok.rs = str_null;
		if(tok.rs.len < *len)
			memcpy(buf, tok.rs.s, tok.rs.len);
		else
			goto overflow;
	}
	
	*len = tok.rs.len;
	buf[tok.rs.len] = '\0';
	return 0;
	
overflow:
	LM_ERR("buffer overflow -- increase the buffer size...\n");
	return -1;
}


int pv_printf(struct sip_msg* msg, pv_elem_p list, char *buf, int *len)
{
	int n, h;
	pv_value_t tok;
	pv_elem_p it;
	char *cur;
	
	if(msg==NULL || list==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;

	*buf = '\0';
	cur = buf;
	
	h = 0;
	n = 0;
	for (it=list; it; it=it->next)
	{
		/* put the text */
		if(it->text.s && it->text.len>0)
		{
			if(n+it->text.len < *len)
			{
				memcpy(cur, it->text.s, it->text.len);
				n += it->text.len;
				cur += it->text.len;
			} else {
				LM_ERR("no more space for text [%d]\n", it->text.len);
				goto overflow;
			}
		}
		/* put the value of the specifier */
		if(it->spec.type!=PVT_NONE
				&& pv_get_spec_value(msg, &(it->spec), &tok)==0)
		{
			if(tok.flags&PV_VAL_NULL)
				tok.rs = str_null;
			if(n+tok.rs.len < *len)
			{
				if(tok.rs.len>0)
				{
					memcpy(cur, tok.rs.s, tok.rs.len);
					n += tok.rs.len;
					cur += tok.rs.len;
				}
			} else {
				LM_ERR("no more space for spec value\n");
				goto overflow;
			}
		}
	}

	goto done;
	
overflow:
	LM_ERR("buffer overflow -- increase the buffer size...\n");
	return -1;

done:
#ifdef EXTRA_DEBUG
	LM_DBG("final buffer length %d\n", n);
#endif
	*cur = '\0';
	*len = n;
	return 0;
}



pvname_list_t* parse_pvname_list(str *in, unsigned int type)
{
	pvname_list_t* head = NULL;
	pvname_list_t* al = NULL;
	pvname_list_t* last = NULL;
	char *p;
	pv_spec_t spec;
	str s;

	if(in==NULL || in->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	p = in->s;
	while(is_in_str(p, in))
	{
		while(is_in_str(p, in) && (*p==' '||*p=='\t'||*p==','||*p==';'))
			p++;
		if(!is_in_str(p, in))
		{
			if(head==NULL)
				LM_ERR("wrong item name list [%.*s]\n", in->len, in->s);
			return head;
		}
		s.s=p;
		s.len = in->s+in->len-p;
		p = pv_parse_spec(&s, &spec);
		if(p==NULL || (type && spec.type!=type))
		{
			LM_ERR("wrong item name list [%.*s]!\n", in->len, in->s);
			goto error;
		}
		al = (pvname_list_t*)pkg_malloc(sizeof(pvname_list_t));
		if(al==NULL)
		{
			LM_ERR("no more memory!\n");
			goto error;
		}
		memset(al, 0, sizeof(pvname_list_t));
		memcpy(&al->sname, &spec, sizeof(pv_spec_t));

		if(last==NULL)
		{
			head = al;
			last = al;
		} else {
			last->next = al;
			last = al;
		}
	}

	return head;

error:
	while(head)
	{
		al = head;
		head=head->next;
		pkg_free(al);
	}
	return NULL;
}

int pv_elem_free_all(pv_elem_p log)
{
	pv_elem_p t;
	while(log)
	{
		t = log;
		log = log->next;
		pkg_free(t);
	}
	return 0;
}

void pv_value_destroy(pv_value_t *val)
{
	if(val==0) return;
	if(val->flags&PV_VAL_PKG) pkg_free(val->rs.s);
	if(val->flags&PV_VAL_SHM) shm_free(val->rs.s);
	memset(val, 0, sizeof(pv_value_t));
}

#define PV_PRINT_BUF_SIZE  1024
#define PV_PRINT_BUF_NO    3
/*IMPORTANT NOTE - even if the function prints and returns a static buffer, it
 * has built-in support for 3 levels of nesting (or concurrent usage).
 * If you think it's not enough for you, either use pv_printf() directly,
 * either increase PV_PRINT_BUF_NO   --bogdan */
int pv_printf_s(struct sip_msg* msg, pv_elem_p list, str *s)
{
	static int buf_itr = 0;
	static char buf[PV_PRINT_BUF_NO][PV_PRINT_BUF_SIZE];

	if (list->next==0 && list->spec.getf==0) {
		*s = list->text;
		return 0;
	} else {
		s->s = buf[buf_itr];
		s->len = PV_PRINT_BUF_SIZE;
		buf_itr = (buf_itr+1)%PV_PRINT_BUF_NO;
		return pv_printf( msg, list, s->s, &s->len);
	}
}

void pv_spec_free(pv_spec_t *spec)
{
	if(spec==0) return;
	/* should go recursively */
	pkg_free(spec);
}

int pv_spec_dbg(pv_spec_p sp)
{
	if(sp==NULL)
	{
		LM_DBG("spec: <<NULL>>\n");
		return 0;
	}
	LM_DBG("<spec>\n");
	LM_DBG("type: %d\n", sp->type);
	LM_DBG("getf: %p\n", sp->getf);
	LM_DBG("setf: %p\n", sp->setf);
	LM_DBG("tran: %p\n", sp->trans);
	LM_DBG("<param>\n");
	LM_DBG("<name>\n");
	LM_DBG("type: %d\n", sp->pvp.pvn.type);
	if(sp->pvp.pvn.type==PV_NAME_INTSTR)
	{
		LM_DBG("sub-type: %d\n", sp->pvp.pvn.u.isname.type);
		if (sp->pvp.pvn.u.isname.type&AVP_NAME_STR)
		{
			LM_DBG("name str: %.*s\n",
					sp->pvp.pvn.u.isname.name.s.len,
					sp->pvp.pvn.u.isname.name.s.s);
		} else {
			LM_DBG("name in: %d\n",
					sp->pvp.pvn.u.isname.name.n);
		}

	} else if(sp->pvp.pvn.type==PV_NAME_PVAR) {
		pv_spec_dbg((pv_spec_p)sp->pvp.pvn.u.dname);
	} else {
		LM_DBG("name: unknown\n");
	}
	LM_DBG("</name>\n");
	LM_DBG("<index>\n");
	LM_DBG("type: %d\n", sp->pvp.pvi.type);
	if(sp->pvp.pvi.type==PV_IDX_INT)
	{
		LM_DBG("index: %d\n", sp->pvp.pvi.u.ival);
	} else if(sp->pvp.pvi.type==PV_IDX_PVAR) {
		pv_spec_dbg((pv_spec_p)sp->pvp.pvi.u.dval);
	} else if(sp->pvp.pvi.type==PV_IDX_ALL){
		LM_DBG("index: *\n");
	} else {
		LM_DBG("index: unknown\n");
	}
	LM_DBG("</index>\n");
	LM_DBG("</param>\n");
	LM_DBG("</spec\n");
	return 0;
}


/**
 *
 */
int pv_init_extra_list(void)
{
	_pv_extra_list = (pv_extra_p*)pkg_malloc(sizeof(pv_extra_p));
	if(_pv_extra_list==0)
	{
		LM_ERR("cannot alloc extra items list\n");
		return -1;
	}
	*_pv_extra_list=0;
	return 0;
}

int pv_add_extra(pv_export_t *e)
{
	char *p;
	str  *in;
	pv_extra_t *pvi = NULL;
	pv_extra_t *pvj = NULL;
	pv_extra_t *pvn = NULL;
	int found;

	if(e==NULL || e->name.s==NULL || e->getf==NULL || e->type==PVT_NONE)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}
	
	if(_pv_extra_list==0)
	{
		LM_DBG("extra items list is not initialized\n");
		if(pv_init_extra_list()!=0)
		{
			LM_ERR("cannot intit extra list\n");
			return -1;
		}
	}
	in = &(e->name);
	p = in->s;	
	while(is_in_str(p,in) && is_pv_valid_char(*p))
		p++;
	if(is_in_str(p,in))
	{
		LM_ERR("invalid char [%c] in [%.*s]\n", *p, in->len, in->s);
		return -1;
	}
	found = 0;
	pvi = *_pv_extra_list;
	while(pvi)
	{
		if(pvi->pve.name.len > in->len)
			break;
		if(pvi->pve.name.len==in->len)
		{
			found = strncmp(pvi->pve.name.s, in->s, in->len);
			if(found>0)
				break;
			if(found==0)
			{
				LM_ERR("pvar [%.*s] already exists\n", in->len, in->s);
				return -1;
			}
		}
		pvj = pvi;
		pvi = pvi->next;
	}

	pvn = (pv_extra_t*)pkg_malloc(sizeof(pv_extra_t));
	if(pvn==0)
	{
		LM_ERR("no more memory\n");
		return -1;
	}
	memcpy(pvn, e, sizeof(pv_extra_t));
	pvn->pve.type += PVT_EXTRA;

	if(pvj==0)
	{
		pvn->next = *_pv_extra_list;
		*_pv_extra_list = pvn;
		goto done;
	}
	pvn->next = pvj->next;
	pvj->next = pvn;

done:
	return 0;
}

int register_pvars_mod(char *mod_name, pv_export_t *items)
{
	int ret;
	int i;

	if (items==0)
		return 0;

	for ( i=0 ; items[i].name.s ; i++ ) {
		ret = pv_add_extra(&items[i]);
		if (ret!=0) {
			LM_ERR("failed to register pseudo-variable <%.*s> for module %s\n",
					items[i].name.len, items[i].name.s, mod_name);
		}
	}
	return 0;
}

/**
 *
 */
int pv_free_extra_list(void)
{
	pv_extra_p xe;
	pv_extra_p xe1;
	if(_pv_extra_list!=0)
	{
		xe = *_pv_extra_list;
		while(xe!=0)
		{
			xe1 = xe;
			xe = xe->next;
			pkg_free(xe1);
		}
		pkg_free(_pv_extra_list);
		_pv_extra_list = 0;
	}
	
	return 0;
}


