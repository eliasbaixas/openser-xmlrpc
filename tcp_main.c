/*
 * $Id: tcp_main.c 3590 2008-01-28 17:46:56Z bogdan_iancu $
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
 * History:
 * --------
 *  2002-11-29  created by andrei
 *  2002-12-11  added tcp_send (andrei)
 *  2003-01-20  locking fixes, hashtables (andrei)
 *  2003-02-20  s/lock_t/gen_lock_t/ to avoid a conflict on solaris (andrei)
 *  2003-02-25  Nagle is disabled if -DDISABLE_NAGLE (andrei)
 *  2003-03-29  SO_REUSEADDR before calling bind to allow
 *              server restart, Nagle set on the (hopefuly) 
 *              correct socket (jiri)
 *  2003-03-31  always try to find the corresponding tcp listen socket for
 *               a temp. socket and store in in *->bind_address: added
 *               find_tcp_si, modified tcpconn_connect (andrei)
 *  2003-04-14  set sockopts to TOS low delay (andrei)
 *  2003-06-30  moved tcp new connect checking & handling to
 *               handle_new_connect (andrei)
 *  2003-07-09  tls_close called before closing the tcp connection (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2003-10-27  tcp port aliases support added (andrei)
 *  2003-11-04  always lock before manipulating refcnt; sendchild
 *              does not inc refcnt by itself anymore (andrei)
 *  2003-11-07  different unix sockets are used for fd passing
 *              to/from readers/writers (andrei)
 *  2003-11-17  handle_new_connect & tcp_connect will close the 
 *              new socket if tcpconn_new return 0 (e.g. out of mem) (andrei)
 *  2003-11-28  tcp_blocking_write & tcp_blocking_connect added (andrei)
 *  2004-11-08  dropped find_tcp_si and replaced with find_si (andrei)
 *  2005-12-22  added tos configurability (thanks to Andreas Granig)
 *  2005-06-07  new tcp optimized code, supports epoll (LT), sigio + real time
 *               signals, poll & select (andrei)
 *  2005-06-26  *bsd kqueue support (andrei)
 *  2005-07-04  solaris /dev/poll support (andrei)
 *  2005-07-08  tcp_max_connections, tcp_connection_lifetime, don't accept
 *               more connections if tcp_max_connections is exceeded (andrei)
 *  2005-10-21  cleanup all the open connections on exit
 *              decrement the no. of open connections on timeout too    (andrei)
 */


#ifdef USE_TCP


#ifndef SHM_MEM
#error "shared memory support needed (add -DSHM_MEM to Makefile.defs)"
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/uio.h>  /* writev*/
#include <netdb.h>
#include <stdlib.h> /*exit() */

#include <unistd.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_SELECT
#include <sys/select.h>
#endif
#include <sys/poll.h>


#include "ip_addr.h"
#include "pass_fd.h"
#include "tcp_conn.h"
#include "globals.h"
#include "pt.h"
#include "locking.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "timer.h"
#include "sr_module.h"
#include "tcp_server.h"
#include "tcp_init.h"
#include "tsend.h"
#include "ut.h"
#ifdef USE_TLS
#include "tls/tls_server.h"
#endif 

#define local_malloc pkg_malloc
#define local_free   pkg_free

#define HANDLE_IO_INLINE
#include "io_wait.h"
#include <fcntl.h> /* must be included after io_wait.h if SIGIO_RT is used */


enum fd_types { F_NONE, F_SOCKINFO /* a tcp_listen fd */,
				F_TCPCONN, F_TCPCHILD, F_PROC };

struct tcp_child{
	pid_t pid;
	int proc_no; /* ser proc_no, for debugging */
	int unix_sock; /* unix "read child" sock fd */
	int busy;
	int n_reqs; /* number of requests serviced so far */
};



int tcp_accept_aliases=0; /* by default don't accept aliases */
int tcp_connect_timeout=DEFAULT_TCP_CONNECT_TIMEOUT;
int tcp_send_timeout=DEFAULT_TCP_SEND_TIMEOUT;
int tcp_con_lifetime=DEFAULT_TCP_CONNECTION_LIFETIME;
enum poll_types tcp_poll_method=0; /* by default choose the best method */
int tcp_max_connections=DEFAULT_TCP_MAX_CONNECTIONS;
int tcp_max_fd_no=0;

static int tcp_connections_no=0; /* current open connections */

/* connection hash table (after ip&port) , includes also aliases */
struct tcp_conn_alias** tcpconn_aliases_hash=0;
/* connection hash table (after connection id) */
struct tcp_connection** tcpconn_id_hash=0;
gen_lock_t* tcpconn_lock=0;

struct tcp_child *tcp_children=0;
static int* connection_id=0; /*  unique for each connection, used for 
								quickly finding the corresponding connection
								for a reply */
int unix_tcp_sock = -1;

static int tcp_proto_no=-1; /* tcp protocol number as returned by
							   getprotobyname */

static io_wait_h io_h;



/* set all socket/fd options:  disable nagle, tos lowdelay, non-blocking
 * return -1 on error */
static int init_sock_opt(int s)
{
	int flags;
	int optval;
	
#ifdef DISABLE_NAGLE
	flags=1;
	if ( (tcp_proto_no!=-1) && (setsockopt(s, tcp_proto_no , TCP_NODELAY,
					&flags, sizeof(flags))<0) ){
		LM_WARN("could not disable Nagle: %s\n", strerror(errno));
	}
#endif
	/* tos*/
	optval = tos;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,sizeof(optval)) ==-1){
		LM_WARN("setsockopt tos: %s\n",	strerror(errno));
		/* continue since this is not critical */
	}
	/* non-blocking */
	flags=fcntl(s, F_GETFL);
	if (flags==-1){
		LM_ERR("fnctl failed: (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	if (fcntl(s, F_SETFL, flags|O_NONBLOCK)==-1){
		LM_ERR("set non-blocking failed: (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}



/* blocking connect on a non-blocking fd; it will timeout after
 * tcp_connect_timeout 
 * if BLOCKING_USE_SELECT and HAVE_SELECT are defined it will internally
 * use select() instead of poll (bad if fd > FD_SET_SIZE, poll is preferred)
 */
static int tcp_blocking_connect(int fd, const struct sockaddr *servaddr,
								socklen_t addrlen)
{
	int n;
#if defined(HAVE_SELECT) && defined(BLOCKING_USE_SELECT)
	fd_set sel_set;
	fd_set orig_set;
	struct timeval timeout;
#else
	struct pollfd pf;
#endif
	int elapsed;
	int to;
	int ticks;
	int err;
	unsigned int err_len;
	int poll_err;
	
	poll_err=0;
	to=tcp_connect_timeout;
	ticks=get_ticks();
again:
	n=connect(fd, servaddr, addrlen);
	if (n==-1){
		if (errno==EINTR){
			elapsed=(get_ticks()-ticks)*TIMER_TICK;
			if (elapsed<to)		goto again;
			else goto error_timeout;
		}
		if (errno!=EINPROGRESS && errno!=EALREADY){
			LM_ERR("(%d) %s\n", errno, strerror(errno));
			goto error;
		}
	}else goto end;
	
	/* poll/select loop */
#if defined(HAVE_SELECT) && defined(BLOCKING_USE_SELECT)
		FD_ZERO(&orig_set);
		FD_SET(fd, &orig_set);
#else
		pf.fd=fd;
		pf.events=POLLOUT;
#endif
	while(1){
		elapsed=(get_ticks()-ticks)*TIMER_TICK;
		if (elapsed<to)
			to-=elapsed;
		else 
			goto error_timeout;
#if defined(HAVE_SELECT) && defined(BLOCKING_USE_SELECT)
		sel_set=orig_set;
		timeout.tv_sec=to;
		timeout.tv_usec=0;
		n=select(fd+1, 0, &sel_set, 0, &timeout);
#else
		n=poll(&pf, 1, to*1000);
#endif
		if (n<0){
			if (errno==EINTR) continue;
			LM_ERR("poll/select failed: (%d) %s\n", errno, strerror(errno));
			goto error;
		}else if (n==0) /* timeout */ continue;
#if defined(HAVE_SELECT) && defined(BLOCKING_USE_SELECT)
		if (FD_ISSET(fd, &sel_set))
#else
		if (pf.revents&(POLLERR|POLLHUP|POLLNVAL)){ 
			LM_ERR("poll error: flags %x\n", pf.revents);
			poll_err=1;
		}
#endif
		{
			err_len=sizeof(err);
			getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
			if ((err==0) && (poll_err==0)) goto end;
			if (err!=EINPROGRESS && err!=EALREADY){
				LM_ERR("failed to retrieve SO_ERROR (%d) %s\n", err,
						strerror(err));
				goto error;
			}
		}
	}
error_timeout:
	/* timeout */
	LM_ERR("timeout %d s elapsed from %d s\n", elapsed, tcp_connect_timeout);
error:
	return -1;
end:
	return 0;
}



#if 0
/* blocking write even on non-blocking sockets 
 * if TCP_TIMEOUT will return with error */
static int tcp_blocking_write(struct tcp_connection* c, int fd, char* buf,
								unsigned int len)
{
	int n;
	fd_set sel_set;
	struct timeval timeout;
	int ticks;
	int initial_len;
	
	initial_len=len;
again:
	
	n=send(fd, buf, len,
#ifdef HAVE_MSG_NOSIGNAL
			MSG_NOSIGNAL
#else
			0
#endif
		);
	if (n<0){
		if (errno==EINTR)	goto again;
		else if (errno!=EAGAIN && errno!=EWOULDBLOCK){
			LM_ERR("failed to send: (%d) %s\n",	errno, strerror(errno));
			goto error;
		}
	}else if (n<len){
		/* partial write */
		buf+=n;
		len-=n;
	}else{
		/* success: full write */
		goto end;
	}
	while(1){
		FD_ZERO(&sel_set);
		FD_SET(fd, &sel_set);
		timeout.tv_sec=tcp_send_timeout;
		timeout.tv_usec=0;
		ticks=get_ticks();
		n=select(fd+1, 0, &sel_set, 0, &timeout);
		if (n<0){
			if (errno==EINTR) continue; /* signal, ignore */
			LM_ERR("select failed: (%d) %s\n", errno, strerror(errno));
			goto error;
		}else if (n==0){
			/* timeout */
			if (get_ticks()-ticks>=tcp_send_timeout){
				LM_ERR("send timeout (%d)\n", tcp_send_timeout);
				goto error;
			}
			continue;
		}
		if (FD_ISSET(fd, &sel_set)){
			/* we can write again */
			goto again;
		}
	}
error:
		return -1;
end:
		return initial_len;
}
#endif



struct tcp_connection* tcpconn_new(int sock, union sockaddr_union* su,
									struct socket_info* ba, int type, 
									int state)
{
	struct tcp_connection *c;
	
	c=(struct tcp_connection*)shm_malloc(sizeof(struct tcp_connection));
	if (c==0){
		LM_ERR("shared memory allocation failure\n");
		goto error;
	}
	memset(c, 0, sizeof(struct tcp_connection)); /* zero init */
	c->s=sock;
	c->fd=-1; /* not initialized */
	if (lock_init(&c->write_lock)==0){
		LM_ERR("init lock failed\n");
		goto error;
	}
	
	c->rcv.src_su=*su;
	
	c->refcnt=0;
	su2ip_addr(&c->rcv.src_ip, su);
	c->rcv.src_port=su_getport(su);
	c->rcv.bind_address=ba;
	if (ba){
		c->rcv.dst_ip=ba->address;
		c->rcv.dst_port=ba->port_no;
	}
	print_ip("tcpconn_new: new tcp connection to: ", &c->rcv.src_ip, "\n");
	LM_DBG("on port %d, type %d\n", c->rcv.src_port, type);
	init_tcp_req(&c->req);
	c->id=(*connection_id)++;
	c->rcv.proto_reserved1=0; /* this will be filled before receive_message*/
	c->rcv.proto_reserved2=0;
	c->state=state;
	c->extra_data=0;
#ifdef USE_TLS
	if (type==PROTO_TLS){
		if (tls_tcpconn_init(c, sock)==-1) goto error;
	}else
#endif /* USE_TLS*/
	{
		c->type=PROTO_TCP;
		c->rcv.proto=PROTO_TCP;
		c->timeout=get_ticks()+tcp_con_lifetime;
	}
	c->flags|=F_CONN_REMOVED;
	
	tcp_connections_no++;
	return c;
	
error:
	if (c) shm_free(c);
	return 0;
}



struct tcp_connection* tcpconn_connect(struct socket_info* send_sock,
		union sockaddr_union* server, int type)
{
	int s;
	union sockaddr_union my_name;
	socklen_t my_name_len;
	struct tcp_connection* con;

	s=socket(AF2PF(server->s.sa_family), SOCK_STREAM, 0);
	if (s==-1){
		LM_ERR("socket: (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	if (init_sock_opt(s)<0){
		LM_ERR("init_sock_opt failed\n");
		goto error;
	}
	my_name_len = sockaddru_len(send_sock->su);
	memcpy( &my_name, &send_sock->su, my_name_len);
	su_setport( &my_name, 0);
	if (bind(s, &my_name.s, my_name_len )!=0) {
		LM_ERR("bind failed (%d) %s\n", errno,strerror(errno));
		goto error;
	}

	if (tcp_blocking_connect(s, &server->s, sockaddru_len(*server))<0){
		LM_ERR("tcp_blocking_connect failed\n");
		goto error;
	}
	con=tcpconn_new(s, server, send_sock, type, S_CONN_CONNECT);
	if (con==0){
		LM_ERR("tcpconn_new failed, closing the socket\n");
		goto error;
	}
	return con;
	/*FIXME: set sock idx! */
error:
	if (s!=-1) close(s); /* close the opened socket */
	return 0;
}



struct tcp_connection*  tcpconn_add(struct tcp_connection *c)
{
	unsigned hash;

	if (c){
		TCPCONN_LOCK;
		/* add it at the begining of the list*/
		hash=tcp_id_hash(c->id);
		c->id_hash=hash;
		tcpconn_listadd(tcpconn_id_hash[hash], c, id_next, id_prev);
		
		hash=tcp_addr_hash(&c->rcv.src_ip, c->rcv.src_port);
		/* set the first alias */
		c->con_aliases[0].port=c->rcv.src_port;
		c->con_aliases[0].hash=hash;
		c->con_aliases[0].parent=c;
		tcpconn_listadd(tcpconn_aliases_hash[hash], &c->con_aliases[0],
						next, prev);
		c->aliases++;
		TCPCONN_UNLOCK;
		LM_DBG("hashes: %d, %d\n", hash, c->id_hash);
		return c;
	}else{
		LM_CRIT("null connection pointer\n");
		return 0;
	}
}


/* unsafe tcpconn_rm version (nolocks) */
void _tcpconn_rm(struct tcp_connection* c)
{
	int r;
	tcpconn_listrm(tcpconn_id_hash[c->id_hash], c, id_next, id_prev);
	/* remove all the aliases */
	for (r=0; r<c->aliases; r++)
		tcpconn_listrm(tcpconn_aliases_hash[c->con_aliases[r].hash], 
						&c->con_aliases[r], next, prev);
	lock_destroy(&c->write_lock);
#ifdef USE_TLS
	if (c->type==PROTO_TLS) tls_tcpconn_clean(c);
#endif
	shm_free(c);
}



void tcpconn_rm(struct tcp_connection* c)
{
	int r;
	TCPCONN_LOCK;
	tcpconn_listrm(tcpconn_id_hash[c->id_hash], c, id_next, id_prev);
	/* remove all the aliases */
	for (r=0; r<c->aliases; r++)
		tcpconn_listrm(tcpconn_aliases_hash[c->con_aliases[r].hash], 
						&c->con_aliases[r], next, prev);
	TCPCONN_UNLOCK;
	lock_destroy(&c->write_lock);
#ifdef USE_TLS
	if ((c->type==PROTO_TLS)&&(c->extra_data)) tls_tcpconn_clean(c);
#endif
	shm_free(c);
}


/* finds a connection, if id=0 uses the ip addr & port (host byte order)
 * WARNING: unprotected (locks) use tcpconn_get unless you really
 * know what you are doing */
struct tcp_connection* _tcpconn_find(int id, struct ip_addr* ip, int port)
{

	struct tcp_connection *c;
	struct tcp_conn_alias* a;
	unsigned hash;
	
#ifdef EXTRA_DEBUG
	LM_DBG("%d  port %d\n",id, port);
	if (ip) print_ip("tcpconn_find: ip ", ip, "\n");
#endif
	if (id){
		hash=tcp_id_hash(id);
		for (c=tcpconn_id_hash[hash]; c; c=c->id_next){
#ifdef EXTRA_DEBUG
			LM_DBG("c=%p, c->id=%d, port=%d\n",c, c->id, c->rcv.src_port);
			print_ip("ip=", &c->rcv.src_ip, "\n");
#endif
			if ((id==c->id)&&(c->state!=S_CONN_BAD)) return c;
		}
	}else if (ip){
		hash=tcp_addr_hash(ip, port);
		for (a=tcpconn_aliases_hash[hash]; a; a=a->next){
#ifdef EXTRA_DEBUG
			LM_DBG("a=%p, c=%p, c->id=%d, alias port= %d port=%d\n", 
				a, a->parent, a->parent->id, a->port, a->parent->rcv.src_port);
			print_ip("ip=",&a->parent->rcv.src_ip,"\n");
#endif
			if ( (a->parent->state!=S_CONN_BAD) && (port==a->port) &&
					(ip_addr_cmp(ip, &a->parent->rcv.src_ip)) )
				return a->parent;
		}
	}
	return 0;
}



/* _tcpconn_find with locks and timeout */
struct tcp_connection* tcpconn_get(int id, struct ip_addr* ip, int port,
									int timeout)
{
	struct tcp_connection* c;
	TCPCONN_LOCK;
	c=_tcpconn_find(id, ip, port);
	if (c) {
			c->refcnt++;
			c->timeout=get_ticks()+timeout;
	}
	TCPCONN_UNLOCK;
	return c;
}



/* add port as an alias for the "id" connection
 * returns 0 on success,-1 on failure */
int tcpconn_add_alias(int id, int port, int proto)
{
	struct tcp_connection* c;
	unsigned hash;
	struct tcp_conn_alias* a;
	
	a=0;
	/* fix the port */
	port=port?port:((proto==PROTO_TLS)?SIPS_PORT:SIP_PORT);
	TCPCONN_LOCK;
	/* check if alias already exists */
	c=_tcpconn_find(id, 0, 0);
	if (c){
		hash=tcp_addr_hash(&c->rcv.src_ip, port);
		/* search the aliases for an already existing one */
		for (a=tcpconn_aliases_hash[hash]; a; a=a->next){
			if ( (a->parent->state!=S_CONN_BAD) && (port==a->port) &&
					(ip_addr_cmp(&c->rcv.src_ip, &a->parent->rcv.src_ip)) ){
				/* found */
				if (a->parent!=c) goto error_sec;
				else goto ok;
			}
		}
		if (c->aliases>=TCP_CON_MAX_ALIASES) goto error_aliases;
		c->con_aliases[c->aliases].parent=c;
		c->con_aliases[c->aliases].port=port;
		c->con_aliases[c->aliases].hash=hash;
		tcpconn_listadd(tcpconn_aliases_hash[hash], 
								&c->con_aliases[c->aliases], next, prev);
		c->aliases++;
	}else goto error_not_found;
ok:
	TCPCONN_UNLOCK;
#ifdef EXTRA_DEBUG
	if (a) LM_DBG("alias already present\n");
	else   LM_DBG("alias port %d for hash %d, id %d\n", port, hash, c->id);
#endif
	return 0;
error_aliases:
	TCPCONN_UNLOCK;
	LM_ERR("too many aliases for connection %p (%d)\n", c, c->id);
	return -1;
error_not_found:
	TCPCONN_UNLOCK;
	LM_ERR("no connection found for id %d\n",id);
	return -1;
error_sec:
	TCPCONN_UNLOCK;
	LM_ERR("possible port hijack attempt\n");
	LM_ERR("alias already present and points to another connection "
			"(%d : %d and %d : %d)\n", a->parent->id,  port, c->id, port);
	return -1;
}



void tcpconn_ref(struct tcp_connection* c)
{
	TCPCONN_LOCK;
	c->refcnt++; /* FIXME: atomic_dec */
	TCPCONN_UNLOCK;
}



void tcpconn_put(struct tcp_connection* c)
{
	TCPCONN_LOCK;
	c->refcnt--; /* FIXME: atomic_dec */
	TCPCONN_UNLOCK;
}



/* finds a tcpconn & sends on it */
int tcp_send(struct socket_info* send_sock, int type, char* buf, unsigned len,
			union sockaddr_union* to, int id)
{
	struct tcp_connection *c;
	struct tcp_connection *tmp;
	struct ip_addr ip;
	int port;
	int fd;
	long response[2];
	int n;
	
	port=0;
	if (to){
		su2ip_addr(&ip, to);
		port=su_getport(to);
		c=tcpconn_get(id, &ip, port, tcp_con_lifetime); 
	}else if (id){
		c=tcpconn_get(id, 0, 0, tcp_con_lifetime);
	}else{
		LM_CRIT("tcp_send called with null id & to\n");
		return -1;
	}
	
	if (id){
		if (c==0) {
			if (to){
				/* try again w/o id */
				c=tcpconn_get(0, &ip, port, tcp_con_lifetime);
				goto no_id;
			}else{
				LM_ERR("id %d not found, dropping\n", id);
				return -1;
			}
		}else goto get_fd;
	}
no_id:
		if (c==0){
			LM_DBG("no open tcp connection found, opening new one\n");
			/* create tcp connection */
			if ((c=tcpconn_connect(send_sock, to, type))==0){
				LM_ERR("connect failed\n");
				return -1;
			}
			c->refcnt++; /* safe to do it w/o locking, it's not yet
							available to the rest of the world */
			fd=c->s;
			
			/* send the new tcpconn to "tcp main" */
			response[0]=(long)c;
			response[1]=CONN_NEW;
			n=send_fd(unix_tcp_sock, response, sizeof(response), c->s);
			if (n<=0){
				LM_ERR("failed send_fd: %s (%d)\n",	strerror(errno), errno);
				n=-1;
				goto end;
			}	
			goto send_it;
		}
get_fd:
			/* todo: see if this is not the same process holding
			 *  c  and if so send directly on c->fd */
			LM_DBG("tcp connection found (%p), acquiring fd\n", c);
			/* get the fd */
			response[0]=(long)c;
			response[1]=CONN_GET_FD;
			n=send_all(unix_tcp_sock, response, sizeof(response));
			if (n<=0){
				LM_ERR("failed to get fd(write):%s (%d)\n",	
						strerror(errno), errno);
				n=-1;
				goto release_c;
			}
			LM_DBG("c= %p, n=%d\n", c, n);
			tmp=c;
			n=receive_fd(unix_tcp_sock, &c, sizeof(c), &fd, MSG_WAITALL);
			if (n<=0){
				LM_ERR("failed to get fd(receive_fd):"
							" %s (%d)\n", strerror(errno), errno);
				n=-1;
				goto release_c;
			}
			if (c!=tmp){
				LM_CRIT("got different connection:"
						"  %p (id= %d, refcnt=%d state=%d != "
						"  %p (id= %d, refcnt=%d state=%d (n=%d)\n",
						  c,   c->id,   c->refcnt,   c->state,
						  tmp, tmp->id, tmp->refcnt, tmp->state, n
				   );
				n=-1; /* fail */
				goto end;
			}
			LM_DBG("after receive_fd: c= %p n=%d fd=%d\n",c, n, fd);
		
	
	
send_it:
	LM_DBG("sending...\n");
	lock_get(&c->write_lock);
#ifdef USE_TLS
	if (c->type==PROTO_TLS)
		n=tls_blocking_write(c, fd, buf, len);
	else
#endif
		/* n=tcp_blocking_write(c, fd, buf, len); */
		n=tsend_stream(fd, buf, len, tcp_send_timeout*1000); 
	lock_release(&c->write_lock);
	LM_DBG("after write: c= %p n=%d fd=%d\n",c, n, fd);
	LM_DBG("buf=\n%.*s\n", (int)len, buf);
	if (n<0){
		LM_ERR("failed to send\n");
		/* error on the connection , mark it as bad and set 0 timeout */
		c->state=S_CONN_BAD;
		c->timeout=0;
		/* tell "main" it should drop this (optional it will t/o anyway?)*/
		response[0]=(long)c;
		response[1]=CONN_ERROR;
		n=send_all(unix_tcp_sock, response, sizeof(response));
		/* CONN_ERROR will auto-dec refcnt => we must not call tcpconn_put !!*/
		if (n<=0){
			LM_ERR("return failed (write):%s (%d)\n",
					strerror(errno), errno);
		}
		close(fd);
		return -1; /* error return, no tcpconn_put */
	}
end:
	close(fd);
release_c:
	tcpconn_put(c); /* release c (lock; dec refcnt; unlock) */
	return n;
}



int tcp_init(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;
#ifdef DISABLE_NAGLE
	int flag;
	struct protoent* pe;

	if (tcp_proto_no==-1){ /* if not already set */
		pe=getprotobyname("tcp");
		if (pe==0){
			LM_ERR("could not get TCP protocol number\n");
			tcp_proto_no=-1;
		}else{
			tcp_proto_no=pe->p_proto;
		}
	}
#endif
	
	addr=&sock_info->su;
	/* sock_info->proto=PROTO_TCP; */
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LM_ERR("could no init sockaddr_union\n");
		goto error;
	}
	sock_info->socket=socket(AF2PF(addr->s.sa_family), SOCK_STREAM, 0);
	if (sock_info->socket==-1){
		LM_ERR("socket: %s\n", strerror(errno));
		goto error;
	}
#ifdef DISABLE_NAGLE
	flag=1;
	if ( (tcp_proto_no!=-1) &&
		 (setsockopt(sock_info->socket, tcp_proto_no , TCP_NODELAY,
					 &flag, sizeof(flag))<0) ){
		LM_ERR("could not disable Nagle: %s\n",	strerror(errno));
	}
#endif


#if  !defined(TCP_DONT_REUSEADDR) 
	/* Stevens, "Network Programming", Section 7.5, "Generic Socket
     * Options": "...server started,..a child continues..on existing
	 * connection..listening server is restarted...call to bind fails
	 * ... ALL TCP servers should specify the SO_REUSEADDRE option 
	 * to allow the server to be restarted in this situation
	 *
	 * Indeed, without this option, the server can't restart.
	 *   -jiri
	 */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR,
				(void*)&optval, sizeof(optval))==-1) {
		LM_ERR("setsockopt %s\n", strerror(errno));
		goto error;
	}
#endif
	/* tos */
	optval = tos;
	if (setsockopt(sock_info->socket, IPPROTO_IP, IP_TOS, (void*)&optval, 
				sizeof(optval)) ==-1){
		LM_WARN("setsockopt tos: %s\n", strerror(errno));
		/* continue since this is not critical */
	}
	if (bind(sock_info->socket, &addr->s, sockaddru_len(*addr))==-1){
		LM_ERR("bind(%x, %p, %d) on %s:%d : %s\n",
 				sock_info->socket, &addr->s, 
 				(unsigned)sockaddru_len(*addr),
 				sock_info->address_str.s,
				sock_info->port_no,
 				strerror(errno));
		goto error;
	}
	if (listen(sock_info->socket, 10)==-1){
		LM_ERR("listen(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
		goto error;
	}
	
	return 0;
error:
	if (sock_info->socket!=-1){
		close(sock_info->socket);
		sock_info->socket=-1;
	}
	return -1;
}



static int send2child(struct tcp_connection* tcpconn)
{
	int i;
	int min_busy;
	int idx;
	
	min_busy=tcp_children[0].busy;
	idx=0;
	for (i=0; i<tcp_children_no; i++){
		if (!tcp_children[i].busy){
			idx=i;
			min_busy=0;
			break;
		}else if (min_busy>tcp_children[i].busy){
			min_busy=tcp_children[i].busy;
			idx=i;
		}
	}
	
	tcp_children[idx].busy++;
	tcp_children[idx].n_reqs++;
	if (min_busy){
		LM_WARN("no free tcp receiver, connection passed to the least"
				"busy one (%d)\n", min_busy);
	}
	LM_DBG("to tcp child %d %d(%d), %p\n", idx, tcp_children[idx].proc_no,
					tcp_children[idx].pid, tcpconn);
	if (send_fd(tcp_children[idx].unix_sock, &tcpconn, sizeof(tcpconn),
			tcpconn->s)<=0){
		LM_ERR("send_fd failed\n");
		return -1;
	}
	
	return 0;
}


/* handles a new connection, called internally by tcp_main_loop/handle_io.
 * params: si - pointer to one of the tcp socket_info structures on which
 *              an io event was detected (connection attempt)
 * returns:  handle_* return convention: -1 on error, 0 on EAGAIN (no more
 *           io events queued), >0 on success. success/error refer only to
 *           the accept.
 */
static inline int handle_new_connect(struct socket_info* si)
{
	union sockaddr_union su;
	struct tcp_connection* tcpconn;
	socklen_t su_len;
	int new_sock;
	
	/* got a connection on r */
	su_len=sizeof(su);
	new_sock=accept(si->socket, &(su.s), &su_len);
	if (new_sock==-1){
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK))
			return 0;
		LM_ERR("failed to accept connection(%d): %s\n", errno, strerror(errno));
		return -1;
	}
	if (tcp_connections_no>=tcp_max_connections){
		LM_ERR("maximum number of connections exceeded: %d/%d\n",
					tcp_connections_no, tcp_max_connections);
		close(new_sock);
		return 1; /* success, because the accept was succesfull */
	}
	if (init_sock_opt(new_sock)<0){
		LM_ERR("init_sock_opt failed\n");
		close(new_sock);
		return 1; /* success, because the accept was succesfull */
	}
	
	/* add socket to list */
	tcpconn=tcpconn_new(new_sock, &su, si, si->proto, S_CONN_ACCEPT);
	if (tcpconn){
		tcpconn->refcnt++; /* safe, not yet available to the
							  outside world */
		tcpconn_add(tcpconn);
		LM_DBG("new connection: %p %d flags: %04x\n", 
				tcpconn, tcpconn->s, tcpconn->flags);
		/* pass it to a child */
		if(send2child(tcpconn)<0){
			LM_ERR("no children available\n");
			TCPCONN_LOCK;
			tcpconn->refcnt--;
			if (tcpconn->refcnt==0){
				close(tcpconn->s);
				_tcpconn_rm(tcpconn);
			}else tcpconn->timeout=0; /* force expire */
			TCPCONN_UNLOCK;
		}
	}else{ /*tcpconn==0 */
		LM_ERR("tcpconn_new failed, closing socket\n");
		close(new_sock);
		
	}
	return 1; /* accept() was succesfull */
}



/* used internally by tcp_main_loop() */
static void tcpconn_destroy(struct tcp_connection* tcpconn)
{
	int fd;

	TCPCONN_LOCK; /*avoid races w/ tcp_send*/
	tcpconn->refcnt--;
	if (tcpconn->refcnt==0){ 
		LM_DBG("destroying connection %p, flags %04x\n",
				tcpconn, tcpconn->flags);
		fd=tcpconn->s;
#ifdef USE_TLS
		/*FIXME: lock ->writelock ? */
		if (tcpconn->type==PROTO_TLS)
			tls_close(tcpconn, fd);
#endif
		_tcpconn_rm(tcpconn);
		close(fd);
		tcp_connections_no--;
	}else{
		/* force timeout */
		tcpconn->timeout=0;
		tcpconn->state=S_CONN_BAD;
		LM_DBG("delaying (%p, flags %04x) ...\n",
				tcpconn, tcpconn->flags);
		
	}
	TCPCONN_UNLOCK;
}



/* handles an io event on one of the watched tcp connections
 * 
 * params: tcpconn - pointer to the tcp_connection for which we have an io ev.
 *         fd_i    - index in the fd_array table (needed for delete)
 * returns:  handle_* return convention, but on success it always returns 0
 *           (because it's one-shot, after a succesfull execution the fd is
 *            removed from tcp_main's watch fd list and passed to a child =>
 *            tcp_main is not interested in further io events that might be
 *            queued for this fd)
 */
inline static int handle_tcpconn_ev(struct tcp_connection* tcpconn, int fd_i)
{
	int fd;
	
	/*  is refcnt!=0 really necessary? 
	 *  No, in fact it's a bug: I can have the following situation: a send only
	 *   tcp connection used by n processes simultaneously => refcnt = n. In 
	 *   the same time I can have a read event and this situation is perfectly
	 *   valid. -- andrei
	 */
#if 0
	if ((tcpconn->refcnt!=0)){
		/* FIXME: might be valid for sigio_rt iff fd flags are not cleared
		 *        (there is a short window in which it could generate a sig
		 *         that would be catched by tcp_main) */
		LM_CRIT("io event on referenced tcpconn (%p), refcnt=%d, fd=%d\n",
					tcpconn, tcpconn->refcnt, tcpconn->s);
		return -1;
	}
#endif
	/* pass it to child, so remove it from the io watch list */
	LM_DBG("data available on %p %d\n", tcpconn, tcpconn->s);
	if (io_watch_del(&io_h, tcpconn->s, fd_i, 0)==-1) goto error;
	tcpconn->flags|=F_CONN_REMOVED;
	tcpconn_ref(tcpconn); /* refcnt ++ */
	if (send2child(tcpconn)<0){
		LM_ERR("no children available\n");
		TCPCONN_LOCK;
		tcpconn->refcnt--;
		if (tcpconn->refcnt==0){
			fd=tcpconn->s;
			_tcpconn_rm(tcpconn);
			close(fd);
		}else tcpconn->timeout=0; /* force expire*/
		TCPCONN_UNLOCK;
	}
	return 0; /* we are not interested in possibly queued io events, 
				 the fd was either passed to a child, or closed */
error:
	return -1;
}



static inline void set_tcp_timeout(struct tcp_connection *c)
{
	unsigned int timeout = get_ticks() + tcp_con_lifetime;

	if (c->lifetime) {
		if ( c->lifetime < timeout )
			c->timeout = timeout;
		else
			c->timeout = c->lifetime;
		c->lifetime = 0;
	} else {
		c->timeout = timeout;
	}
}


/* handles io from a tcp child process
 * params: tcp_c - pointer in the tcp_children array, to the entry for
 *                 which an io event was detected 
 *         fd_i  - fd index in the fd_array (usefull for optimizing
 *                 io_watch_deletes)
 * returns:  handle_* return convention: -1 on error, 0 on EAGAIN (no more
 *           io events queued), >0 on success. success/error refer only to
 *           the reads from the fd.
 */
inline static int handle_tcp_child(struct tcp_child* tcp_c, int fd_i)
{
	struct tcp_connection* tcpconn;
	long response[2];
	int cmd;
	int bytes;
	
	if (tcp_c->unix_sock<=0){
		/* (we can't have a fd==0, 0 is never closed )*/
		LM_CRIT("fd %d for %d (pid %d, ser no %d)\n", tcp_c->unix_sock,
				(int)(tcp_c-&tcp_children[0]), tcp_c->pid, tcp_c->proc_no);
		goto error;
	}
	/* read until sizeof(response)
	 * (this is a SOCK_STREAM so read is not atomic) */
	bytes=recv_all(tcp_c->unix_sock, response, sizeof(response), MSG_DONTWAIT);
	if (bytes<(int)sizeof(response)){
		if (bytes==0){
			/* EOF -> bad, child has died */
			LM_DBG("dead tcp child %d (pid %d, no %d)"
					" (shutting down?)\n", (int)(tcp_c-&tcp_children[0]), 
					tcp_c->pid, tcp_c->proc_no );
			/* don't listen on it any more */
			io_watch_del(&io_h, tcp_c->unix_sock, fd_i, 0); 
			goto error; /* eof. so no more io here, it's ok to return error */
		}else if (bytes<0){
			/* EAGAIN is ok if we try to empty the buffer
			 * e.g.: SIGIO_RT overflow mode or EPOLL ET */
			if ((errno!=EAGAIN) && (errno!=EWOULDBLOCK)){
				LM_CRIT("read from tcp child %ld (pid %d, no %d) %s [%d]\n",
						(long)(tcp_c-&tcp_children[0]), tcp_c->pid,
						tcp_c->proc_no, strerror(errno), errno );
			}else{
				bytes=0;
			}
			/* try to ignore ? */
			goto end;
		}else{
			/* should never happen */
			LM_CRIT("too few bytes received (%d)\n", bytes );
			bytes=0; /* something was read so there is no error; otoh if
					  receive_fd returned less then requested => the receive
					  buffer is empty => no more io queued on this fd */
			goto end;
		}
	}
	
	LM_DBG("reader response= %lx, %ld from %d \n",
					response[0], response[1], (int)(tcp_c-&tcp_children[0]));
	cmd=response[1];
	tcpconn=(struct tcp_connection*)response[0];
	if (tcpconn==0){
		/* should never happen */
		LM_CRIT("null tcpconn pointer received from tcp child %d (pid %d):"
					"%lx, %lx\n", (int)(tcp_c-&tcp_children[0]), tcp_c->pid,
					response[0], response[1]) ;
		goto end;
	}
	switch(cmd){
		case CONN_RELEASE:
			tcp_c->busy--;
			if (tcpconn->state==S_CONN_BAD){ 
				tcpconn_destroy(tcpconn);
				break;
			}
			/* update the timeout (lifetime) */
			set_tcp_timeout( tcpconn );
			tcpconn_put(tcpconn);
			/* must be after the de-ref*/
			io_watch_add(&io_h, tcpconn->s, F_TCPCONN, tcpconn);
			tcpconn->flags&=~F_CONN_REMOVED;
			LM_DBG("cmd CONN_RELEASE  %p refcnt= %d\n", 
											tcpconn, tcpconn->refcnt);
			break;
		case CONN_ERROR:
		case CONN_DESTROY:
		case CONN_EOF:
			/* WARNING: this will auto-dec. refcnt! */
				tcp_c->busy--;
				/* main doesn't listen on it => we don't have to delete it
				 if (tcpconn->s!=-1)
					io_watch_del(&io_h, tcpconn->s, -1, IO_FD_CLOSING);
				*/
				tcpconn_destroy(tcpconn); /* closes also the fd */
				break;
		default:
				LM_CRIT("unknown cmd %d from tcp reader %d\n",
									cmd, (int)(tcp_c-&tcp_children[0]));
	}
end:
	return bytes;
error:
	return -1;
}



/* handles io from a "generic" ser process (get fd or new_fd from a tcp_send)
 * 
 * params: p     - pointer in the ser processes array (pt[]), to the entry for
 *                 which an io event was detected
 *         fd_i  - fd index in the fd_array (usefull for optimizing
 *                 io_watch_deletes)
 * returns:  handle_* return convention:
 *          -1 on error reading from the fd,
 *           0 on EAGAIN  or when no  more io events are queued 
 *             (receive buffer empty),
 *           >0 on successfull reads from the fd (the receive buffer might
 *             be non-empty).
 */
inline static int handle_ser_child(struct process_table* p, int fd_i)
{
	struct tcp_connection* tcpconn;
	long response[2];
	int cmd;
	int bytes;
	int ret;
	int fd;
	
	ret=-1;
	if (p->unix_sock<=0){
		/* (we can't have a fd==0, 0 is never closed )*/
		LM_CRIT("fd %d for %d (pid %d)\n", 
				p->unix_sock, (int)(p-&pt[0]), p->pid);
		goto error;
	}
			
	/* get all bytes and the fd (if transmitted)
	 * (this is a SOCK_STREAM so read is not atomic) */
	bytes=receive_fd(p->unix_sock, response, sizeof(response), &fd,
						MSG_DONTWAIT);
	if (bytes<(int)sizeof(response)){
		/* too few bytes read */
		if (bytes==0){
			/* EOF -> bad, child has died */
			LM_DBG("dead child %d, pid %d"
					" (shutting down?)\n", (int)(p-&pt[0]), p->pid);
			/* don't listen on it any more */
			io_watch_del(&io_h, p->unix_sock, fd_i, 0);
			goto error; /* child dead => no further io events from it */
		}else if (bytes<0){
			/* EAGAIN is ok if we try to empty the buffer
			 * e.g: SIGIO_RT overflow mode or EPOLL ET */
			if ((errno!=EAGAIN) && (errno!=EWOULDBLOCK)){
				LM_CRIT("read from child %d (pid %d):  %s [%d]\n", 
						(int)(p-&pt[0]), p->pid, strerror(errno), errno);
				ret=-1;
			}else{
				ret=0;
			}
			/* try to ignore ? */
			goto end;
		}else{
			/* should never happen */
			LM_CRIT("too few bytes received (%d)\n", bytes );
			ret=0; /* something was read so there is no error; otoh if
					  receive_fd returned less then requested => the receive
					  buffer is empty => no more io queued on this fd */
			goto end;
		}
	}
	ret=1; /* something was received, there might be more queued */
	LM_DBG("read response= %lx, %ld, fd %d from %d (%d)\n",
					response[0], response[1], fd, (int)(p-&pt[0]), p->pid);
	cmd=response[1];
	tcpconn=(struct tcp_connection*)response[0];
	if (tcpconn==0){
		LM_CRIT("null tcpconn pointer received from child %d (pid %d)"
			"%lx, %lx\n", (int)(p-&pt[0]), p->pid, response[0], response[1]) ;
		goto end;
	}
	switch(cmd){
		case CONN_ERROR:
			if (!(tcpconn->flags & F_CONN_REMOVED) && (tcpconn->s!=-1)){
				io_watch_del(&io_h, tcpconn->s, -1, IO_FD_CLOSING);
				tcpconn->flags|=F_CONN_REMOVED;
			}
			tcpconn_destroy(tcpconn); /* will close also the fd */
			break;
		case CONN_GET_FD:
			/* send the requested FD  */
			/* WARNING: take care of setting refcnt properly to
			 * avoid race condition */
			if (send_fd(p->unix_sock, &tcpconn, sizeof(tcpconn),
							tcpconn->s)<=0){
				LM_ERR("send_fd failed\n");
			}
			break;
		case CONN_NEW:
			/* update the fd in the requested tcpconn*/
			/* WARNING: take care of setting refcnt properly to
			 * avoid race condition */
			if (fd==-1){
				LM_CRIT(" cmd CONN_NEW: no fd received\n");
				break;
			}
			tcpconn->s=fd;
			/* add tcpconn to the list*/
			tcpconn_add(tcpconn);
			/* update the timeout*/
			tcpconn->timeout=get_ticks()+tcp_con_lifetime;
			io_watch_add(&io_h, tcpconn->s, F_TCPCONN, tcpconn);
			tcpconn->flags&=~F_CONN_REMOVED;
			break;
		default:
			LM_CRIT("unknown cmd %d\n", cmd);
	}
end:
	return ret;
error:
	return -1;
}



/* generic handle io routine, it will call the appropiate
 *  handle_xxx() based on the fd_map type
 *
 * params:  fm  - pointer to a fd hash entry
 *          idx - index in the fd_array (or -1 if not known)
 * return: -1 on error
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 */
inline static int handle_io(struct fd_map* fm, int idx)
{	
	int ret;
	
	switch(fm->type){
		case F_SOCKINFO:
			ret=handle_new_connect((struct socket_info*)fm->data);
			break;
		case F_TCPCONN:
			ret=handle_tcpconn_ev((struct tcp_connection*)fm->data, idx);
			break;
		case F_TCPCHILD:
			ret=handle_tcp_child((struct tcp_child*)fm->data, idx);
			break;
		case F_PROC:
			ret=handle_ser_child((struct process_table*)fm->data, idx);
			break;
		case F_NONE:
			LM_CRIT("empty fd map\n");
			goto error;
		default:
			LM_CRIT("uknown fd type %d\n", fm->type); 
			goto error;
	}
	return ret;
error:
	return -1;
}



/* very inefficient for now - FIXME
 * keep in sync with tcpconn_destroy, the "delete" part should be
 * the same except for io_watch_del..*/
static inline void tcpconn_timeout(int force)
{
	struct tcp_connection *c, *next;
	unsigned int ticks;
	unsigned h;
	int fd;
	
	
	ticks=get_ticks();
	TCPCONN_LOCK; /* fixme: we can lock only on delete IMO */
	for(h=0; h<TCP_ID_HASH_SIZE; h++){
		c=tcpconn_id_hash[h];
		while(c){
			next=c->id_next;
			if (force ||((c->refcnt==0) && (ticks>c->timeout))) {
				if (!force)
					LM_DBG("timeout for hash=%d - %p"
							" (%d > %d)\n", h, c, ticks, c->timeout);
				fd=c->s;
#ifdef USE_TLS
				if (c->type==PROTO_TLS)
					tls_close(c, fd);
#endif
				_tcpconn_rm(c);
				if ((!force)&&(fd>0)&&(c->refcnt==0)) {
					if (!(c->flags & F_CONN_REMOVED)){
						io_watch_del(&io_h, fd, -1, IO_FD_CLOSING);
						c->flags|=F_CONN_REMOVED;
					}
					close(fd);
				}
				tcp_connections_no--;
			}
			c=next;
		}
	}
	TCPCONN_UNLOCK;
}



/* tcp main loop */
void tcp_main_loop(void)
{

	struct socket_info* si;
	int r;

	/* init io_wait (here because we want the memory allocated only in
	 * the tcp_main process) */

	/* FIXME: TODO: make tcp_max_fd_no a config param */
	if  (init_io_wait(&io_h, tcp_max_fd_no, tcp_poll_method)<0)
		goto error;
	/* init: start watching all the fds*/

	/* add all the sockets we listens on for connections */
	for (si=tcp_listen; si; si=si->next){
		if ((si->proto==PROTO_TCP) &&(si->socket!=-1)){
			if (io_watch_add(&io_h, si->socket, F_SOCKINFO, si)<0){
				LM_CRIT("failed to add listen socket to the fd list\n");
				goto error;
			}
		}else{
			LM_CRIT("non tcp address in tcp_listen\n");
		}
	}
#ifdef USE_TLS
	if (!tls_disable){
		for (si=tls_listen; si; si=si->next){
			if ((si->proto==PROTO_TLS) && (si->socket!=-1)){
				if (io_watch_add(&io_h, si->socket, F_SOCKINFO, si)<0){
					LM_CRIT("failed to add tls listen socket to the fd list\n");
					goto error;
				}
			}else{
				LM_CRIT("non tls address in tls_listen\n");
			}
		}
	}
#endif
	/* add all the unix sockets used for communcation with other openser 
	 * processes (get fd, new connection a.s.o) */
	for (r=1; r<counted_processes; r++){
		/* skip myslef (as process) and -1 socks (disabled) 
		   (we can't have 0, we never close it!) */
		if (r!=process_no && pt[r].unix_sock>0) 
			if (io_watch_add(&io_h, pt[r].unix_sock, F_PROC, &pt[r])<0){
					LM_CRIT("failed to add process %d (%s) unix socket "
						"to the fd list\n", r, pt[r].desc);
					goto error;
			}
	}
	/* add all the unix sokets used for communication with the tcp childs */
	for (r=0; r<tcp_children_no; r++){
		if (tcp_children[r].unix_sock>0)/*we can't have 0, we never close it!*/
			if (io_watch_add(&io_h, tcp_children[r].unix_sock, F_TCPCHILD,
							&tcp_children[r]) <0){
				LM_CRIT("failed to add tcp child %d unix socket to "
						"the fd list\n", r);
				goto error;
			}
	}
	
	/* main loop */
	switch(io_h.poll_method){
		case POLL_POLL:
			while(1){
				/* wait and process IO */
				io_wait_loop_poll(&io_h, TCP_MAIN_SELECT_TIMEOUT, 0); 
				/* remove old connections */
				tcpconn_timeout(0);
			}
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			while(1){
				io_wait_loop_select(&io_h, TCP_MAIN_SELECT_TIMEOUT, 0);
				tcpconn_timeout(0);
			}
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			while(1){
				io_wait_loop_sigio_rt(&io_h, TCP_MAIN_SELECT_TIMEOUT);
				tcpconn_timeout(0);
			}
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
			while(1){
				io_wait_loop_epoll(&io_h, TCP_MAIN_SELECT_TIMEOUT, 0);
				tcpconn_timeout(0);
			}
			break;
		case POLL_EPOLL_ET:
			while(1){
				io_wait_loop_epoll(&io_h, TCP_MAIN_SELECT_TIMEOUT, 1);
				tcpconn_timeout(0);
			}
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			while(1){
				io_wait_loop_kqueue(&io_h, TCP_MAIN_SELECT_TIMEOUT, 0);
				tcpconn_timeout(0);
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
			while(1){
				io_wait_loop_devpoll(&io_h, TCP_MAIN_SELECT_TIMEOUT, 0);
				tcpconn_timeout(0);
			}
			break;
#endif
		default:
			LM_CRIT("no support for poll method %s (%d)\n", 
					poll_method_name(io_h.poll_method), io_h.poll_method);
			goto error;
	}
error:
	destroy_io_wait(&io_h);
	LM_CRIT("exiting...");
	exit(-1);
}



/* cleanup before exit */
void destroy_tcp(void)
{
		if (tcpconn_id_hash){
			tcpconn_timeout(1); /* force close/expire for all active tcpconns*/
			shm_free(tcpconn_id_hash);
			tcpconn_id_hash=0;
		}
		if (connection_id){
			shm_free(connection_id);
			connection_id=0;
		}
		if (tcpconn_aliases_hash){
			shm_free(tcpconn_aliases_hash);
			tcpconn_aliases_hash=0;
		}
		if (tcpconn_lock){
			lock_destroy(tcpconn_lock);
			lock_dealloc((void*)tcpconn_lock);
			tcpconn_lock=0;
		}
}



int init_tcp(void)
{
	char* poll_err;
	
	/* init lock */
	tcpconn_lock=lock_alloc();
	if (tcpconn_lock==0){
		LM_CRIT("could not alloc lock\n");
		goto error;
	}
	if (lock_init(tcpconn_lock)==0){
		LM_CRIT("could not init lock\n");
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
		goto error;
	}
	/* init tcp children array */
	tcp_children = (struct tcp_child*)pkg_malloc
		( tcp_children_no*sizeof(struct tcp_child) );
	if (tcp_children==0) {
		LM_CRIT("could not alloc tcp_children array in pkg memory\n");
		goto error;
	}
	memset( tcp_children, 0, tcp_children_no*sizeof(struct tcp_child));
	/* init globals */
	connection_id=(int*)shm_malloc(sizeof(int));
	if (connection_id==0){
		LM_CRIT("could not alloc globals in shm memory\n");
		goto error;
	}
	*connection_id=1;
	/* alloc hashtables*/
	tcpconn_aliases_hash=(struct tcp_conn_alias**)
			shm_malloc(TCP_ALIAS_HASH_SIZE* sizeof(struct tcp_conn_alias*));
	if (tcpconn_aliases_hash==0){
		LM_CRIT("could not alloc address hashtable in shm memory\n");
		goto error;
	}
	tcpconn_id_hash=(struct tcp_connection**)shm_malloc(TCP_ID_HASH_SIZE*
								sizeof(struct tcp_connection*));
	if (tcpconn_id_hash==0){
		LM_CRIT("could not alloc id hashtable in shm memory\n");
		goto error;
	}
	/* init hashtables*/
	memset((void*)tcpconn_aliases_hash, 0, 
			TCP_ALIAS_HASH_SIZE * sizeof(struct tcp_conn_alias*));
	memset((void*)tcpconn_id_hash, 0, 
			TCP_ID_HASH_SIZE * sizeof(struct tcp_connection*));
	
	/* fix config variables */
	/* they can have only positive values due the config parser so we can
	 * ignore most of them */
		poll_err=check_poll_method(tcp_poll_method);
	
	/* set an appropiate poll method */
	if (poll_err || (tcp_poll_method==0)){
		tcp_poll_method=choose_poll_method();
		if (poll_err){
			LM_ERR("%s, using %s instead\n",
					poll_err, poll_method_name(tcp_poll_method));
		}else{
			LM_INFO("using %s as the TCP io watch method"
					" (auto detected)\n", poll_method_name(tcp_poll_method));
		}
	}else{
			LM_INFO("using %s as the TCP io watch method (config)\n",
					poll_method_name(tcp_poll_method));
	}
	
	return 0;
error:
	/* clean-up */
	destroy_tcp();
	return -1;
}



/* starts the tcp processes */
int tcp_init_children(int *chd_rank)
{
	int r;
	//int sockfd[2];
	int reader_fd[2]; /* for comm. with the tcp children read  */
	pid_t pid;
	struct socket_info *si;
	
	/* estimate max fd. no:
	 * 1 tcp send unix socket/all_proc, 
	 *  + 1 udp sock/udp proc + 1 tcp_child sock/tcp child*
	 *  + no_listen_tcp */
	for(r=0, si=tcp_listen; si; si=si->next, r++);
#ifdef USE_TLS
	if (! tls_disable)
		for (si=tls_listen; si; si=si->next, r++);
#endif
	
	tcp_max_fd_no=counted_processes*2 +r-1 /* timer */ +3; /* stdin/out/err*/
	tcp_max_fd_no+=tcp_max_connections;
	
	/* create the tcp sock_info structures */
	/* copy the sockets --moved to main_loop*/
	
	/* fork children & create the socket pairs*/
	for(r=0; r<tcp_children_no; r++){
		/*if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
			LM_ERR("socketpair failed: %s\n", strerror(errno));
			goto error;
		}*/
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, reader_fd)<0){
			LM_ERR("socketpair failed: %s\n", strerror(errno));
			goto error;
		}
		
		(*chd_rank)++;
		pid=openser_fork("SIP receiver TCP");
		if (pid<0){
			LM_ERR("fork failed\n");
			goto error;
		}else if (pid>0){
			/* parent */
			close(reader_fd[1]);
			tcp_children[r].pid=pid;
			tcp_children[r].proc_no=process_no;
			tcp_children[r].busy=0;
			tcp_children[r].n_reqs=0;
			tcp_children[r].unix_sock=reader_fd[0];
		}else{
			/* child */
			set_proc_attrs("TCP receiver");
			pt[process_no].idx=r;
			bind_address=0; /* force a SEGFAULT if someone uses a non-init.
							   bind address on tcp */
			if (init_child(*chd_rank) < 0) {
				LM_ERR("init_children failed\n");
				exit(-1);
			}
			tcp_receive_loop(reader_fd[1]);
			exit(-1);
		}
	}
	return 0;
error:
	return -1;
}

#endif
