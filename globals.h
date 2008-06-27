/*
 * $Id: globals.h 2759 2007-09-13 18:24:31Z miconda $
 *
 * global variables
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
 */



#ifndef globals_h
#define globals_h

#include "types.h"
#include "ip_addr.h"
#include "str.h"
#include "poll_types.h"

#define NO_DNS     0
#define DO_DNS     1
#define DO_REV_DNS 2



extern char * cfg_file;
extern int config_check;
extern char *stat_file;
extern unsigned short port_no;

extern int uid;
extern int gid;
extern char* pid_file;
extern char* pgid_file;
extern int own_pgid; /* whether or not we have our own pgid (and it's ok
>--->--->--->--->--->--->--->--->--->--->--- to use kill(0, sig) */

extern struct socket_info* bind_address; /* pointer to the crt. proc.
											listening address */
extern struct socket_info* sendipv4; /* ipv4 socket to use when msg.
										comes from ipv6*/
extern struct socket_info* sendipv6; /* same as above for ipv6 */
#ifdef USE_TCP
extern struct socket_info* sendipv4_tcp; /* ipv4 socket to use when msg.
										comes from ipv6*/
extern struct socket_info* sendipv6_tcp; /* same as above for ipv6 */
extern int unix_tcp_sock; /* socket used for communication with tcp main*/
#endif
#ifdef USE_TLS
extern struct socket_info* sendipv4_tls; /* ipv4 socket to use when msg.
										comes from ipv6*/
extern struct socket_info* sendipv6_tls; /* same as above for ipv6 */
#endif
#ifdef USE_SCTP
extern struct socket_info* sendipv4_sctp; /* ipv4 socket to use when msg.
                                             comes from ipv6*/
extern struct socket_info* sendipv6_sctp; /* same as above for ipv6 */
#endif

extern int auto_aliases;

extern unsigned int maxbuffer;
extern int children_no;
#ifdef USE_TCP
extern int tcp_children_no;
extern int tcp_disable;
extern int tcp_accept_aliases;
extern int tcp_connect_timeout;
extern int tcp_send_timeout;
extern int tcp_con_lifetime; /* connection lifetime */
extern enum poll_types tcp_poll_method;
extern int tcp_max_fd_no;
extern int tcp_max_connections;
#endif
#ifdef USE_TLS
extern int tls_disable;
extern unsigned short tls_port_no;
#endif
#ifdef USE_SCTP
extern int sctp_disable;
#endif
extern int dont_fork;
extern int check_via;
extern int received_dns;
/* extern int process_no; */
extern int sip_warning;
extern int server_signature;
extern str server_header;
extern str user_agent_header;
extern char* user;
extern char* group;
extern char* sock_user;
extern char* sock_group;
extern int sock_uid;
extern int sock_gid;
extern int sock_mode;
extern char* chroot_dir;
extern char* working_dir;

#ifdef USE_MCAST
extern int mcast_loopback;
extern int mcast_ttl;
#endif /* USE_MCAST */

extern int tos;

extern int disable_dns_failover;
extern int disable_dns_blacklist;

extern int cfg_errors;

extern unsigned long shm_mem_size;

/* AVP configuration */
extern char *avp_db_url;  /* db url used by user preferences (AVPs) */

extern int reply_to_via;

extern int is_main;

/* debugging level for dumping memory status */
extern int memlog;

/* looking up outbound interface ? */
extern int mhomed;

/* command-line arguments */
extern int my_argc;
extern char **my_argv;

/* pre-set addresses */
extern str default_global_address;
/* pre-ser ports */
extern str default_global_port;

/* core dump and file limits */
extern int disable_core_dump;
extern int open_files_limit;

/* resolver */
extern int dns_retr_time;
extern int dns_retr_no;
extern int dns_servers_no;
extern int dns_search_list;

extern int max_while_loops;

#endif
