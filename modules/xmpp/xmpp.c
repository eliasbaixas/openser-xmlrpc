/*
 * $Id: xmpp.c 2943 2007-10-19 15:15:01Z anca_vamanu $
 *
 * XMPP Module
 * This file is part of openser, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Author: Andreea Spirea
 *
 */

/*
 * An inbound SIP message:
 *   from sip:user1@domain1 to sip:user2*domain2@gateway_domain
 * is translated to an XMPP message:
 *   from user1*domain1@xmpp_domain to user2@domain2
 *
 * An inbound XMPP message:
 *   from user1@domain1 to user2*domain2@xmpp_domain
 * is translated to a SIP message:
 *   from sip:user1*domain1@gateway_domain to sip:user2@domain2
 *
 * Where '*' is the domain_separator, and gateway_domain and
 * xmpp_domain are defined below.
 */

/*
 * 2-way dialback sequence with xmppd2:
 *
 *  Originating server (us)         Receiving server (them)      Authoritative server (us)
 *  -----------------------         -----------------------      -------------------------
 *           |                               |                               |
 *           |    establish connection       |                               |
 *           |------------------------------>|                               |
 *           |    send stream header         |                               |
 *           |------------------------------>|                               |
 *           |    send stream header         |                               |
 *           |<------------------------------|                               |
 *           |    send db:result request     |                               |
 *           |------------------------------>|                               |
 *                                           |    establish connection       |
 *                                           |------------------------------>|
 *                                           |    send stream header         |
 *                                           |------------------------------>|
 *                                           |    send stream header         |
 *                                           |<------------------------------|
 *                                           |    send db:result request     |
 *                                           |------------------------------>|
 *           |    send db:verify request     |
 *           |------------------------------>|
 *           |    send db:verify response    |
 *           |<------------------------------|
 *                                           |    send db:result response    |
 *                                           |------------------------------>|
 *                                           |    send db:verify request     |
 *                                           |<------------------------------|
 *                                           |    send db:verify response    |
 *                                           |------------------------------>|
 *           |    send db:result response    |
 *           |<------------------------------|
 *           :                               :                               :
 *           :                               :                               :
 *           |    outgoing <message/>        |                               :
 *           |------------------------------>|                               :
 *                                           |    incoming <message/>        |
 *                                           |------------------------------>|
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../tm/tm_load.h"

#include "xode.h"
#include "xmpp.h"
#include "network.h"
#include "xmpp_api.h"

#include <arpa/inet.h>

/* XXX hack */
#define DB_KEY	"this-be-a-random-key"

MODULE_VERSION

struct tm_binds tmb;

static int  mod_init(void);
static void xmpp_process(int rank);
static int  cmd_send_message(struct sip_msg* msg, char* _foo, char* _bar);
void destroy(void);

static int pipe_fds[2] = {-1,-1};

/*
 * Configuration
 */
char *backend = "component";
char *domain_sep_str = NULL;
char domain_separator = '*';
char *gateway_domain = "sip2xmpp.example.net";
char *xmpp_domain = "xmpp2sip.example.net";
char *xmpp_host = "xmpp.example.com";
int xmpp_port = 0;
char *xmpp_password = "secret";

#define DEFAULT_COMPONENT_PORT 5347
#define DEFAULT_SERVER_PORT 5269

static proc_export_t procs[] = {
	{"XMPP receiver",  0,  0, xmpp_process, 1 },
	{0,0,0,0,0}
};


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"xmpp_send_message", (cmd_function)cmd_send_message, 0, 0, 0, REQUEST_ROUTE},
	{"bind_xmpp",         (cmd_function)bind_xmpp,        0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{ "backend",		STR_PARAM, &backend },
	{ "domain_separator",	STR_PARAM, &domain_sep_str },
	{ "gateway_domain",	STR_PARAM, &gateway_domain },
	{ "xmpp_domain",	STR_PARAM, &xmpp_domain },
	{ "xmpp_host",		STR_PARAM, &xmpp_host },
	{ "xmpp_port",		INT_PARAM, &xmpp_port },
	{ "xmpp_password",	STR_PARAM, &xmpp_password },
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"xmpp",          /* Module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Exported parameters */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	procs,           /* extra processes */
	mod_init,        /* Initialization function */
	0,               /* Response function */
	destroy,         /* Destroy function */
	0,               /* Child init function */
};

/*
 * initialize module
 */
static int mod_init(void) {

	LM_DBG("initializing\n");

	if (load_tm_api(&tmb)) {
		LM_ERR("failed to load tm API\n");
		return -1;
	}
	
	if (strcmp(backend, "component") && strcmp(backend, "server")) {
		LM_ERR("invalid backend '%s'\n", backend);
		return -1;
	}

	if (!xmpp_port) {
		if (!strcmp(backend, "component"))
			xmpp_port = DEFAULT_COMPONENT_PORT;
		else if (!strcmp(backend, "server"))
			xmpp_port = DEFAULT_SERVER_PORT;
	}

	/* fix up the domain separator -- we only need 1 char */
	if (domain_sep_str && *domain_sep_str)
		domain_separator = *domain_sep_str;

	if(init_xmpp_cb_list()<0){
		LM_ERR("failed to init callback list\n");
		return -1;
	}

	if (pipe(pipe_fds) < 0) {
		LM_ERR("pipe() failed\n");
		return -1;
	}

	return 0;
}


static void xmpp_process(int rank)
{
	/* if this blasted server had a decent I/O loop, we'd
	 * just add our socket to it and connect().
	 */
	close(pipe_fds[1]);

	LM_DBG("started child connection process\n");
	if (!strcmp(backend, "component"))
		xmpp_component_child_process(pipe_fds[0]);
	else if (!strcmp(backend, "server"))
		xmpp_server_child_process(pipe_fds[0]);
}


/* TODO */
void destroy(void)
{
	LM_DBG("cleaning up...\n");
}

/*********************************************************************************/

/* Relay a MESSAGE to a SIP client */
int xmpp_send_sip_msg(char *from, char *to, char *msg)
{
	str msg_type = { "MESSAGE", 7 };
	str hdr, fromstr, tostr, msgstr;
	char buf[512];
	
	hdr.s = buf;
	hdr.len = snprintf(buf, sizeof(buf),
			"Content-type: text/plain" CRLF "Contact: %s" CRLF, from);

	fromstr.s = from;
	fromstr.len = strlen(from);
	tostr.s = to;
	tostr.len = strlen(to);
	msgstr.s = msg;
	msgstr.len = strlen(msg);

	return tmb.t_request(&msg_type, 0, &tostr, &fromstr, &hdr, &msgstr,
			0, 0, 0);
}


/*********************************************************************************/

static char *shm_strdup(str *src)
{
	char *res;

	if (!src || !src->s)
		return NULL;
	if (!(res = (char *) shm_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;
	return res;
}

void xmpp_free_pipe_cmd(struct xmpp_pipe_cmd *cmd)
{
	if (cmd->from)
		shm_free(cmd->from);
	if (cmd->to)
		shm_free(cmd->to);
	if (cmd->body)
		shm_free(cmd->body);
	if (cmd->id)
		shm_free(cmd->id);
	shm_free(cmd);
}

static int xmpp_send_pipe_cmd(enum xmpp_pipe_cmd_type type, str *from, str *to, 
		str *body, str *id)
{
	struct xmpp_pipe_cmd *cmd;
	
	/* todo: make shm allocation for one big chunk to include all fields */
	cmd = (struct xmpp_pipe_cmd *) shm_malloc(sizeof(struct xmpp_pipe_cmd));
	memset(cmd, 0, sizeof(struct xmpp_pipe_cmd));

	cmd->type = type;
	cmd->from = shm_strdup(from);
	cmd->to = shm_strdup(to);
	cmd->body = shm_strdup(body);
	cmd->id = shm_strdup(id);

	if (write(pipe_fds[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to write to command pipe: %s\n", strerror(errno));
		xmpp_free_pipe_cmd(cmd);
		return -1;
	}
	return 0;
}

static int cmd_send_message(struct sip_msg* msg, char* _foo, char* _bar)
{
	str body, from_uri, dst, tagid;
	int mime;

	LM_DBG("cmd_send_message\n");
	
	/* extract body */
	if (!(body.s = get_body(msg))) {
		LM_ERR("failed to extract body\n");
		return -1;
	}
	if (!msg->content_length) {
		LM_ERR("no content-length found\n");
		return -1;
	}
	body.len = get_content_length(msg);
	if ((mime = parse_content_type_hdr(msg)) < 1) {
		LM_ERR("failed parse content-type\n");
		return -1;
	}
	if (mime != (TYPE_TEXT << 16) + SUBTYPE_PLAIN 
			&& mime != (TYPE_MESSAGE << 16) + SUBTYPE_CPIM) {
		LM_ERR("invalid content-type 0x%x\n", mime);
		return -1;
	}

	/* extract sender */
	if (parse_headers(msg, HDR_TO_F | HDR_FROM_F, 0) == -1 || !msg->to
			|| !msg->from) {
		LM_ERR("no To/From headers\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}
	from_uri = ((struct to_body *) msg->from->parsed)->uri;
	tagid = ((struct to_body *) msg->from->parsed)->tag_value;
	LM_DBG("message from <%.*s>\n", from_uri.len, from_uri.s);

	/* extract recipient */
	dst.len = 0;
	if (msg->new_uri.len > 0) {
		LM_DBG("using new URI as destination\n");
		dst = msg->new_uri;
	} else if (msg->first_line.u.request.uri.s 
			&& msg->first_line.u.request.uri.len > 0) {
		LM_DBG("using R-URI as destination\n");
		dst = msg->first_line.u.request.uri;
	} else if (msg->to->parsed) {
		LM_DBG("using TO-URI as destination\n");
		dst = ((struct to_body *) msg->to->parsed)->uri;
	} else {
		LM_ERR("failed to find a valid destination\n");
		return -1;
	}
	
	if (!xmpp_send_pipe_cmd(XMPP_PIPE_SEND_MESSAGE, &from_uri, &dst, &body,
				&tagid))
		return 1;
	return -1;
}


/**
 *
 */
int xmpp_send_xpacket(str *from, str *to, str *msg, str *id)
{
	if(from==NULL || to==NULL || msg==NULL || id==NULL)
		return -1;
	return xmpp_send_pipe_cmd(XMPP_PIPE_SEND_PACKET, from, to, msg, id);
}

/**
 *
 */
int xmpp_send_xmessage(str *from, str *to, str *msg, str *id)
{
	if(from==NULL || to==NULL || msg==NULL || id==NULL)
		return -1;
	return xmpp_send_pipe_cmd(XMPP_PIPE_SEND_MESSAGE, from, to, msg, id);
}

/**
 *
 */
int xmpp_send_xsubscribe(str *from, str *to, str *msg, str *id)
{
	if(from==NULL || to==NULL || msg==NULL || id==NULL)
		return -1;
	return xmpp_send_pipe_cmd(XMPP_PIPE_SEND_PSUBSCRIBE, from, to, msg, id);
}

/**
 *
 */
int xmpp_send_xnotify(str *from, str *to, str *msg, str *id)
{
	if(from==NULL || to==NULL || msg==NULL || id==NULL)
		return -1;
	return xmpp_send_pipe_cmd(XMPP_PIPE_SEND_PNOTIFY, from, to, msg, id);
}

