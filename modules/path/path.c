/*
 * $Id: path.c 2802 2007-09-21 14:11:40Z bogdan_iancu $
 *
 * Path handling for intermediate proxies.
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
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
 */


#include <string.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/parse_param.h"

#include "path.h"
#include "path_mod.h"

#define PATH_PREFIX		"Path: <sip:"
#define PATH_PREFIX_LEN		(sizeof(PATH_PREFIX)-1)

#define PATH_LR_PARAM		";lr"
#define PATH_LR_PARAM_LEN	(sizeof(PATH_LR_PARAM)-1)

#define PATH_RC_PARAM		";received="
#define PATH_RC_PARAM_LEN	(sizeof(PATH_RC_PARAM)-1)

#define	PATH_CRLF		">\r\n"
#define PATH_CRLF_LEN		(sizeof(PATH_CRLF)-1)

static int prepend_path(struct sip_msg* _m, str *user, int recv)
{
	struct lump *l;
	char *prefix, *suffix, *crlf;
	int prefix_len, suffix_len;
	struct hdr_field *hf;
	str rcv_addr = {0, 0};
	char *src_ip;
		
	prefix = suffix = crlf = 0;

	prefix_len = PATH_PREFIX_LEN + (user->len ? (user->len+1) : 0);
	prefix = pkg_malloc(prefix_len);
	if (!prefix) {
		LM_ERR("no pkg memory left for prefix\n");
		goto out1;
	}
	memcpy(prefix, PATH_PREFIX, PATH_PREFIX_LEN);
	if (user->len) {
		memcpy(prefix + PATH_PREFIX_LEN, user->s, user->len);
		memcpy(prefix + prefix_len - 1, "@", 1);
	}

	suffix_len = PATH_LR_PARAM_LEN + (recv ? PATH_RC_PARAM_LEN : 0);
	suffix = pkg_malloc(suffix_len);
	if (!suffix) {
		LM_ERR("no pkg memory left for suffix\n");
		goto out1;
	}
	memcpy(suffix, PATH_LR_PARAM, PATH_LR_PARAM_LEN);
	if(recv)
		memcpy(suffix+PATH_LR_PARAM_LEN, PATH_RC_PARAM, PATH_RC_PARAM_LEN);

	crlf = pkg_malloc(PATH_CRLF_LEN);
	if (!crlf) {
		LM_ERR("no pkg memory left for crlf\n");
		goto out1;
	}
	memcpy(crlf, PATH_CRLF, PATH_CRLF_LEN);

	if (parse_headers(_m, HDR_PATH_F, 0) < 0) {
		LM_ERR("failed to parse message for Path header\n");
		goto out1;
	}
	for (hf = _m->headers; hf; hf = hf->next) {
		if (hf->type == HDR_PATH_T) {
			break;
		} 
	}
	if (hf)
		/* path found, add ours in front of that */
		l = anchor_lump(_m, hf->name.s - _m->buf, 0, 0);
	else
		/* no path, append to message */
		l = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (!l) {
		LM_ERR("failed to get anchor\n");
		goto out1;
	}

	l = insert_new_lump_before(l, prefix, prefix_len, 0);
	if (!l) goto out1;
	l = insert_subst_lump_before(l, SUBST_SND_ALL, 0);
	if (!l) goto out2;
	l = insert_new_lump_before(l, suffix, suffix_len, 0);
	if (!l) goto out2;
	if (recv) {
		/* TODO: agranig: optimize this one! */
		src_ip = ip_addr2a(&_m->rcv.src_ip);
		rcv_addr.s = pkg_malloc(4 + IP_ADDR_MAX_STR_SIZE + 7); /* sip:<ip>:<port>\0 */
		if(!rcv_addr.s) {
			LM_ERR("no pkg memory left for receive-address\n");
			goto out3;
		}
		rcv_addr.len = snprintf(rcv_addr.s, 4 + IP_ADDR_MAX_STR_SIZE + 6, "sip:%s:%u", src_ip, _m->rcv.src_port);
		l = insert_new_lump_before(l, rcv_addr.s, rcv_addr.len, 0);
		if (!l) goto out3;
	}
	l = insert_new_lump_before(l, crlf, CRLF_LEN+1, 0);
	if (!l) goto out4;
	
	return 1;
	
out1:
	if (prefix) pkg_free(prefix);
out2:
	if (suffix) pkg_free(suffix);
out3:
	if (rcv_addr.s) pkg_free(rcv_addr.s);
out4:
	if (crlf) pkg_free(crlf);

	LM_ERR("failed to insert prefix lump\n");

	return -1;
}

/*
 * Prepend own uri to Path header
 */
int add_path(struct sip_msg* _msg, char* _a, char* _b)
{
	str user = {0,0};
	return prepend_path(_msg, &user, 0);
}

/*
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg* _msg, char* _usr, char* _b)
{
	return prepend_path(_msg, (str*)_usr, 0);
}

/*
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg* _msg, char* _a, char* _b)
{
	str user = {0,0};
	return prepend_path(_msg, &user, 1);
}

/*
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg* _msg, char* _usr, char* _b)
{
	return prepend_path(_msg, (str*)_usr, 1);
}

/*
 * rr callback
 */
void path_rr_callback(struct sip_msg *_m, str *r_param, void *cb_param)
{
	param_hooks_t hooks;
	param_t *params;
			
	if (parse_params(r_param, CLASS_CONTACT, &hooks, &params) != 0) {
		LM_ERR("failed to parse route parametes\n");
		return;
	}

	if (hooks.contact.received) {
		if (set_dst_uri(_m, &hooks.contact.received->body) != 0) {
			LM_ERR("failed to set dst-uri\n");
			free_params(params);
			return;
		}
	}
	free_params(params);
}
