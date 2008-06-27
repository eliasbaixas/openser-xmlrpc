/*
 * $Id: acc_mod.c 3270 2007-12-06 12:44:15Z bogdan_iancu $
 * 
 * Accounting module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * -------
 * 2003-03-06: aligned to change in callback names (jiri)
 * 2003-03-06: fixed improper sql connection, now from 
 * 	           child_init (jiri)
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-04-06: Opens database connection in child_init only (janakj)
 * 2003-04-24  parameter validation (0 t->uas.request) added (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2003-12-04  global TM callbacks switched to per transaction callbacks
 *             (bogdan)
 * 2004-06-06  db cleanup: static db_url, calls to acc_db_{bind,init,close)
 *             (andrei)
 * 2005-05-30  acc_extra patch commited (ramona)
 * 2005-06-28  multi leg call support added (bogdan)
 * 2006-01-13  detect_direction (for sequential requests) added (bogdan)
 * 2006-09-08  flexible multi leg accounting support added (bogdan)
 * 2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../tm/tm_load.h"
#include "../rr/api.h"
#include "acc.h"
#include "acc_mod.h"
#include "acc_extra.h"
#include "acc_logic.h"

#ifdef RAD_ACC
#include "../../radius.h"
#endif

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "diam_tcp.h"
#endif

MODULE_VERSION

struct tm_binds tmb;
struct rr_binds rrb;

static int mod_init( void );
static void destroy(void);
static int child_init(int rank);


/* ----- General purpose variables ----------- */

/* what would you like to report on */
/* should early media replies (183) be logged ? default==no */
int early_media = 0;
/* would you like us to report CANCELs from upstream too? */
int report_cancels = 0;
/* report e2e ACKs too */
int report_ack = 0;
/* detect and correct direction in the sequential requests */
int detect_direction = 0;
/* should failed replies (>=3xx) be logged ? default==no */
int failed_transaction_flag = -1;
/* multi call-leg support */
static char* leg_info_str = 0;
struct acc_extra *leg_info = 0;


/* ----- SYSLOG acc variables ----------- */

int log_flag = -1;
int log_missed_flag = -1;
/* noisiness level logging facilities are used */
int log_level = L_NOTICE;
/* log extra variables */
static char *log_extra_str = 0;
struct acc_extra *log_extra = 0;


/* ----- RADIUS acc variables ----------- */

#ifdef RAD_ACC
static char *radius_config = DEFAULT_RADIUSCLIENT_CONF;
int radius_flag = -1;
int radius_missed_flag = -1;
static int service_type = -1;
void *rh;
/* rad extra variables */
static char *rad_extra_str = 0;
struct acc_extra *rad_extra = 0;
#endif


/* ----- DIAMETER acc variables ----------- */

#ifdef DIAM_ACC
int diameter_flag = -1;
int diameter_missed_flag = -1;
/* diameter extra variables */
static char *dia_extra_str = 0;
struct acc_extra *dia_extra = 0;
/* buffer used to read from TCP connection*/
rd_buf_t *rb;
char* diameter_client_host="localhost";
int diameter_client_port=3000;
#endif


/* ----- SQL acc variables ----------- */

#ifdef SQL_ACC
int db_flag = -1;
int db_missed_flag = -1;
/* db extra variables */
static char *db_extra_str = 0;
struct acc_extra *db_extra = 0;
/* Database url */
static char *db_url = 0;
/* name of database tables */
char *db_table_acc = "acc";
char *db_table_mc = "missed_calls";
/* names of columns in tables acc/missed calls*/
char* acc_method_col     = "method";
char* acc_fromtag_col    = "from_tag";
char* acc_totag_col      = "to_tag";
char* acc_callid_col     = "callid";
char* acc_sipcode_col    = "sip_code";
char* acc_sipreason_col  = "sip_reason";
char* acc_time_col       = "time";
#endif

/* ------------- fixup function --------------- */
static int acc_fixup(void** param, int param_no);
static int free_acc_fixup(void** param, int param_no);


static cmd_export_t cmds[] = {
	{"acc_log_request", w_acc_log_request, 1, acc_fixup, free_acc_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#ifdef SQL_ACC
	{"acc_db_request",  w_acc_db_request,  2, acc_fixup, free_acc_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef RAD_ACC
	{"acc_rad_request", w_acc_rad_request, 1, acc_fixup, free_acc_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef DIAM_ACC
	{"acc_diam_request",w_acc_diam_request,1, acc_fixup, free_acc_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
	{0, 0, 0, 0, 0, 0}
};



static param_export_t params[] = {
	{"early_media",             INT_PARAM, &early_media             },
	{"failed_transaction_flag", INT_PARAM, &failed_transaction_flag },
	{"report_ack",              INT_PARAM, &report_ack              },
	{"report_cancels",          INT_PARAM, &report_cancels          },
	{"multi_leg_info",          STR_PARAM, &leg_info_str            },
	{"detect_direction",        INT_PARAM, &detect_direction        },
	/* syslog specific */
	{"log_flag",             INT_PARAM, &log_flag             },
	{"log_missed_flag",      INT_PARAM, &log_missed_flag      },
	{"log_level",            INT_PARAM, &log_level            },
	{"log_extra",            STR_PARAM, &log_extra_str        },
#ifdef RAD_ACC
	{"radius_config",        STR_PARAM, &radius_config        },
	{"radius_flag",          INT_PARAM, &radius_flag          },
	{"radius_missed_flag",   INT_PARAM, &radius_missed_flag   },
	{"service_type",         INT_PARAM, &service_type         },
	{"radius_extra",         STR_PARAM, &rad_extra_str        },
#endif
	/* DIAMETER specific */
#ifdef DIAM_ACC
	{"diameter_flag",        INT_PARAM, &diameter_flag        },
	{"diameter_missed_flag", INT_PARAM, &diameter_missed_flag },
	{"diameter_client_host", STR_PARAM, &diameter_client_host },
	{"diameter_client_port", INT_PARAM, &diameter_client_port },
	{"diameter_extra",       STR_PARAM, &dia_extra_str        },
#endif
	/* db-specific */
#ifdef SQL_ACC
	{"db_flag",              INT_PARAM, &db_flag              },
	{"db_missed_flag",       INT_PARAM, &db_missed_flag       },
	{"db_extra",             STR_PARAM, &db_extra_str         },
	{"db_url",               STR_PARAM, &db_url               },
	{"db_table_acc",         STR_PARAM, &db_table_acc         },
	{"db_table_missed_calls",STR_PARAM, &db_table_mc          },
	{"acc_method_column",    STR_PARAM, &acc_method_col       },
	{"acc_from_tag_column",  STR_PARAM, &acc_fromtag_col      },
	{"acc_to_tag_column",    STR_PARAM, &acc_totag_col        },
	{"acc_callid_column",    STR_PARAM, &acc_callid_col       },
	{"acc_sip_code_column",  STR_PARAM, &acc_sipcode_col      },
	{"acc_sip_reason_column",STR_PARAM, &acc_sipreason_col    },
	{"acc_time_column",      STR_PARAM, &acc_time_col         },
#endif
	{0,0,0}
};


struct module_exports exports= {
	"acc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	params,     /* exported params */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* initialization module */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* per-child init function */
};



/************************** FIXUP functions ****************************/


static int acc_fixup(void** param, int param_no)
{
	struct acc_param *accp;
	char *p;

	p = (char*)*param;
	if (p==0 || p[0]==0) {
		LM_ERR("first parameter is empty\n");
		return E_SCRIPT;
	}

	if (param_no == 1) {
		accp = (struct acc_param*)pkg_malloc(sizeof(struct acc_param));
		if (!accp) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( accp, 0, sizeof(struct acc_param));
		accp->reason.s = p;
		accp->reason.len = strlen(p);
		/* any code? */
		if (accp->reason.len>=3 && isdigit((int)p[0])
		&& isdigit((int)p[1]) && isdigit((int)p[2]) ) {
			accp->code = (p[0]-'0')*100 + (p[1]-'0')*10 + (p[2]-'0');
			accp->code_s.s = p;
			accp->code_s.len = 3;
			accp->reason.s += 3;
			for( ; isspace((int)accp->reason.s[0]) ; accp->reason.s++ );
			accp->reason.len = strlen(accp->reason.s);
		}
		*param = (void*)accp;
#ifdef SQL_ACC
	} else if (param_no == 2) {
		/* only for db acc - the table name */
		if (db_url==0) {
			pkg_free(p);
			*param = 0;
		}
#endif
	}
	return 0;
}

static int free_acc_fixup(void** param, int param_no)
{
	if(*param)
	{
		pkg_free(*param);
		*param = 0;
	}
	return 0;
}



/************************** INTERFACE functions ****************************/

static int mod_init( void )
{
	LM_INFO("initializing...\n");

	/* ----------- GENERIC INIT SECTION  ----------- */

	if (flag_idx2mask(&failed_transaction_flag)<0)
		return -1;

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	/* if detect_direction is enabled, load rr also */
	if (detect_direction) {
		if (load_rr_api(&rrb)!=0) {
			LM_ERR("can't load RR API\n");
			return -1;
		}
		/* we need the append_fromtag on in RR */
		if (!rrb.append_fromtag) {
			LM_ERR("'append_fromtag' RR param is not enabled!"
				" - required by 'detect_direction'\n");
			return -1;
		}
	}

	/* listen for all incoming requests  */
	if ( tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, acc_onreq, 0 ) <=0 ) {
		LM_ERR("cannot register TMCB_REQUEST_IN callback\n");
		return -1;
	}

	/* init the extra engine */
	init_acc_extra();

	/* configure multi-leg accounting */
	if (leg_info_str && (leg_info=parse_acc_leg(leg_info_str))==0 ) {
		LM_ERR("failed to parse multileg_info param\n");
		return -1;
	}

	/* ----------- SYSLOG INIT SECTION ----------- */

	/* parse the extra string, if any */
	if (log_extra_str && (log_extra=parse_acc_extra(log_extra_str))==0 ) {
		LM_ERR("failed to parse log_extra param\n");
		return -1;
	}

	if (flag_idx2mask(&log_flag)<0)
		return -1;

	if (flag_idx2mask(&log_missed_flag)<0)
		return -1;

	acc_log_init();

	/* ------------ SQL INIT SECTION ----------- */

#ifdef SQL_ACC
	if (db_url && db_url[0]) {
		/* parse the extra string, if any */
		if (db_extra_str && (db_extra=parse_acc_extra(db_extra_str))==0 ) {
			LM_ERR("failed to parse db_extra param\n");
			return -1;
		}
		if (acc_db_init(db_url)<0){
			LM_ERR("failed...did you load a database module?\n");
			return -1;
		}
		/* fix the flags */
		if (flag_idx2mask(&db_flag)<0)
			return -1;
		if (flag_idx2mask(&db_missed_flag)<0)
			return -1;
	} else {
		db_url = 0;
		db_flag = 0;
		db_missed_flag = 0;
	}
#endif

	/* ------------ RADIUS INIT SECTION ----------- */

#ifdef RAD_ACC
	if (radius_config && radius_config[0]) {
		/* parse the extra string, if any */
		if (rad_extra_str && (rad_extra=parse_acc_extra(rad_extra_str))==0 ) {
			LM_ERR("failed to parse rad_extra param\n");
			return -1;
		}

		/* fix the flags */
		if (flag_idx2mask(&radius_flag)<0)
			return -1;
		if (flag_idx2mask(&radius_missed_flag)<0)
			return -1;
		if (init_acc_rad( radius_config, service_type)!=0 ) {
			LM_ERR("failed to init radius\n");
			return -1;
		}
	} else {
		radius_config = 0;
		radius_flag = 0;
		radius_missed_flag = 0;
	}
#endif

	/* ------------ DIAMETER INIT SECTION ----------- */

#ifdef DIAM_ACC
	/* fix the flags */
	if (flag_idx2mask(&diameter_flag)<0)
		return -1;
	if (flag_idx2mask(&diameter_missed_flag)<0)
		return -1;

	/* parse the extra string, if any */
	if (dia_extra_str && (dia_extra=parse_acc_extra(dia_extra_str))==0 ) {
		LM_ERR("failed to parse dia_extra param\n");
		return -1;
	}

	if (acc_diam_init()!=0) {
		LM_ERR("failed to init diameter engine\n");
		return -1;
	}

#endif
	return 0;
}


static int child_init(int rank)
{
#ifdef SQL_ACC
	if (db_url && acc_db_init_child(db_url)<0)
		return -1;
#endif

	/* DIAMETER */
#ifdef DIAM_ACC
	/* open TCP connection */
	LM_DBG("initializing TCP connection\n");

	sockfd = init_mytcp(diameter_client_host, diameter_client_port);
	if(sockfd==-1) 
	{
		LM_ERR("TCP connection not established\n");
		return -1;
	}

	LM_DBG("a TCP connection was established on sockfd=%d\n", sockfd);

	/* every child with its buffer */
	rb = (rd_buf_t*)pkg_malloc(sizeof(rd_buf_t));
	if(!rb)
	{
		LM_DBG("no more pkg memory\n");
		return -1;
	}
	rb->buf = 0;
#endif

	return 0;
}


static void destroy(void)
{
	if (log_extra)
		destroy_extras( log_extra);
#ifdef SQL_ACC
	acc_db_close();
	if (db_extra)
		destroy_extras( db_extra);
#endif
#ifdef RAD_ACC
	if (rad_extra)
		destroy_extras( rad_extra);
#endif
#ifdef DIAM_ACC
	close_tcp_connection(sockfd);
	if (dia_extra)
		destroy_extras( dia_extra);
#endif
}

