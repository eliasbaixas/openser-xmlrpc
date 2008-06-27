/*
 * $Id: msilo.c 3652 2008-02-07 09:35:13Z miconda $
 *
 * MSILO module
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
 * History
 * -------
 *
 * 2003-01-23: switched from t_uac to t_uac_dlg (dcm)
 * 2003-02-28: protocolization of t_uac_dlg completed (jiri)
 * 2003-03-11: updated to the new module interface (andrei)
 *             removed non-constant initializers to some strs (andrei)
 * 2003-03-16: flags parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db_init removed from mod_init, will be called from child_init
 *             now (janakj)
 * 2003-04-07: m_dump takes a parameter which sets the way the outgoing URI
 *             is computed (dcm)
 * 2003-08-05 adapted to the new parse_content_type_hdr function (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 * 2006-09-10 m_dump now checks if registering UA supports MESSAGE method (jh)
 * 2006-10-05 added max_messages module variable (jh)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_allow.h"
#include "../../parser/parse_methods.h"
#include "../../resolve.h"
#include "../../usr_avp.h"
#include "../../pvar.h"

#include "../tm/tm_load.h"

#define CONTACT_PREFIX "Content-Type: text/plain"CRLF"Contact: <"
#define CONTACT_SUFFIX  ">;msilo=yes"CRLF
#define CONTACT_PREFIX_LEN (sizeof(CONTACT_PREFIX)-1)
#define CONTACT_SUFFIX_LEN  (sizeof(CONTACT_SUFFIX)-1)
#define OFFLINE_MESSAGE	"] is offline. The message will be delivered when user goes online."
#define OFFLINE_MESSAGE_LEN	(sizeof(OFFLINE_MESSAGE)-1)

#include "ms_msg_list.h"
#include "msfuncs.h"

#define MAX_DEL_KEYS	1	
#define NR_KEYS			10

char *sc_mid      = "id";        /* 0 */
char *sc_from     = "src_addr";  /* 1 */
char *sc_to       = "dst_addr";  /* 2 */
char *sc_uri_user = "username";  /* 3 */
char *sc_uri_host = "domain";    /* 4 */
char *sc_body     = "body";      /* 5 */
char *sc_ctype    = "ctype";     /* 6 */
char *sc_exp_time = "exp_time";  /* 7 */
char *sc_inc_time = "inc_time";  /* 8 */
char *sc_snd_time = "snd_time";  /* 9 */

#define SET_STR_VAL(_str, _res, _r, _c)	\
	if (RES_ROWS(_res)[_r].values[_c].nul == 0) \
	{ \
		switch(RES_ROWS(_res)[_r].values[_c].type) \
		{ \
		case DB_STRING: \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.string_val; \
			(_str).len=strlen((_str).s); \
			break; \
		case DB_STR: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.str_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.str_val.s; \
			break; \
		case DB_BLOB: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.blob_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.blob_val.s; \
			break; \
		default: \
			(_str).len=0; \
			(_str).s=NULL; \
		} \
	}

MODULE_VERSION

#define S_TABLE_VERSION 5

/** database connection */
static db_con_t *db_con = NULL;
static db_func_t msilo_dbf;

/** precessed msg list - used for dumping the messages */
msg_list ml = NULL;

/** TM bind */
struct tm_binds tmb;

/** parameters */

char *ms_db_url=DEFAULT_DB_URL;
char *ms_db_table="silo";
str  ms_registrar={NULL, 0}; /*"sip:registrar@example.org";*/
str  ms_reminder={NULL, 0};
str  ms_outbound_proxy={NULL, 0};

int  ms_expire_time=259200;
int  ms_check_time=60;
int  ms_send_time=0;
int  ms_clean_period=10;
int  ms_use_contact=1;
int  ms_add_date = 1;
int  ms_max_messages = 0;

char*    ms_snd_time_avp_param = 0;
int_str  ms_snd_time_avp_name;
unsigned short  ms_snd_time_avp_type;

str msg_type = { "MESSAGE", 7 };

#define MSILO_PRINTBUF_SIZE 1024
static char msilo_printbuf[MSILO_PRINTBUF_SIZE];

/** module functions */
static int mod_init(void);
static int child_init(int);

static int m_store(struct sip_msg*, char*, char*);
static int m_dump(struct sip_msg*, char*, char*);

void destroy(void);

static int fixup_msilo(void** param, int param_no);

void m_clean_silo(unsigned int ticks, void *);
void m_send_ontimer(unsigned int ticks, void *);

int ms_reset_stime(int mid);

int check_message_support(struct sip_msg* msg);

/** TM callback function */
static void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"m_store",  m_store, 0, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_store",  m_store, 1, fixup_msilo, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_dump",   m_dump,  0, 0, 0, REQUEST_ROUTE},
	{"m_dump",   m_dump,  1, fixup_msilo, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{ "db_url",       STR_PARAM, &ms_db_url             },
	{ "db_table",     STR_PARAM, &ms_db_table           },
	{ "registrar",    STR_PARAM, &ms_registrar.s        },
	{ "reminder",     STR_PARAM, &ms_reminder.s         },
	{ "outbound_proxy",STR_PARAM, &ms_outbound_proxy.s  },
	{ "expire_time",  INT_PARAM, &ms_expire_time        },
	{ "check_time",   INT_PARAM, &ms_check_time         },
	{ "send_time",    INT_PARAM, &ms_send_time          },
	{ "clean_period", INT_PARAM, &ms_clean_period       },
	{ "use_contact",  INT_PARAM, &ms_use_contact        },
	{ "sc_mid",       STR_PARAM, &sc_mid                },
	{ "sc_from",      STR_PARAM, &sc_from               },
	{ "sc_to",        STR_PARAM, &sc_to                 },
	{ "sc_uri_user",  STR_PARAM, &sc_uri_user           },
	{ "sc_uri_host",  STR_PARAM, &sc_uri_host           },
	{ "sc_body",      STR_PARAM, &sc_body               },
	{ "sc_ctype",     STR_PARAM, &sc_ctype              },
	{ "sc_exp_time",  STR_PARAM, &sc_exp_time           },
	{ "sc_inc_time",  STR_PARAM, &sc_inc_time           },
	{ "sc_snd_time",  STR_PARAM, &sc_snd_time           },
	{ "snd_time_avp", STR_PARAM, &ms_snd_time_avp_param },
	{ "add_date",     INT_PARAM, &ms_add_date           },
	{ "max_messages", INT_PARAM, &ms_max_messages       },
	{ 0,0,0 }
};

#ifdef STATISTICS
#include "../../statistics.h"

stat_var* ms_stored_msgs;
stat_var* ms_dumped_msgs;
stat_var* ms_failed_msgs;
stat_var* ms_dumped_rmds;
stat_var* ms_failed_rmds;

stat_export_t msilo_stats[] = {
	{"stored_messages" ,  0,  &ms_stored_msgs  },
	{"dumped_messages" ,  0,  &ms_dumped_msgs  },
	{"failed_messages" ,  0,  &ms_failed_msgs  },
	{"dumped_reminders" , 0,  &ms_dumped_rmds  },
	{"failed_reminders" , 0,  &ms_failed_rmds  },
	{0,0,0}
};

#endif
/** module exports */
struct module_exports exports= {
	"msilo",    /* module id */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* module's exported functions */
	params,     /* module's exported parameters */
#ifdef STATISTICS
	msilo_stats,
#else
	0,          /* exported statistics */
#endif
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	(response_function) 0,       /* response handler */
	(destroy_function) destroy,  /* module destroy function */
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	pv_spec_t avp_spec;
	str _s;
	int ver = 0;

	LM_DBG("initializing ...\n");

	/* binding to mysql module  */
	if (bind_dbmod(ms_db_url, &msilo_dbf))
	{
		LM_DBG("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(msilo_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	if (ms_snd_time_avp_param && *ms_snd_time_avp_param) {
		_s.s = ms_snd_time_avp_param; _s.len = strlen(_s.s);
		if (pv_parse_spec(&_s, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n",
					ms_snd_time_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &ms_snd_time_avp_name,
					&ms_snd_time_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", ms_snd_time_avp_param);
			return -1;
		}
	} else {
		ms_snd_time_avp_name.n = 0;
		ms_snd_time_avp_type = 0;
	}

	db_con = msilo_dbf.init(ms_db_url);
	if (!db_con)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}
	_s.s = ms_db_table;
	_s.len = strlen(ms_db_table);
	ver =  table_version(&msilo_dbf, db_con, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LM_ERR("wrong version v%d for table <%s>,"
				" need v%d\n", ver, ms_db_table, S_TABLE_VERSION);
		return -1;
	}
	if(db_con)
		msilo_dbf.close(db_con);
	db_con = NULL;

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	ml = msg_list_init();
	if(ml==NULL)
	{
		LM_ERR("can't initialize msg list\n");
		return -1;
	}
	if(ms_check_time<0)
	{
		LM_ERR("bad check time value\n");
		return -1;
	}
	register_timer(m_clean_silo, 0, ms_check_time);
	if(ms_send_time>0 && ms_reminder.s!=NULL)
		register_timer(m_send_ontimer, 0, ms_send_time);

	if(ms_registrar.s!=NULL)
		ms_registrar.len = strlen(ms_registrar.s);
	if(ms_reminder.s!=NULL)
		ms_reminder.len = strlen(ms_reminder.s);
	if(ms_outbound_proxy.s!=NULL)
		ms_outbound_proxy.len = strlen(ms_outbound_proxy.s);

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	LM_DBG("rank #%d / pid <%d>\n", rank, getpid());
	if (msilo_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	db_con = msilo_dbf.init(ms_db_url);
	if (!db_con)
	{
		LM_ERR("child %d: failed to connect database\n", rank);
		return -1;
	}
	else
	{
		if (msilo_dbf.use_table(db_con, ms_db_table) < 0) {
			LM_ERR("child %d: failed in use_table\n", rank);
			return -1;
		}
		
		LM_DBG("#%d database connection opened successfully\n", rank);
	}
	return 0;
}

/**
 * store message
 * mode = "0" -- look for outgoing URI starting with new_uri
 * 		= "1" -- look for outgoing URI starting with r-uri
 * 		= "2" -- look for outgoing URI only at to header
 */

static int m_store(struct sip_msg* msg, char* owner, char* s2)
{
	str body, str_hdr, ctaddr;
	struct to_body to, *pto, *pfrom;
	struct sip_uri puri;
	str duri;
	db_key_t db_keys[NR_KEYS-1];
	db_val_t db_vals[NR_KEYS-1];
	db_key_t db_cols[1]; 
	db_res_t* res = NULL;
	
	int nr_keys = 0, val, lexpire;
	content_type_t ctype;
	static char buf[512];
	static char buf1[1024];
	int mime;
	int printbuf_len;

	int_str        avp_value;
	struct usr_avp *avp;

	LM_DBG("------------ start ------------\n");

	/* get message body - after that whole SIP MESSAGE is parsed */
	body.s = get_body( msg );
	if (body.s==0) 
	{
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}
	
	/* content-length (if present) must be already parsed */
	if (!msg->content_length) 
	{
		LM_ERR("no Content-Length header found!\n");
		goto error;
	}
	body.len = get_content_length( msg );

	/* check if the body of message contains something */
	if(body.len <= 0)
	{
		LM_ERR("body of the message is empty!\n");
		goto error;
	}
	
	/* get TO URI */
	if(!msg->to || !msg->to->body.s)
	{
	    LM_ERR("cannot find 'to' header!\n");
	    goto error;
	}
	
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("the 'To' header ALREADY PARSED: <%.*s>\n",
			pto->uri.len, pto->uri.s );	
	}
	else
	{
		LM_DBG("the 'To' header NOT PARSED ->parsing ...\n");
		memset( &to , 0, sizeof(to) );
		parse_to(msg->to->body.s, msg->to->body.s+msg->to->body.len+1, &to);
		if(to.uri.len > 0) /* && to.error == PARSE_OK) */
		{
			LM_DBG("'To' parsed OK <%.*s>.\n", to.uri.len, to.uri.s);
			pto = &to;
		}
		else
		{
			LM_ERR("'To' cannot be parsed\n");
			goto error;
		}
	}
	
	if(pto->uri.len == ms_registrar.len && 
			!strncasecmp(pto->uri.s, ms_registrar.s, ms_registrar.len))
	{
		LM_DBG("message to MSILO REGISTRAR!\n");
		goto error;
	}

	/* get the owner */
	memset(&puri, 0, sizeof(struct sip_uri));
	if(owner)
	{
		printbuf_len = MSILO_PRINTBUF_SIZE-1;
		if(pv_printf(msg, (pv_elem_t*)owner, msilo_printbuf, &printbuf_len)<0)
		{
			LM_ERR("cannot print the format\n");
			return -1;
		}
		if(parse_uri(msilo_printbuf, printbuf_len, &puri)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto error;
		} else {
			LM_DBG("using user id [%.*s]\n", printbuf_len, msilo_printbuf);
		}
	} else { /* get it from R-URI */
		if(msg->new_uri.len <= 0)
		{
			if(msg->first_line.u.request.uri.len <= 0)
			{
				LM_ERR("bad dst URI!\n");
				goto error;
			}
			duri = msg->first_line.u.request.uri;
		} else {
			duri = msg->new_uri;
		}
		LM_DBG("NEW R-URI found - check if is AoR!\n");
		if(parse_uri(duri.s, duri.len, &puri)!=0)
		{
			LM_ERR("bad dst R-URI!!\n");
			goto error;
		}
	}
	if(puri.user.len<=0)
	{
		LM_ERR("no username for owner\n");
		goto error;
	}
	
	db_keys[nr_keys] = sc_uri_user;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri.user.s;
	db_vals[nr_keys].val.str_val.len = puri.user.len;

	nr_keys++;

	db_keys[nr_keys] = sc_uri_host;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri.host.s;
	db_vals[nr_keys].val.str_val.len = puri.host.len;

	nr_keys++;

	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		goto error;
	}

	if (ms_max_messages > 0) {
	    db_cols[0] = sc_inc_time;
	    if (msilo_dbf.query(db_con, db_keys, 0, db_vals, db_cols,
				2, 1, 0, &res) < 0 ) {
			LM_ERR("failed to query the database\n");
			return -1;
	    }
	    if (RES_ROW_N(res) >= ms_max_messages) {
			LM_ERR("too many messages for AoR '%.*s@%.*s'\n",
			    puri.user.len, puri.user.s, puri.host.len, puri.host.s);
 	        msilo_dbf.free_result(db_con, res);
		return -1;
	    }
	    msilo_dbf.free_result(db_con, res);
	}

	/* Set To key */
	db_keys[nr_keys] = sc_to;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pto->uri.s;
	db_vals[nr_keys].val.str_val.len = pto->uri.len;

	nr_keys++;

	/* check FROM URI */
	if(!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		goto error;
	}

	if(msg->from->parsed == NULL)
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
	LM_DBG("'From' header: <%.*s>\n", pfrom->uri.len, pfrom->uri.s);	
	
	if(ms_registrar.s && pfrom->uri.len == ms_registrar.len && 
			!strncasecmp(pfrom->uri.s, ms_registrar.s, ms_registrar.len))
	{
		LM_DBG("message from MSILO REGISTRAR!\n");
		goto error;
	}

	db_keys[nr_keys] = sc_from;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pfrom->uri.s;
	db_vals[nr_keys].val.str_val.len = pfrom->uri.len;

	nr_keys++;

	/* add the message's body in SQL query */
	
	db_keys[nr_keys] = sc_body;
	
	db_vals[nr_keys].type = DB_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val.s = body.s;
	db_vals[nr_keys].val.blob_val.len = body.len;

	nr_keys++;
	
	lexpire = ms_expire_time;
	/* add 'content-type' -- parse the content-type header */
	if ((mime=parse_content_type_hdr(msg))<1 ) 
	{
		LM_ERR("cannot parse Content-Type header\n");
		goto error;
	}

	db_keys[nr_keys]      = sc_ctype;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul  = 0;
	db_vals[nr_keys].val.str_val.s   = "text/plain";
	db_vals[nr_keys].val.str_val.len = 10;
	
	/** check the content-type value */
	if( mime!=(TYPE_TEXT<<16)+SUBTYPE_PLAIN
		&& mime!=(TYPE_MESSAGE<<16)+SUBTYPE_CPIM )
	{
		if(m_extract_content_type(msg->content_type->body.s, 
				msg->content_type->body.len, &ctype, CT_TYPE) != -1)
		{
			LM_DBG("'content-type' found\n");
			db_vals[nr_keys].val.str_val.s   = ctype.type.s;
			db_vals[nr_keys].val.str_val.len = ctype.type.len;
		}
	}
	nr_keys++;

	/* check 'expires' -- no more parsing - already done by get_body() */
	if(msg->expires && msg->expires->body.len > 0)
	{
		LM_DBG("'expires' found\n");
		val = atoi(msg->expires->body.s);
		if(val > 0)
			lexpire = (ms_expire_time<=val)?ms_expire_time:val;
	}

	/* current time */
	val = (int)time(NULL);
	
	/* add expiration time */
	db_keys[nr_keys] = sc_exp_time;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val+lexpire;
	nr_keys++;

	/* add incoming time */
	db_keys[nr_keys] = sc_inc_time;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val;
	nr_keys++;

	/* add sending time */
	db_keys[nr_keys] = sc_snd_time;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = 0;
	if(ms_snd_time_avp_name.n!=0)
	{
		avp = NULL;
		avp=search_first_avp(ms_snd_time_avp_type, ms_snd_time_avp_name,
				&avp_value, 0);
		if(avp!=NULL && is_avp_str_val(avp))
		{
			if(ms_extract_time(&avp_value.s, &db_vals[nr_keys].val.int_val)!=0)
				db_vals[nr_keys].val.int_val = 0;
		}
	}
	nr_keys++;

	if(msilo_dbf.insert(db_con, db_keys, db_vals, nr_keys) < 0)
	{
		LM_ERR("failed to store message\n");
		goto error;
	}
	LM_DBG("message stored. T:<%.*s> F:<%.*s>\n",
		pto->uri.len, pto->uri.s, pfrom->uri.len, pfrom->uri.s);
	
#ifdef STATISTICS
	update_stat(ms_stored_msgs, 1);
#endif

	if(ms_registrar.len <= 0
			|| ms_registrar.len+CONTACT_PREFIX_LEN+CONTACT_SUFFIX_LEN+1>=1024)
		goto done;

	LM_DBG("sending info message.\n");
	strcpy(buf1, CONTACT_PREFIX);
	strncat(buf1,ms_registrar.s,ms_registrar.len);
	strncat(buf1, CONTACT_SUFFIX, CONTACT_SUFFIX_LEN);
	str_hdr.len = CONTACT_PREFIX_LEN+ms_registrar.len+CONTACT_SUFFIX_LEN;
	str_hdr.s = buf1;

	strncpy(buf, "User [", 6);
	body.len = 6;
	if(pto->uri.len+OFFLINE_MESSAGE_LEN+7/*6+1*/ < 512)
	{
		strncpy(buf+body.len, pto->uri.s, pto->uri.len);
		body.len += pto->uri.len;
	}
	strncpy(buf+body.len, OFFLINE_MESSAGE, OFFLINE_MESSAGE_LEN);
	body.len += OFFLINE_MESSAGE_LEN;
	body.s = buf;
	/* look for Contact header -- must be parsed by now*/
	ctaddr.s = NULL;
	if(ms_use_contact && msg->contact!=NULL && msg->contact->body.s!=NULL
			&& msg->contact->body.len > 0)
	{
		LM_DBG("contact header found\n");
		if((msg->contact->parsed!=NULL 
			&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL)
			|| (parse_contact(msg->contact)==0
			&& msg->contact->parsed!=NULL
			&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL))
		{
			LM_DBG("using contact header for info msg\n");
			ctaddr.s = 
			((contact_body_t*)(msg->contact->parsed))->contacts->uri.s;
			ctaddr.len =
			((contact_body_t*)(msg->contact->parsed))->contacts->uri.len;
		
			if(!ctaddr.s || ctaddr.len < 6 || strncmp(ctaddr.s, "sip:", 4)
				|| ctaddr.s[4]==' ')
				ctaddr.s = NULL;
			else
				LM_DBG("feedback contact [%.*s]\n",	ctaddr.len,ctaddr.s);
		}
	}
		
	tmb.t_request(&msg_type,  /* Type of the message */
			(ctaddr.s)?&ctaddr:&pfrom->uri,    /* Request-URI */
			&pfrom->uri,      /* To */
			&ms_registrar,    /* From */
			&str_hdr,         /* Optional headers including CRLF */
			&body,            /* Message body */
			(ms_outbound_proxy.s)?&ms_outbound_proxy:0, /* outbound uri */
			NULL,             /* Callback function */
			NULL              /* Callback parameter */
		);

done:
	return 1;
error:
	return -1;
}

/**
 * dump message
 */
static int m_dump(struct sip_msg* msg, char* owner, char* str2)
{
	struct to_body to, *pto = NULL;
	db_key_t db_keys[3];
	db_key_t ob_key;
	db_op_t  db_ops[3];
	db_val_t db_vals[3];
	db_key_t db_cols[6];
	db_res_t* db_res = NULL;
	int i, db_no_cols = 6, db_no_keys = 3, mid, n;
	static char hdr_buf[1024];
	static char body_buf[1024];
	struct sip_uri puri;

	str str_vals[4], hdr_str , body_str;
	time_t rtime;
	int printbuf_len;
	
	/* init */
	ob_key = sc_mid;

	db_keys[0]=sc_uri_user;
	db_keys[1]=sc_uri_host;
	db_keys[2]=sc_snd_time;
	db_ops[0]=OP_EQ;
	db_ops[1]=OP_EQ;
	db_ops[2]=OP_EQ;

	db_cols[0]=sc_mid;
	db_cols[1]=sc_from;
	db_cols[2]=sc_to;
	db_cols[3]=sc_body;
	db_cols[4]=sc_ctype;
	db_cols[5]=sc_inc_time;

	
	LM_DBG("------------ start ------------\n");
	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;
	
	/* check for TO header */
	if(msg->to==NULL && (parse_headers(msg, HDR_TO_F, 0)==-1
				|| msg->to==NULL || msg->to->body.s==NULL))
	{
		LM_ERR("cannot find TO HEADER!\n");
		goto error;
	}

	/* get TO header URI */
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",
			pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &to , 0, sizeof(to) );
		parse_to(msg->to->body.s,
			msg->to->body.s + msg->to->body.len + 1, &to);
		if(to.uri.len <= 0) /* || to.error != PARSE_OK) */
		{
			LM_ERR("'To' header NOT parsed\n");
			goto error;
		}
		pto = &to;
	}

	/**
	 * check if has expires=0 (REGISTER)
	 */
	if(parse_headers(msg, HDR_EXPIRES_F, 0) >= 0)
	{
		/* check 'expires' > 0 */
		if(msg->expires && msg->expires->body.len > 0)
		{
			i = atoi(msg->expires->body.s);
			if(i <= 0)
			{ /* user goes offline */
				LM_DBG("user <%.*s> goes offline - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
				goto error;
			}
			else
				LM_DBG("user <%.*s> online - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
		}
	}
	else
	{
		LM_ERR("failed to parse 'expires'\n");
		goto error;
	}

	if (check_message_support(msg)!=0) {
	    LM_DBG("MESSAGE method not supported\n");
	    return -1;
	}
	 
	/* get the owner */
	memset(&puri, 0, sizeof(struct sip_uri));
	if(owner)
	{
		printbuf_len = MSILO_PRINTBUF_SIZE-1;
		if(pv_printf(msg, (pv_elem_t*)owner, msilo_printbuf, &printbuf_len)<0)
		{
			LM_ERR("cannot print the format\n");
			return -1;
		}
		if(parse_uri(msilo_printbuf, printbuf_len, &puri)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto error;
		} else {
			LM_DBG("using user id [%.*s]\n", printbuf_len,
					msilo_printbuf);
		}
	} else { /* get it from  To URI */
		if(parse_uri(pto->uri.s, pto->uri.len, &puri)!=0)
		{
			LM_ERR("bad owner To URI!\n");
			goto error;
		}
	}
	if(puri.user.len<=0 || puri.user.s==NULL
			|| puri.host.len<=0 || puri.host.s==NULL)
	{
		LM_ERR("bad owner URI!\n");
		goto error;
	}

	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = puri.user.s;
	db_vals[0].val.str_val.len = puri.user.len;

	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = puri.host.s;
	db_vals[1].val.str_val.len = puri.host.len;

	db_vals[2].type = DB_INT;
	db_vals[2].nul = 0;
	db_vals[2].val.int_val = 0;
	
	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		goto error;
	}

	if((msilo_dbf.query(db_con,db_keys,db_ops,db_vals,db_cols,db_no_keys,
				db_no_cols, ob_key, &db_res)!=0) || (RES_ROW_N(db_res) <= 0))
	{
		LM_DBG("no stored message for <%.*s>!\n", pto->uri.len,	pto->uri.s);
		goto done;
	}
		
	LM_DBG("dumping [%d] messages for <%.*s>!!!\n", 
			RES_ROW_N(db_res), pto->uri.len, pto->uri.s);

	for(i = 0; i < RES_ROW_N(db_res); i++) 
	{
		mid =  RES_ROWS(db_res)[i].values[0].val.int_val;
		if(msg_list_check_msg(ml, mid))
		{
			LM_DBG("message[%d] mid=%d already sent.\n", i, mid);
			continue;
		}
		
		memset(str_vals, 0, 4*sizeof(str));
		SET_STR_VAL(str_vals[0], db_res, i, 1); /* from */
		SET_STR_VAL(str_vals[1], db_res, i, 2); /* to */
		SET_STR_VAL(str_vals[2], db_res, i, 3); /* body */
		SET_STR_VAL(str_vals[3], db_res, i, 4); /* ctype */
		rtime = 
			(time_t)RES_ROWS(db_res)[i].values[5/*inc time*/].val.int_val;

		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				str_vals[0]/*from*/, rtime /*Date*/) < 0)
		{
			LM_ERR("headers building failed [%d]\n", mid);
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				LM_ERR("failed to free the query result\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			goto error;
		}
			
		LM_DBG("msg [%d-%d] for: %.*s\n", i+1, mid,	pto->uri.len, pto->uri.s);
			
		/** sending using TM function: t_uac */
		body_str.len = 1024;
		n = m_build_body(&body_str, rtime, str_vals[2/*body*/], 0);
		if(n<0)
			LM_DBG("sending simple body\n");
		else
			LM_DBG("sending composed body\n");
		
			tmb.t_request(&msg_type,  /* Type of the message */
					&str_vals[1],     /* Request-URI (To) */
					&str_vals[1],     /* To */
					&str_vals[0],     /* From */
					&hdr_str,         /* Optional headers including CRLF */
					(n<0)?&str_vals[2]:&body_str, /* Message body */
					(ms_outbound_proxy.s)?&ms_outbound_proxy:0,
									/* outbound uri */
					m_tm_callback,    /* Callback function */
					(void*)(long)mid  /* Callback parameter */
				);
	}

done:
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (db_res!=NULL && msilo_dbf.free_result(db_con, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return 1;
error:
	return -1;
}

/**
 * - cleaning up the messages that got reply
 * - delete expired messages from database
 */
void m_clean_silo(unsigned int ticks, void *param)
{
	msg_list_el mle = NULL, p;
	db_key_t db_keys[MAX_DEL_KEYS];
	db_val_t db_vals[MAX_DEL_KEYS];
	db_op_t  db_ops[1] = { OP_LEQ };
	int n;
	
	LM_DBG("cleaning stored messages - %d\n", ticks);
	
	msg_list_check(ml);
	mle = p = msg_list_reset(ml);
	n = 0;
	while(p)
	{
		if(p->flag & MS_MSG_DONE)
		{
#ifdef STATISTICS
			if(p->flag & MS_MSG_TSND)
				update_stat(ms_dumped_msgs, 1);
			else
				update_stat(ms_dumped_rmds, 1);
#endif

			db_keys[n] = sc_mid;
			db_vals[n].type = DB_INT;
			db_vals[n].nul = 0;
			db_vals[n].val.int_val = p->msgid;
			LM_DBG("cleaning sent message [%d]\n", p->msgid);
			n++;
			if(n==MAX_DEL_KEYS)
			{
				if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0) 
					LM_ERR("failed to clean %d messages.\n",n);
				n = 0;
			}
		}
		if((p->flag & MS_MSG_ERRO) && (p->flag & MS_MSG_TSND))
		{ /* set snd time to 0 */
			ms_reset_stime(p->msgid);
#ifdef STATISTICS
			update_stat(ms_failed_rmds, 1);
#endif

		}
#ifdef STATISTICS
		if((p->flag & MS_MSG_ERRO) && !(p->flag & MS_MSG_TSND))
			update_stat(ms_failed_msgs, 1);
#endif
		p = p->next;
	}
	if(n>0)
	{
		if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0) 
			LM_ERR("failed to clean %d messages\n", n);
		n = 0;
	}

	msg_list_el_free_all(mle);
	
	/* cleaning expired messages */
	if(ticks%(ms_check_time*ms_clean_period)<ms_check_time)
	{
		LM_DBG("cleaning expired messages\n");
		db_keys[0] = sc_exp_time;
		db_vals[0].type = DB_INT;
		db_vals[0].nul = 0;
		db_vals[0].val.int_val = (int)time(NULL);
		if (msilo_dbf.delete(db_con, db_keys, db_ops, db_vals, 1) < 0) 
			LM_DBG("ERROR cleaning expired messages\n");
	}
}


/**
 * destroy function
 */
void destroy(void)
{
	LM_DBG("msilo destroy module ...\n");
	msg_list_free(ml);

	if(db_con && msilo_dbf.close)
		msilo_dbf.close(db_con);
}

/** 
 * TM callback function - delete message from database if was sent OK
 */
void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("message id not received\n");
		goto done;
	}
	
	LM_DBG("completed with status %d [mid: %ld/%d]\n",
		ps->code, (long)ps->param, *((int*)ps->param));
	if(!db_con)
	{
		LM_ERR("db_con is NULL\n");
		goto done;
	}
	if(ps->code >= 300)
	{
		LM_DBG("message <%d> was not sent successfully\n", *((int*)ps->param));
		msg_list_set_flag(ml, *((int*)ps->param), MS_MSG_ERRO);
		goto done;
	}

	LM_DBG("message <%d> was sent successfully\n", *((int*)ps->param));
	msg_list_set_flag(ml, *((int*)ps->param), MS_MSG_DONE);

done:
	return;
}

void m_send_ontimer(unsigned int ticks, void *param)
{
	db_key_t db_keys[2];
	db_op_t  db_ops[2];
	db_val_t db_vals[2];
	db_key_t db_cols[6];
	db_res_t* db_res = NULL;
	int i, db_no_cols = 6, db_no_keys = 2, mid, n;
	static char hdr_buf[1024];
	static char uri_buf[1024];
	static char body_buf[1024];
	str puri;
	time_t ttime;

	str str_vals[4], hdr_str , body_str;
	time_t stime;

	if(ms_reminder.s==NULL)
	{
		LM_WARN("reminder address null\n");
		return;
	}
	
	/* init */
	db_keys[0]=sc_snd_time;
	db_keys[1]=sc_snd_time;
	db_ops[0]=OP_NEQ;
	db_ops[1]=OP_LEQ;

	db_cols[0]=sc_mid;
	db_cols[1]=sc_uri_user;
	db_cols[2]=sc_uri_host;
	db_cols[3]=sc_body;
	db_cols[4]=sc_ctype;
	db_cols[5]=sc_snd_time;

	
	LM_DBG("------------ start ------------\n");
	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;
	
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = 0;
	
	db_vals[1].type = DB_INT;
	db_vals[1].nul = 0;
	ttime = time(NULL);
	db_vals[1].val.int_val = (int)ttime;
	
	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return;
	}

	if((msilo_dbf.query(db_con,db_keys,db_ops,db_vals,db_cols,db_no_keys,
				db_no_cols, NULL,&db_res)!=0) || (RES_ROW_N(db_res) <= 0))
	{
		LM_DBG("no message for <%.*s>!\n", 24, ctime((const time_t*)&ttime));
		goto done;
	}
		
	LM_DBG("dumping [%d] messages for <%.*s>!!!\n", RES_ROW_N(db_res), 24,
			ctime((const time_t*)&ttime));

	for(i = 0; i < RES_ROW_N(db_res); i++) 
	{
		mid =  RES_ROWS(db_res)[i].values[0].val.int_val;
		if(msg_list_check_msg(ml, mid))
		{
			LM_DBG("message[%d] mid=%d already sent.\n", i, mid);
			continue;
		}
		
		memset(str_vals, 0, 4*sizeof(str));
		SET_STR_VAL(str_vals[0], db_res, i, 1); /* user */
		SET_STR_VAL(str_vals[1], db_res, i, 2); /* host */
		SET_STR_VAL(str_vals[2], db_res, i, 3); /* body */
		SET_STR_VAL(str_vals[3], db_res, i, 4); /* ctype */

		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				ms_reminder/*from*/,0/*Date*/) < 0)
		{
			LM_ERR("headers building failed [%d]\n", mid);
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				LM_DBG("failed to free result of query\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			return;
		}

		puri.s = uri_buf;
		puri.len = 4 + str_vals[0].len + 1 + str_vals[1].len;
		memcpy(puri.s, "sip:", 4);
		memcpy(puri.s+4, str_vals[0].s, str_vals[0].len);
		puri.s[4+str_vals[0].len] = '@';
		memcpy(puri.s+4+str_vals[0].len+1, str_vals[1].s, str_vals[1].len);
		
		LM_DBG("msg [%d-%d] for: %.*s\n", i+1, mid,	puri.len, puri.s);
			
		/** sending using TM function: t_uac */
		body_str.len = 1024;
		stime = 
			(time_t)RES_ROWS(db_res)[i].values[5/*snd time*/].val.int_val;
		n = m_build_body(&body_str, 0, str_vals[2/*body*/], stime);
		if(n<0)
			LM_DBG("sending simple body\n");
		else
			LM_DBG("sending composed body\n");
		
		msg_list_set_flag(ml, mid, MS_MSG_TSND);
		
		tmb.t_request(&msg_type,  /* Type of the message */
					&puri,            /* Request-URI */
					&puri,            /* To */
					&ms_reminder,     /* From */
					&hdr_str,         /* Optional headers including CRLF */
					(n<0)?&str_vals[2]:&body_str, /* Message body */
					(ms_outbound_proxy.s)?&ms_outbound_proxy:0,
							/* outbound uri */
					m_tm_callback,    /* Callback function */
					(void*)(long)mid  /* Callback parameter */
				);
	}

done:
	/**
	 * Free the result because we don't need it anymore
	 */
	if (db_res!=NULL && msilo_dbf.free_result(db_con, db_res) < 0)
		LM_DBG("failed to free result of query\n");

	return;
}

int ms_reset_stime(int mid)
{
	db_key_t db_keys[1];
	db_op_t  db_ops[1];
	db_val_t db_vals[1];
	db_key_t db_cols[1];
	db_val_t db_cvals[1];
	
	db_keys[0]=sc_mid;
	db_ops[0]=OP_EQ;

	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = mid;
	

	db_cols[0]=sc_snd_time;
	db_cvals[0].type = DB_INT;
	db_cvals[0].nul = 0;
	db_cvals[0].val.int_val = 0;
	
	LM_DBG("updating send time for [%d]!\n", mid);
	
	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if(msilo_dbf.update(db_con,db_keys,db_ops,db_vals,db_cols,db_cvals,1,1)!=0)
	{
		LM_ERR("failed to make update for [%d]!\n",	mid);
		return -1;
	}
	return 0;
}

/*
 * Check if REGISTER request has contacts that support MESSAGE method or
 * if MESSAGE method is listed in Allow header and contact does not have 
 * methods parameter.
 */
int check_message_support(struct sip_msg* msg)
{
	contact_t* c;
	unsigned int allow_message = 0;
	unsigned int allow_hdr = 0;
	str *methods_body;
	unsigned int methods;

	/* Parse all headers in order to see all Allow headers */
	if (parse_headers(msg, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if (parse_allow(msg) == 0)
	{
		allow_hdr = 1;
		allow_message = get_allow_methods(msg) & METHOD_MESSAGE;
	}
	LM_DBG("Allow message: %u\n", allow_message);

	if (!msg->contact)
	{
		LM_ERR("no Contact found\n");
		return -1;
	}
	if (parse_contact(msg->contact) < 0)
	{
		LM_ERR("failed to parse Contact HF\n");
		return -1;
	}
	if (((contact_body_t*)msg->contact->parsed)->star)
	{
		LM_DBG("* Contact found\n");
		return -1;
	}

	if (contact_iterator(&c, msg, 0) < 0)
		return -1;

	/* 
	 * Check contacts for MESSAGE method in methods parameter list
	 * If contact does not have methods parameter, use Allow header methods,
	 * if any.  Stop if MESSAGE method is found.
	 */
	while(c)
	{
		if (c->methods)
		{
			methods_body = &(c->methods->body);
			if (parse_methods(methods_body, &methods) < 0)
			{
				LM_ERR("failed to parse contact methods\n");
				return -1;
			}
			if (methods & METHOD_MESSAGE)
			{
				LM_DBG("MESSAGE contact found\n");
				return 0;
			}
		} else {
			if (allow_message)
			{
				LM_DBG("MESSAGE found in Allow Header\n");
				return 0;
			}
		}
		if (contact_iterator(&c, msg, c) < 0)
		{
			LM_DBG("MESSAGE contact not found\n");
			return -1;
		}
	}
	/* no Allow header and no methods in Contact => dump MESSAGEs */
	if(allow_hdr==0)
		return 0;
	return -1;
}

static int fixup_msilo(void** param, int param_no)
{
	pv_elem_t *model;
	str s;
	if(*param)
	{
		s.s = (char*)(*param); s.len =  strlen(s.s);
		if(pv_parse_format(&s, &model)<0)
		{
			LM_ERR("wrong format[%s]\n", (char*)(*param));
			return E_UNSPEC;
		}
			
		*param = (void*)model;
		return 0;
	}
	LM_ERR("null format\n");
	return E_UNSPEC;
}

