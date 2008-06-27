/*
 * $Id: save.c 2802 2007-09-21 14:11:40Z bogdan_iancu $
 *
 * Process REGISTER request and send reply
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
 * ----------
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-02-28 scrathcpad compatibility abandoned (jiri)
 * 2003-03-21 save_noreply added, patch provided by Maxim Sobolev 
 *            <sobomax@portaone.com> (janakj)
 * 2005-07-11 added sip_natping_flag for nat pinging with SIP method
 *            instead of UDP package (bogdan)
 * 2006-04-13 added tcp_persistent_flag for keeping the TCP connection as long
 *            as a TCP contact is registered (bogdan)
 * 2006-11-22 save_noreply and save_memory merged into save() (bogdan)
 * 2006-11-28 Added statistic support for the number of accepted/rejected 
 *            registrations. (Jeffrey Magder - SOMA Networks) 
 * 2007-02-24  sip_natping_flag moved into branch flags, so migrated to 
 *             nathelper module (bogdan)
 */


#include "../../str.h"
#include "../../socket_info.h"
#include "../../parser/parse_allow.h"
#include "../../parser/parse_methods.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "../../dset.h"
#ifdef USE_TCP
#include "../../tcp_server.h"
#endif
#include "../usrloc/usrloc.h"
#include "common.h"
#include "sip_msg.h"
#include "rerrno.h"
#include "reply.h"
#include "reg_mod.h"
#include "regtime.h"
#include "path.h"
#include "save.h"

static int mem_only = 0;

/*
 * Process request that contained a star, in that case, 
 * we will remove all bindings with the given username 
 * from the usrloc and return 200 OK response
 */
static inline int star(udomain_t* _d, str* _a)
{
	urecord_t* r;
	ucontact_t* c;
	
	ul.lock_udomain(_d, _a);

	if (!ul.get_urecord(_d, _a, &r)) {
		c = r->contacts;
		while(c) {
			if (mem_only) {
				c->flags |= FL_MEM;
			} else {
				c->flags &= ~FL_MEM;
			}
			c = c->next;
		}
	}

	if (ul.delete_urecord(_d, _a, 0) < 0) {
		LM_ERR("failed to remove record from usrloc\n");
		
		     /* Delete failed, try to get corresponding
		      * record structure and send back all existing
		      * contacts
		      */
		rerrno = R_UL_DEL_R;
		if (!ul.get_urecord(_d, _a, &r)) {
			build_contact(r->contacts);
		}
		ul.unlock_udomain(_d, _a);
		return -1;
	}
	ul.unlock_udomain(_d, _a);
	return 0;
}


/*
 */
static struct socket_info *get_sock_hdr(struct sip_msg *msg)
{
	struct socket_info *sock;
	struct hdr_field *hf;
	str socks;
	str hosts;
	int port;
	int proto;

	if (parse_headers( msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse message\n");
		return 0;
	}

	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len==sock_hdr_name.len &&
		strncasecmp(hf->name.s, sock_hdr_name.s, sock_hdr_name.len)==0 )
			break;
	}

	/* hdr found? */
	if (hf==0)
		return 0;

	trim_len( socks.len, socks.s, hf->body );
	if (socks.len==0)
		return 0;

	if (parse_phostport( socks.s, socks.len, &hosts.s, &hosts.len, 
	&port, &proto)!=0) {
		LM_ERR("bad socket <%.*s> in \n",
			socks.len, socks.s);
		return 0;
	}
	sock = grep_sock_info(&hosts,(unsigned short)port,(unsigned short)proto);
	if (sock==0) {
		LM_ERR("non-local socket <%.*s>\n",	socks.len, socks.s);
		return 0;
	}

	LM_DBG("%d:<%.*s>:%d -> p=%p\n", proto,socks.len,socks.s,port_no,sock );

	return sock;
}



/*
 * Process request that contained no contact header
 * field, it means that we have to send back a response
 * containing a list of all existing bindings for the
 * given username (in To HF)
 */
static inline int no_contacts(udomain_t* _d, str* _a)
{
	urecord_t* r;
	int res;
	
	ul.lock_udomain(_d, _a);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LM_ERR("failed to retrieve record from usrloc\n");
		ul.unlock_udomain(_d, _a);
		return -1;
	}
	
	if (res == 0) {  /* Contacts found */
		build_contact(r->contacts);
	}
	ul.unlock_udomain(_d, _a);
	return 0;
}



/*
 * Fills the common part (for all contacts) of the info structure
 */
static inline ucontact_info_t* pack_ci( struct sip_msg* _m, contact_t* _c,
											unsigned int _e, unsigned int _f)
{
	static ucontact_info_t ci;
	static str no_ua = str_init("n/a");
	static str callid;
	static str path_received = {0,0};
	static str path;
	static str received = {0,0};
	static int received_found;
	static unsigned int allowed, allow_parsed;
	static struct sip_msg *m = 0;
	int_str val;

	if (_m!=0) {
		memset( &ci, 0, sizeof(ucontact_info_t));

		/* Get callid of the message */
		callid = _m->callid->body;
		trim_trailing(&callid);
		if (callid.len > CALLID_MAX_SIZE) {
			rerrno = R_CALLID_LEN;
			LM_ERR("callid too long\n");
			goto error;
		}
		ci.callid = &callid;

		/* Get CSeq number of the message */
		if (str2int(&get_cseq(_m)->number, (unsigned int*)&ci.cseq) < 0) {
			rerrno = R_INV_CSEQ;
			LM_ERR("failed to convert cseq number\n");
			goto error;
		}

		/* set received socket */
		if (_m->flags&sock_flag) {
			ci.sock = get_sock_hdr(_m);
			if (ci.sock==0)
				ci.sock = _m->rcv.bind_address;
		} else {
			ci.sock = _m->rcv.bind_address;
		}

		/* additional info from message */
		if (parse_headers(_m, HDR_USERAGENT_F, 0) != -1 && _m->user_agent &&
		_m->user_agent->body.len>0 && _m->user_agent->body.len<UA_MAX_SIZE) {
			ci.user_agent = &_m->user_agent->body;
		} else {
			ci.user_agent = &no_ua;
		}

		/* extract Path headers */
		if (path_enabled) {
			if (build_path_vector(_m, &path, &path_received) < 0) {
				rerrno = R_PARSE_PATH;
				goto error;
			}
			if (path.len && path.s) {
				ci.path = &path;
				/* save in msg too for reply */
				if (set_path_vector(_m, &path) < 0) {
					rerrno = R_PARSE_PATH;
					goto error;
				}
			}
		}

		ci.last_modified = act_time;

		/* set flags */
		ci.flags  = _f;
		ci.cflags =  getb0flags();

		/* get received */
		if (path_received.len && path_received.s) {
			ci.cflags |= ul.nat_flag;
			ci.received = path_received;
		}

		allow_parsed = 0; /* not parsed yet */
		received_found = 0; /* not found yet */
		m = _m; /* remember the message */
	}

	if(_c!=0) {
		/* Calculate q value of the contact */
		if (calc_contact_q(_c->q, &ci.q) < 0) {
			rerrno = R_INV_Q;
			LM_ERR("failed to calculate q\n");
			goto error;
		}

		/* set expire time */
		ci.expires = _e;

		/* Get methods of contact */
		if (_c->methods) {
			if (parse_methods(&(_c->methods->body), &ci.methods) < 0) {
				rerrno = R_PARSE;
				LM_ERR("failed to parse contact methods\n");
				goto error;
			}
		} else {
			/* check on Allow hdr */
			if (allow_parsed == 0) {
				if (m && parse_allow( m ) != -1) {
					allowed = get_allow_methods(m);
				} else {
					allowed = ALL_METHODS;
				}
				allow_parsed = 1;
			}
			ci.methods = allowed;
		}

		/* get received */
		if (ci.received.len==0) {
			if (_c->received) {
				ci.received = _c->received->body;
			} else {
				if (received_found==0) {
					memset(&val, 0, sizeof(int_str));
					if (rcv_avp_name.n!=0
								&& search_first_avp(rcv_avp_type, rcv_avp_name, &val, 0)
								&& val.s.len > 0) {
						if (val.s.len>RECEIVED_MAX_SIZE) {
							rerrno = R_CONTACT_LEN;
							LM_ERR("received too long\n");
							goto error;
						}
						received = val.s;
					} else {
						received.s = 0;
						received.len = 0;
					}
					received_found = 1;
				}
				ci.received = received;
			}
		}

	}

	return &ci;
error:
	return 0;
}



/*
 * Message contained some contacts, but record with same address
 * of record was not found so we have to create a new record
 * and insert all contacts from the message that have expires
 * > 0
 */
static inline int insert_contacts(struct sip_msg* _m, contact_t* _c,
													udomain_t* _d, str* _a)
{
	ucontact_info_t* ci;
	urecord_t* r;
	ucontact_t* c;
	unsigned int flags;
	int num;
	int e;
#ifdef USE_TCP
	int e_max;
	int tcp_check;
	struct sip_uri uri;
#endif

	flags = mem_only;
#ifdef USE_TCP
	if ( (_m->flags&tcp_persistent_flag) &&
	(_m->rcv.proto==PROTO_TCP||_m->rcv.proto==PROTO_TLS)) {
		e_max = 0;
		tcp_check = 1;
	} else {
		e_max = tcp_check = 0;
	}
#endif

	for( num=0,r=0,ci=0 ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &e);
		/* Skip contacts with zero expires */
		if (e == 0)
			continue;

		if (max_contacts && (num >= max_contacts)) {
			LM_INFO("too many contacts (%d) for AOR <%.*s>\n", 
					num, _a->len, _a->s);
			rerrno = R_TOO_MANY;
			goto error;
		}
		num++;

		if (r==0) {
			if (ul.insert_urecord(_d, _a, &r) < 0) {
				rerrno = R_UL_NEW_R;
				LM_ERR("failed to insert new record structure\n");
				goto error;
			}
		}

		/* pack the contact_info */
		if ( (ci=pack_ci( (ci==0)?_m:0, _c, e, flags))==0 ) {
			LM_ERR("failed to extract contact info\n");
			goto error;
		}

		if ( r->contacts==0 ||
		ul.get_ucontact(r, &_c->uri, ci->callid, ci->cseq+1, &c)!=0 ) {
			if (ul.insert_ucontact( r, &_c->uri, ci, &c) < 0) {
				rerrno = R_UL_INS_C;
				LM_ERR("failed to insert contact\n");
				goto error;
			}
		} else {
			if (ul.update_ucontact( r, c, ci) < 0) {
				rerrno = R_UL_UPD_C;
				LM_ERR("failed to update contact\n");
				goto error;
			}
		}
#ifdef USE_TCP
		if (tcp_check) {
			/* parse contact uri to see if transport is TCP */
			if (parse_uri( _c->uri.s, _c->uri.len, &uri)<0) {
				LM_ERR("failed to parse contact <%.*s>\n", 
						_c->uri.len, _c->uri.s);
			} else if (uri.proto==PROTO_TCP || uri.proto==PROTO_TLS) {
				if (e_max) {
					LM_WARN("multiple TCP contacts on single REGISTER\n");
					if (e>e_max) e_max = e;
				} else {
					e_max = e;
				}
			}
		}
#endif
	}

	if (r) {
		if (r->contacts)
			build_contact(r->contacts);
		ul.release_urecord(r);
	}

#ifdef USE_TCP
	if ( tcp_check && e_max>0 ) {
		e_max -= act_time;
		force_tcp_conn_lifetime( &_m->rcv , e_max + 10 );
	}
#endif

	return 0;
error:
	if (r)
		ul.delete_urecord(_d, _a, r);
	return -1;
}


static int test_max_contacts(struct sip_msg* _m, urecord_t* _r, contact_t* _c,
														ucontact_info_t *ci)
{
	int num;
	int e;
	ucontact_t* ptr, *cont;
	int ret;
	
	num = 0;
	ptr = _r->contacts;
	while(ptr) {
		if (VALID_CONTACT(ptr, act_time)) {
			num++;
		}
		ptr = ptr->next;
	}
	LM_DBG("%d valid contacts\n", num);
	
	for( ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &e);
		
		ret = ul.get_ucontact( _r, &_c->uri, ci->callid, ci->cseq, &cont);
		if (ret==-1) {
			LM_ERR("invalid cseq for aor <%.*s>\n",_r->aor.len,_r->aor.s);
			rerrno = R_INV_CSEQ;
			return -1;
		} else if (ret==-2) {
			continue;
		}
		if (ret > 0) {
			/* Contact not found */
			if (e != 0) num++;
		} else {
			if (e == 0) num--;
		}
	}
	
	LM_DBG("%d contacts after commit\n", num);
	if (num > max_contacts) {
		LM_INFO("too many contacts for AOR <%.*s>\n", _r->aor.len, _r->aor.s);
		rerrno = R_TOO_MANY;
		return -1;
	}

	return 0;
}


/*
 * Message contained some contacts and appropriate
 * record was found, so we have to walk through
 * all contacts and do the following:
 * 1) If contact in usrloc doesn't exists and
 *    expires > 0, insert new contact
 * 2) If contact in usrloc exists and expires
 *    > 0, update the contact
 * 3) If contact in usrloc exists and expires
 *    == 0, delete contact
 */
static inline int update_contacts(struct sip_msg* _m, urecord_t* _r,
																contact_t* _c)
{
	ucontact_info_t *ci;
	ucontact_t* c;
	int e;
	unsigned int flags;
	int ret;
#ifdef USE_TCP
	int e_max;
	int tcp_check;
	struct sip_uri uri;
#endif

	/* mem flag */
	flags = mem_only;

	/* pack the contact_info */
	if ( (ci=pack_ci( _m, 0, 0, flags))==0 ) {
		LM_ERR("failed to initial pack contact info\n");
		goto error;
	}

	if (max_contacts && test_max_contacts(_m, _r, _c, ci) != 0 )
		goto error;

#ifdef USE_TCP
	if ( (_m->flags&tcp_persistent_flag) &&
	(_m->rcv.proto==PROTO_TCP||_m->rcv.proto==PROTO_TLS)) {
		e_max = -1;
		tcp_check = 1;
	} else {
		e_max = tcp_check = 0;
	}
#endif

	for( ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &e);

		/* search for the contact*/
		ret = ul.get_ucontact( _r, &_c->uri, ci->callid, ci->cseq, &c);
		if (ret==-1) {
			LM_ERR("invalid cseq for aor <%.*s>\n",_r->aor.len,_r->aor.s);
			rerrno = R_INV_CSEQ;
			goto error;
		} else if (ret==-2) {
			continue;
		}

		if ( ret > 0 ) {
			/* Contact not found -> expired? */
			if (e==0)
				continue;

			/* pack the contact_info */
			if ( (ci=pack_ci( 0, _c, e, 0))==0 ) {
				LM_ERR("failed to extract contact info\n");
				goto error;
			}

			if (ul.insert_ucontact( _r, &_c->uri, ci, &c) < 0) {
				rerrno = R_UL_INS_C;
				LM_ERR("failed to insert contact\n");
				goto error;
			}
		} else {
			/* Contact found */
			if (e == 0) {
				/* it's expired */
				if (mem_only) {
					c->flags |= FL_MEM;
				} else {
					c->flags &= ~FL_MEM;
				}

				if (ul.delete_ucontact(_r, c) < 0) {
					rerrno = R_UL_DEL_C;
					LM_ERR("failed to delete contact\n");
					goto error;
				}
			} else {
				/* do update */
				/* pack the contact specific info */
				if ( (ci=pack_ci( 0, _c, e, 0))==0 ) {
					LM_ERR("failed to pack contact specific info\n");
					goto error;
				}

				if (ul.update_ucontact(_r, c, ci) < 0) {
					rerrno = R_UL_UPD_C;
					LM_ERR("failed to update contact\n");
					goto error;
				}
			}
		}
#ifdef USE_TCP
		if (tcp_check) {
			/* parse contact uri to see if transport is TCP */
			if (parse_uri( _c->uri.s, _c->uri.len, &uri)<0) {
				LM_ERR("failed to parse contact <%.*s>\n", 
						_c->uri.len, _c->uri.s);
			} else if (uri.proto==PROTO_TCP || uri.proto==PROTO_TLS) {
				if (e_max>0) {
					LM_WARN("multiple TCP contacts on single REGISTER\n");
				}
				if (e>e_max) e_max = e;
			}
		}
#endif
	}

#ifdef USE_TCP
	if ( tcp_check && e_max>-1 ) {
		if (e_max) e_max -= act_time;
		force_tcp_conn_lifetime( &_m->rcv , e_max + 10 );
	}
#endif

	return 0;
error:
	return -1;
}


/* 
 * This function will process request that
 * contained some contact header fields
 */
static inline int add_contacts(struct sip_msg* _m, contact_t* _c,
													udomain_t* _d, str* _a)
{
	int res;
	urecord_t* r;

	ul.lock_udomain(_d, _a);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LM_ERR("failed to retrieve record from usrloc\n");
		ul.unlock_udomain(_d, _a);
		return -2;
	}

	if (res == 0) { /* Contacts found */
		if (update_contacts(_m, r, _c) < 0) {
			build_contact(r->contacts);
			ul.release_urecord(r);
			ul.unlock_udomain(_d, _a);
			return -3;
		}
		build_contact(r->contacts);
		ul.release_urecord(r);
	} else {
		if (insert_contacts(_m, _c, _d, _a) < 0) {
			ul.unlock_udomain(_d, _a);
			return -4;
		}
	}
	ul.unlock_udomain(_d, _a);
	return 0;
}


/*
 * Process REGISTER request and save it's contacts
 */
#define is_cflag_set(_name) (((unsigned int)(unsigned long)_cflags)&(_name))
int save(struct sip_msg* _m, char* _d, char* _cflags)
{
	contact_t* c;
	int st;
	str aor;

	rerrno = R_FINE;

	if (parse_message(_m) < 0) {
		goto error;
	}

	if (check_contacts(_m, &st) > 0) {
		goto error;
	}
	
	get_act_time();
	c = get_first_contact(_m);

	if (extract_aor(&get_to(_m)->uri, &aor) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		goto error;
	}

	mem_only = is_cflag_set(REG_SAVE_MEM_FL)?FL_MEM:FL_NONE;

	if (c == 0) {
		if (st) {
			if (star((udomain_t*)_d, &aor) < 0) goto error;
		} else {
			if (no_contacts((udomain_t*)_d, &aor) < 0) goto error;
		}
	} else {
		if (add_contacts(_m, c, (udomain_t*)_d, &aor) < 0) goto error;
	}

	update_stat(accepted_registrations, 1);

	if ( !is_cflag_set(REG_SAVE_NORPL_FL) && (send_reply(_m) < 0))
		return -1;

	return 1;
error:
	update_stat(rejected_registrations, 1);

	if ( !is_cflag_set(REG_SAVE_NORPL_FL) )
		send_reply(_m);

	return 0;
}

