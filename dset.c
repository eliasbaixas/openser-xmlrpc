/*
 * $Id: dset.c 2769 2007-09-14 15:13:29Z bogdan_iancu $
 *
 * destination set
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 * ========
 *
 *  2006-12-22  functions branch flags added (bogdan)
 *
 */

#include <string.h>
#include "dprint.h"
#include "config.h"
#include "parser/parser_f.h"
#include "parser/msg_parser.h"
#include "ut.h"
#include "hash_func.h"
#include "error.h"
#include "dset.h"
#include "mem/mem.h"
#include "ip_addr.h"

#define CONTACT "Contact: "
#define CONTACT_LEN (sizeof(CONTACT) - 1)

#define CONTACT_DELIM ", "
#define CONTACT_DELIM_LEN (sizeof(CONTACT_DELIM) - 1)

#define Q_PARAM ">;q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)

struct branch
{
	char uri[MAX_URI_SIZE];
	unsigned int len;

	/* Real destination of the request */
	char dst_uri[MAX_URI_SIZE];
	unsigned int dst_uri_len;

	/* Path vector of the request */
	char path[MAX_PATH_SIZE];
	unsigned int path_len;

	int q; /* Preference of the contact among contact within the array */
	struct socket_info* force_send_socket;
	unsigned int flags;
};


/* 
 * Where we store URIs of additional transaction branches
 * (-1 because of the default branch, #0)
 */
static struct branch branches[MAX_BRANCHES - 1];

/* how many of them we have */
unsigned int nr_branches = 0;

/* The q parameter of the Request-URI */
static qvalue_t ruri_q = Q_UNSPECIFIED; 


/* Branch flags of the Request-URI */
static unsigned int ruri_bflags = 0;



static inline unsigned int* get_ptr_bflags(unsigned int b_idx)
{
	if (b_idx==0) {
		return &ruri_bflags;
	} else {
		if (b_idx-1<nr_branches) {
			return &branches[b_idx-1].flags;
		} else {
			return 0;
		}
	}
}


/*
 * Get the per branch flags for RURI
 */
unsigned int getb0flags(void)
{
	return ruri_bflags;
}


unsigned int setb0flags( unsigned int flags)
{
	ruri_bflags = flags;
	return 0;
}


int setbflag(unsigned int b_idx, unsigned int mask)
{
	unsigned int *flags;

	flags = get_ptr_bflags( b_idx );
	if (flags==0)
		return -1;

	(*flags) |= mask;
	return 1;
}


/*
 * Tests the per branch flags
 */
int isbflagset(unsigned int b_idx, unsigned int mask)
{
	unsigned int *flags;

	flags = get_ptr_bflags( b_idx );
	if (flags==0)
		return -1;

	return ( (*flags) & mask) ? 1 : -1;
}


/*
 * Resets the per branch flags
 */
int resetbflag(unsigned int b_idx, unsigned int mask)
{
	unsigned int *flags;

	flags = get_ptr_bflags( b_idx );
	if (flags==0)
		return -1;

	(*flags) &= ~mask;
	return 1;
}


/*
 * Return the next branch from the dset
 * array, 0 is returned if there are no
 * more branches
 */
char* get_branch(unsigned int idx, int* len, qvalue_t* q, str* dst_uri,
		str* path, unsigned int *flags, struct socket_info** force_socket)
{
	if (idx < nr_branches) {
		*len = branches[idx].len;
		*q = branches[idx].q;
		if (dst_uri) {
			dst_uri->len = branches[idx].dst_uri_len;
			dst_uri->s = (dst_uri->len)?branches[idx].dst_uri:0;
		}
		if (path) {
			path->len = branches[idx].path_len;
			path->s = (path->len)?branches[idx].path:0;
		}
		if (force_socket)
			*force_socket = branches[idx].force_send_socket;
		if (flags)
			*flags = branches[idx].flags;
		return branches[idx].uri;
	} else {
		*len = 0;
		*q = Q_UNSPECIFIED;
		if (dst_uri) {
			dst_uri->s = 0;
			dst_uri->len = 0;
		}
		if (force_socket)
			*force_socket = 0;
		if (flags)
			*flags = 0;
		return 0;
	}
}


/*
 * Empty the dset array
 */
void clear_branches(void)
{
	nr_branches = 0;
	ruri_q = Q_UNSPECIFIED;
	ruri_bflags = 0;
}


/* 
 * Add a new branch to current transaction 
 */
int append_branch(struct sip_msg* msg, str* uri, str* dst_uri, str* path,
		qvalue_t q, unsigned int flags, struct socket_info* force_socket)
{
	str luri;

	/* if we have already set up the maximum number
	 * of branches, don't try new ones 
	 */
	if (nr_branches == MAX_BRANCHES - 1) {
		LM_ERR("max nr of branches exceeded\n");
		ser_error = E_TOO_MANY_BRANCHES;
		return -1;
	}

	/* if not parameterized, take current uri */
	if (uri==0 || uri->len==0 || uri->s==0) {
		if (msg->new_uri.s)
			luri = msg->new_uri;
		else
			luri = msg->first_line.u.request.uri;
	} else {
		luri = *uri;
	}

	if (luri.len > MAX_URI_SIZE - 1) {
		LM_ERR("too long uri: %.*s\n", luri.len, luri.s);
		return -1;
	}

	/* copy the dst_uri */
	if (dst_uri && dst_uri->len && dst_uri->s) {
		if (dst_uri->len > MAX_URI_SIZE - 1) {
			LM_ERR("too long dst_uri: %.*s\n",
				dst_uri->len, dst_uri->s);
			return -1;
		}
		memcpy(branches[nr_branches].dst_uri, dst_uri->s, dst_uri->len);
		branches[nr_branches].dst_uri[dst_uri->len] = 0;
		branches[nr_branches].dst_uri_len = dst_uri->len;
	} else {
		branches[nr_branches].dst_uri[0] = '\0';
		branches[nr_branches].dst_uri_len = 0;
	}

	/* copy the path string */
	if (path && path->len && path->s) {
		if (path->len > MAX_PATH_SIZE - 1) {
			LM_ERR("too long path: %.*s\n", path->len, path->s);
			return -1;
		}
		memcpy(branches[nr_branches].path, path->s, path->len);
		branches[nr_branches].path[path->len] = 0;
		branches[nr_branches].path_len = path->len;
	} else {
		branches[nr_branches].path[0] = '\0';
		branches[nr_branches].path_len = 0;
	}

	/* copy the ruri */
	memcpy(branches[nr_branches].uri, luri.s, luri.len);
	branches[nr_branches].uri[luri.len] = 0;
	branches[nr_branches].len = luri.len;
	branches[nr_branches].q = q;

	branches[nr_branches].force_send_socket = force_socket;
	branches[nr_branches].flags = flags;

	nr_branches++;
	return 1;
}


/*
 * Create a Contact header field from the dset
 * array
 */
char* print_dset(struct sip_msg* msg, int* len) 
{
	int cnt, i, idx;
	unsigned int qlen;
	qvalue_t q;
	str uri;
	char* p, *qbuf;
	static char dset[MAX_REDIRECTION_LEN];

	if (msg->new_uri.s) {
		cnt = 1;
		*len = msg->new_uri.len;
		if (ruri_q != Q_UNSPECIFIED) {
			*len += 1 + Q_PARAM_LEN + len_q(ruri_q);
		}
	} else {
		cnt = 0;
		*len = 0;
	}

	for( idx=0 ; (uri.s=get_branch(idx,&uri.len,&q,0,0,0,0))!=0 ; idx++ ) {
		cnt++;
		*len += uri.len;
		if (q != Q_UNSPECIFIED) {
			*len += 1 + Q_PARAM_LEN + len_q(q);
		}
	}

	if (cnt == 0) return 0;	

	*len += CONTACT_LEN + CRLF_LEN + (cnt - 1) * CONTACT_DELIM_LEN;

	if (*len + 1 > MAX_REDIRECTION_LEN) {
		LM_ERR("redirection buffer length exceed\n");
		return 0;
	}

	memcpy(dset, CONTACT, CONTACT_LEN);
	p = dset + CONTACT_LEN;
	if (msg->new_uri.s) {
		if (ruri_q != Q_UNSPECIFIED) {
			*p++ = '<';
		}

		memcpy(p, msg->new_uri.s, msg->new_uri.len);
		p += msg->new_uri.len;

		if (ruri_q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(ruri_q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		}
		i = 1;
	} else {
		i = 0;
	}

	for( idx=0 ; (uri.s=get_branch(idx,&uri.len,&q,0,0,0,0))!=0 ; idx++ ) {
		if (i) {
			memcpy(p, CONTACT_DELIM, CONTACT_DELIM_LEN);
			p += CONTACT_DELIM_LEN;
		}

		if (q != Q_UNSPECIFIED) {
			*p++ = '<';
		}

		memcpy(p, uri.s, uri.len);
		p += uri.len;
		if (q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		}
		i++;
	}

	memcpy(p, CRLF " ", CRLF_LEN + 1);
	return dset;
}


/*
 * Sets the q parameter of the Request-URI
 */
void set_ruri_q(qvalue_t q)
{
	ruri_q = q;
}


/*
 * Return the q value of the Request-URI
 */
qvalue_t get_ruri_q(void)
{
	return ruri_q;
}



/*
 * Get actual Request-URI
 */
int get_request_uri(struct sip_msg* _m, str* _u)
{
	/* Use new_uri if present */
	if (_m->new_uri.s) {
		_u->s = _m->new_uri.s;
		_u->len = _m->new_uri.len;
	} else {
		_u->s = _m->first_line.u.request.uri.s;
		_u->len = _m->first_line.u.request.uri.len;
	}

	return 0;
}


/*
 * Rewrite Request-URI
 */
int rewrite_uri(struct sip_msg* _m, str* _s)
{
	char* buf;

	buf = (char*)pkg_malloc(_s->len + 1);
	if (!buf) {
		LM_ERR("No pkg memory left\n");
		return -1;
	}

	memcpy(buf, _s->s, _s->len);
	buf[_s->len] = '\0';

	_m->parsed_uri_ok = 0;
	if (_m->new_uri.s) {
		pkg_free(_m->new_uri.s);
	}

	_m->new_uri.s = buf;
	_m->new_uri.len = _s->len;

	LM_DBG("rewriting Request-URI with '%.*s'\n", _s->len, buf);
	return 1;
}


/* moves the uri to destination for all branches and
 * all uris are set to given uri */
int branch_uri2dset( str *new_uri )
{
	unsigned int b;

	if (new_uri->len+1 > MAX_URI_SIZE) {
		LM_ERR("new uri too long (%d)\n",new_uri->len);
		return -1;
	}

	for( b=0 ; b<nr_branches ; b++ ) {
		/* move uri to dst */
		memcpy( branches[b].dst_uri,  branches[b].uri,  branches[b].len+1);
		branches[b].dst_uri_len =  branches[b].len;
		/* set new uri */
		memcpy( branches[b].uri,  new_uri->s,  new_uri->len);
		branches[b].len =  new_uri->len;
		branches[b].uri[new_uri->len] = 0;
	}

	return 0;
}
