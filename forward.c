/*
 * $Id: forward.c 3224 2007-11-28 17:15:54Z henningw $
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005-2007 Voice Sistem SRL
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
 *  2001-??-??  created by andrei
 *  ????-??-??  lots of changes by a lot of people
 *  2003-01-23  support for determination of outbound interface added :
 *               get_out_socket (jiri)
 *  2003-01-24  reply to rport support added, contributed by
 *               Maxim Sobolev <sobomax@FreeBSD.org> and modified by andrei
 *  2003-02-11  removed calls to upd_send & tcp_send & replaced them with
 *               calls to msg_send (andrei)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-02  fixed get_send_socket for tcp fwd to udp (andrei)
 *  2003-04-03  added su_setport (andrei)
 *  2003-04-04  update_sock_struct_from_via now differentiates between
 *               local replies  & "normal" replies (andrei)
 *  2003-04-12  update_sock_struct_from via uses also FL_FORCE_RPORT for
 *               local replies (andrei)
 *  2003-08-21  check_self properly handles ipv6 addresses & refs   (andrei)
 *  2003-10-21  check_self updated to handle proto (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2004-10-10  modified check_self to use grep_sock_info (andrei)
 *  2004-11-08  added force_send_socket support in get_send_socket (andrei)
 *  2006-09-06  added new algorithm for building VIA branch parameter for 
 *              stateless requests - it complies to RFC3261 requirement to be
 *              unique through time and space (bogdan)
 */



#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "forward.h"
#include "parser/msg_parser.h"
#include "dprint.h"
#include "ut.h"
#include "mem/mem.h"
#include "msg_translator.h"
#include "sr_module.h"
#include "ip_addr.h"
#include "resolve.h"
#include "name_alias.h"
#include "socket_info.h"
#include "core_stats.h"
#include "blacklists.h"



/* return a socket_info_pointer to the sending socket; as opposed to
 * get_send_socket, which returns process's default socket, get_out_socket
 * attempts to determine the outbound interface which will be used;
 * it creates a temporary connected socket to determine it; it will
 * be very likely noticeably slower, but it can deal better with
 * multihomed hosts
 */
struct socket_info* get_out_socket(union sockaddr_union* to, int proto)
{
	int temp_sock;
	socklen_t len;
	union sockaddr_union from; 
	struct socket_info* si;
	struct ip_addr ip;

	if (proto!=PROTO_UDP) {
		LM_CRIT("can only be called for UDP\n");
		return 0;
	}
	
	temp_sock=socket(to->s.sa_family, SOCK_DGRAM, 0 );
	if (temp_sock==-1) {
		LM_ERR("socket() failed: %s\n", strerror(errno));
		return 0;
	}
	if (connect(temp_sock, &to->s, sockaddru_len(*to))==-1) {
		LM_ERR("connect failed: %s\n", strerror(errno));
		goto error;
	}
	len=sizeof(from);
	if (getsockname(temp_sock, &from.s, &len)==-1) {
		LM_ERR("getsockname failed: %s\n", strerror(errno));
		goto error;
	}
	su2ip_addr(&ip, &from);
	si=find_si(&ip, 0, proto);
	if (si==0) goto error;
	close(temp_sock);
	LM_DBG("socket determined: %p\n", si );
	return si;
error:
	LM_ERR("no socket found\n");
	close(temp_sock);
	return 0;
}



/* returns a socket_info pointer to the sending socket or 0 on error
 * params: sip msg (can be null), destination socket_union pointer, protocol
 * if msg!=null and msg->force_send_socket, the force_send_socket will be
 * used
 */
struct socket_info* get_send_socket(struct sip_msg *msg, 
										union sockaddr_union* to, int proto)
{
	struct socket_info* send_sock;
	
	/* check if send interface is not forced */
	if (msg && msg->force_send_socket){
		if (msg->force_send_socket->proto!=proto){
			LM_DBG("force_send_socket of different proto (%d)!\n", proto);
			msg->force_send_socket=find_si(&(msg->force_send_socket->address),
											msg->force_send_socket->port_no,
											proto);
		}
		if (msg->force_send_socket && (msg->force_send_socket->socket!=-1)) 
			return msg->force_send_socket;
		else{
			if (msg->force_send_socket && msg->force_send_socket->socket==-1)
				LM_WARN("not listening on the requested socket, no fork mode?\n");
			else
				LM_WARN("protocol/port mismatch\n");
		}
	};

	if (mhomed && proto==PROTO_UDP){
		send_sock=get_out_socket(to, proto);
		if ((send_sock==0) || (send_sock->socket!=-1))
			return send_sock; /* found or error*/
		else if (send_sock->socket==-1){
			LM_WARN("not listening on the requested socket, no fork mode?\n");
			/* continue: try to use some socket */
		}
	}

	send_sock=0;
	/* check if we need to change the socket (different address families -
	 * eg: ipv4 -> ipv6 or ipv6 -> ipv4) */
	switch(proto){
#ifdef USE_TCP
		case PROTO_TCP:
		/* on tcp just use the "main address", we don't really now the
		 * sending address (we can find it out, but we'll need also to see
		 * if we listen on it, and if yes on which port -> too complicated*/
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tcp;
								break;
#ifdef USE_IPV6
				case AF_INET6:	send_sock=sendipv6_tcp;
								break;
#endif
				default:	LM_ERR("don't know how to forward to af %d\n", 
								to->s.sa_family);
			}
			break;
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tls;
								break;
#ifdef USE_IPV6
				case AF_INET6:	send_sock=sendipv6_tls;
								break;
#endif
				default:	LM_ERR("don't know how"
									" to forward to af %d\n", to->s.sa_family);
			}
			break;
#endif /* USE_TLS */
#ifdef USE_SCTP
		case PROTO_SCTP:
			switch(to->s.sa_family){
				case AF_INET:	send_sock=sendipv4_sctp;
								break;
#ifdef USE_IPV6
				case AF_INET6:	send_sock=sendipv6_sctp;
								break;
#endif
				default:	LM_ERR("don't know how to forward to af %d\n", 
								to->s.sa_family);
			}
			break;
#endif /* USE_SCTP */
		case PROTO_UDP:
			if ((bind_address==0)||(to->s.sa_family!=bind_address->address.af)||
				  (bind_address->proto!=PROTO_UDP)){
				switch(to->s.sa_family){
					case AF_INET:	send_sock=sendipv4;
									break;
#ifdef USE_IPV6
					case AF_INET6:	send_sock=sendipv6;
									break;
#endif
					default:	LM_ERR("don't know how to forward to af %d\n",
										to->s.sa_family);
				}
			}else send_sock=bind_address;
			break;
		default:
			LM_CRIT("unknown proto %d\n", proto);
	}
	return send_sock;
}



/* checks if the proto: host:port is one of the address we listen on;
 * if port==0, the  port number is ignored
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns 1 if true, 0 if false, -1 on error
 * WARNING: uses str2ip6 so it will overwrite any previous
 *  unsaved result of this function (static buffer)
 */
int check_self(str* host, unsigned short port, unsigned short proto)
{
	if (grep_sock_info(host, port, proto)) goto found;
	/* try to look into the aliases*/
	if (grep_aliases(host->s, host->len, port, proto)==0){
		LM_DBG("host != me\n");
		return 0;
	}
found:
	return 1;
}



static inline str* get_sl_branch(struct sip_msg* msg)
{
	static str default_branch = str_init("0");
	struct hdr_field *h_via;
	struct via_body  *b_via;
	str *branch;
	int via_parsed;

	via_parsed = 0;
	branch = 0;

	/* first VIA header must be parsed */
	for( h_via=msg->h_via1 ; h_via ; h_via=h_via->sibling ) {

		b_via = (struct via_body*)h_via->parsed;
		for( ; b_via ; b_via=b_via->next ) {
			/* check if there is any valid branch param */
			if (b_via->branch==0 || b_via->branch->value.s==0
			|| b_via->branch->value.len==0 )
				continue;
			branch = &b_via->branch->value;
			/* check if the branch param has the magic cookie */
			if (branch->len <= (int)MCOOKIE_LEN
			|| memcmp( branch->s, MCOOKIE, MCOOKIE_LEN)!=0 )
				continue;
			/* found a statefull branch -> use it */
			goto found;
		}

		if (!via_parsed) {
			if ( parse_headers(msg,HDR_EOH_F,0)<0 ) {
				LM_ERR("failed to parse all hdrs\n");
				return 0;
			}
			via_parsed = 1;
		}
	}

	/* no statefull branch :(.. -> use the branch from the last via */
found:
	if (branch) {
		if (branch->len>(int)MAX_BRANCH_PARAM_LEN)
			branch->len = MAX_BRANCH_PARAM_LEN;
	} else {
		/* no branch found/...use a default one*/
		branch = &default_branch;
	}
	return branch;
}



int forward_request( struct sip_msg* msg, struct proxy_l * p)
{
	union sockaddr_union to;
	unsigned int len;
	char* buf;
	struct socket_info* send_sock;
	struct socket_info* last_sock;
	str *branch;

	buf=0;

	/* calculate branch for outbound request - if the branch buffer is already
	 * set (maybe by an upper level as TM), used it; otherwise computes 
	 * the branch for stateless fwd. . According to the latest discussions
	 * on the topic, you should reuse the latest statefull branch
	 * --bogdan */
	if ( msg->add_to_branch_len==0 ) {
		branch = get_sl_branch(msg);
		if (branch==0) {
			LM_ERR("unable to compute branch\n");
			goto error;
		}
		msg->add_to_branch_len = branch->len;
		memcpy( msg->add_to_branch_s, branch->s, branch->len);
	}

	hostent2su( &to, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);
	last_sock = 0;

	do {
		send_sock=get_send_socket( msg, &to, p->proto);
		if (send_sock==0){
			LM_ERR("cannot forward to af %d, proto %d no corresponding"
				"listening socket\n", to.s.sa_family, p->proto);
			ser_error=E_NO_SOCKET;
			continue;
		}

		if ( last_sock!=send_sock ) {

			if (buf)
				pkg_free(buf);

			buf = build_req_buf_from_sip_req( msg, &len, send_sock, p->proto);
			if (!buf){
				LM_ERR("building req buf failed\n");
				goto error;
			}

			last_sock = send_sock;
		}

		if (check_blacklists( p->proto, &to, buf, len)) {
			LM_DBG("blocked by blacklists\n");
			ser_error=E_IP_BLOCKED;
			continue;
		}

		/* send it! */
		LM_DBG("sending:\n%.*s.\n", (int)len, buf);
		LM_DBG("orig. len=%d, new_len=%d, proto=%d\n",
			msg->len, len, p->proto );

		if (msg_send(send_sock, p->proto, &to, 0, buf, len)<0){
			ser_error=E_SEND;
			continue;
		}

		ser_error = 0;
		break;

	}while( get_next_su( p, &to, (ser_error==E_IP_BLOCKED)?0:1)==0 );

	if (ser_error) {
		update_stat( drp_reqs, 1);
		goto error;
	}

	/* sent requests stats */
	update_stat( fwd_reqs, 1);

	pkg_free(buf);
	/* received_buf & line_buf will be freed in receive_msg by free_lump_list*/
	return 0;

error:
	if (buf) pkg_free(buf);
	return -1;
}



int update_sock_struct_from_via( union sockaddr_union* to,
								 struct sip_msg* msg,
								 struct via_body* via )
{
	struct hostent* he;
	str* name;
	int err;
	unsigned short port;

	port=0;
	if(via==msg->via1){ 
		/* _local_ reply, we ignore any rport or received value
		 * (but we will send back to the original port if rport is
		 *  present) */
		if ((msg->msg_flags&FL_FORCE_RPORT)||(via->rport))
			port=msg->rcv.src_port;
		else port=via->port;
		if(via->maddr)
			name= &(via->maddr->value);
		else
			name=&(via->host); /* received=ip in 1st via is ignored (it's
							  not added by us so it's bad) */
	}else{
		/* "normal" reply, we use rport's & received value if present */
		if (via->rport && via->rport->value.s){
			LM_DBG("using 'rport'\n");
			port=str2s(via->rport->value.s, via->rport->value.len, &err);
			if (err){
				LM_NOTICE("bad rport value(%.*s)\n",
					via->rport->value.len,via->rport->value.s);
				port=0;
			}
		}

		if (via->maddr){
			name= &(via->maddr->value);
			if (port==0) port=via->port?via->port:SIP_PORT; 
		} else if (via->received){
			LM_DBG("using 'received'\n");
			name=&(via->received->value);
			/* making sure that we won't do SRV lookup on "received" */
			if (port==0) port=via->port?via->port:SIP_PORT; 
		}else{
			LM_DBG("using via host\n");
			name=&(via->host);
			if (port==0) port=via->port;
		}
	}
	LM_DBG("trying SRV lookup\n");
	he=sip_resolvehost(name, &port, &via->proto, 0, 0);

	if (he==0){
		LM_NOTICE("resolve_host(%.*s) failure\n", name->len, name->s);
		return -1;
	}
		
	hostent2su( to, he, 0, port);
	return 1;
}



/* removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	char* new_buf;
	union sockaddr_union* to;
	unsigned int new_len;
	struct sr_module *mod;
	int proto;
	int id; /* used only by tcp*/
	struct socket_info *send_sock;
#ifdef USE_TCP
	char* s;
	int len;
#endif
	
	to=0;
	id=0;
	new_buf=0;
	/*check if first via host = us */
	if (check_via){
		if (check_self(&msg->via1->host,
					msg->via1->port?msg->via1->port:SIP_PORT,
					msg->via1->proto)!=1){
			LM_ERR("host in first via!=me : %.*s:%d\n", 
				msg->via1->host.len, msg->via1->host.s,	msg->via1->port);
			/* send error msg back? */
			goto error;
		}
	}
	/* quick hack, slower for multiple modules*/
	for (mod=modules;mod;mod=mod->next){
		if ((mod->exports) && (mod->exports->response_f)){
			LM_DBG("found module %s, passing reply to it\n",
					mod->exports->name);
			if (mod->exports->response_f(msg)==0) goto skip;
		}
	}

	/* we have to forward the reply stateless, so we need second via -bogdan*/
	if (parse_headers( msg, HDR_VIA2_F, 0 )==-1 
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
	{
		/* no second via => error */
		LM_ERR("no 2nd via found in reply\n");
		goto error;
	}

	to=(union sockaddr_union*)pkg_malloc(sizeof(union sockaddr_union));
	if (to==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	new_buf = build_res_buf_from_sip_res( msg, &new_len);
	if (!new_buf){
		LM_ERR("failed to build rpl from req failed\n");
		goto error;
	}

	proto=msg->via2->proto;
	if (update_sock_struct_from_via( to, msg, msg->via2 )==-1) goto error;

#ifdef USE_TCP
	if (proto==PROTO_TCP
#ifdef USE_TLS
			|| proto==PROTO_TLS
#endif
			){
		/* find id in i param if it exists */
		if (msg->via1->i&&msg->via1->i->value.s){
			s=msg->via1->i->value.s;
			len=msg->via1->i->value.len;
			id=reverse_hex2int(s, len);
		}
	}
#endif

	send_sock = get_send_socket(msg, to, proto);

	if (msg_send(send_sock, proto, to, id, new_buf, new_len)<0) {
		update_stat( drp_rpls, 1);
		goto error0;
	}
	update_stat( fwd_rpls, 1);
	/*
	 * If no port is specified in the second via, then this
	 * message output a wrong port number - zero. Despite that
	 * the correct port is choosen in update_sock_struct_from_via,
	 * as its visible with su_getport(to); .
	 */
	LM_DBG("reply forwarded to %.*s:%d\n", msg->via2->host.len, 
		msg->via2->host.s, (unsigned short) msg->via2->port);

	pkg_free(new_buf);
	pkg_free(to);
skip:
	return 0;
error:
	update_stat( err_rpls, 1);
error0:
	if (new_buf) pkg_free(new_buf);
	if (to) pkg_free(to);
	return -1;
}
