/*
 * $Id: main.c 3590 2008-01-28 17:46:56Z bogdan_iancu $
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005-2006 Voice Sistem S.R.L.
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
 *  2002-01-29  argc/argv globalized via my_{argc|argv} (jiri)
 *  2003-01-23  mhomed added (jiri)
 *  2003-03-19  replaced all malloc/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-29  pkg cleaners for fifo and script callbacks introduced (jiri)
 *  2003-03-31  removed snmp part (obsolete & no place in core) (andrei)
 *  2003-04-06  child_init called in all processes (janakj)
 *  2003-04-08  init_mallocs split into init_{pkg,shm}_mallocs and 
 *               init_shm_mallocs called after cmd. line parsing (andrei)
 *  2003-04-15  added tcp_disable support (andrei)
 *  2003-05-09  closelog() before openlog to force opening a new fd 
 *              (needed on solaris) (andrei)
 *  2003-06-11  moved all signal handlers init. in install_sigs and moved it
 *              after daemonize (so that we won't catch anymore our own
 *              SIGCHLD generated when becoming session leader) (andrei)
 *              changed is_main default value to 1 (andrei)
 *  2003-06-28  kill_all_children is now used instead of kill(0, sig)
 *              see comment above it for explanations. (andrei)
 *  2003-06-29  replaced port_no_str snprintf w/ int2str (andrei)
 *  2003-10-10  added switch for config check (-c) (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2004-03-30  core dump is enabled by default
 *              added support for increasing the open files limit    (andrei)
 *  2004-04-28  sock_{user,group,uid,gid,mode} added
 *              user2uid() & user2gid() added  (andrei)
 *  2004-09-11  added timeout on children shutdown and final cleanup
 *               (if it takes more than 60s => something is definitely wrong
 *                => kill all or abort)  (andrei)
 *              force a shm_unlock before cleaning-up, in case we have a
 *               crashed childvwhich still holds the lock  (andrei)
 *  2004-12-02  removed -p, extended -l to support [proto:]address[:port],
 *               added parse_phostport, parse_proto (andrei)
 *  2005-06-16  always record the pid in pt[process_no].pid twice: once in the
 *               parent & once in the child to avoid a short window when one
 *               of them might use it "unset" (andrei)
 *  2005-12-22  added tos configurability (thanks to Andreas Granig)
 *  2006-04-26  2-stage TLS init: before and after config file parsing (klaus)
 */

/**
 * \file main.c
 * \brief Command line parsing, initializiation and server startup.
 *
 * Contains methods for parsing the command line, the initialization of
 * the execution environment (signals, config file parsing) and forking
 * the TCP, UDP, timer and fifo children.
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <time.h>

#include <sys/ioctl.h>
#include <net/if.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "config.h"
#include "dprint.h"
#include "daemonize.h"
#include "route.h"
#include "udp_server.h"
#include "globals.h"
#include "mem/mem.h"
#ifdef SHM_MEM
#include "mem/shm_mem.h"
#endif
#include "sr_module.h"
#include "timer.h"
#include "parser/msg_parser.h"
#include "ip_addr.h"
#include "resolve.h"
#include "parser/parse_hname2.h"
#include "parser/digest/digest_parser.h"
#include "name_alias.h"
#include "hash_func.h"
#include "pt.h"
#include "script_cb.h"
#include "blacklists.h"

#include "pt.h"
#include "ut.h"
#include "serialize.h"
#include "statistics.h"
#include "pvar.h"
#ifdef USE_TCP
#include "poll_types.h"
#include "tcp_init.h"
#ifdef USE_TLS
#include "tls/tls_init.h"
#endif
#endif

#ifdef USE_SCTP
#include "sctp_server.h"
#endif

#include "version.h"
#include "mi/mi_core.h"

static char id[]="@(#) $Id: main.c 3590 2008-01-28 17:46:56Z bogdan_iancu $";
static char* version=OPENSER_FULL_VERSION;
static char* flags=OPENSER_COMPILE_FLAGS;
char compiled[]= __TIME__ " " __DATE__ ;


static char help_msg[]= "\
Usage: " NAME " -l address [-l address ...] [options]\n\
Options:\n\
    -f file      Configuration file (default " CFG_FILE ")\n\
    -c           Check configuration file for errors\n\
    -C           Similar to '-c' but in addition checks the flags of exported\n\
                  functions from included route blocks\n\
    -l address   Listen on the specified address/interface (multiple -l\n\
                  mean listening on more addresses).  The address format is\n\
                  [proto:]addr[:port], where proto=udp|tcp and \n\
                  addr= host|ip_address|interface_name. E.g: -l locahost, \n\
                  -l udp:127.0.0.1:5080, -l eth0:5062 The default behavior\n\
                  is to listen on all the interfaces.\n\
    -n processes Number of child processes to fork per interface\n\
                  (default: 8)\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                  field to a via\n\
    -R           Same as `-r` but use reverse dns;\n\
                  (to use both use `-rR`)\n\
    -v           Turn on \"via:\" host checking when forwarding replies\n\
    -d           Debugging mode (multiple -d increase the level)\n\
    -D           Do not fork into daemon mode\n\
    -E           Log to stderr\n"
#ifdef USE_TCP
"    -T           Disable tcp\n\
    -N processes Number of tcp child processes (default: equal to `-n`)\n\
    -W method    poll method\n"
#endif
"    -V           Version number\n\
    -h           This help message\n\
    -b nr        Maximum receive buffer size which will not be exceeded by\n\
                  auto-probing procedure even if  OS allows\n\
    -m nr        Size of shared memory allocated in Megabytes\n\
    -w dir       Change the working directory to \"dir\" (default \"/\")\n\
    -t dir       Chroot to \"dir\"\n\
    -u uid       Change uid \n\
    -g gid       Change gid \n\
    -P file      Create a pid file\n\
    -G file      Create a pgid file\n"
;

/**
 * Print compile-time constants 
 */
void print_ct_constants(void)
{
#ifdef ADAPTIVE_WAIT
	printf("ADAPTIVE_WAIT_LOOPS=%d, ", ADAPTIVE_WAIT_LOOPS);
#endif
/*
#ifdef SHM_MEM
	printf("SHM_MEM_SIZE=%d, ", SHM_MEM_SIZE);
#endif
*/
	printf("MAX_RECV_BUFFER_SIZE %d, MAX_LISTEN %d,"
			" MAX_URI_SIZE %d, BUF_SIZE %d\n",
		MAX_RECV_BUFFER_SIZE, MAX_LISTEN, MAX_URI_SIZE, 
		BUF_SIZE );
#ifdef USE_TCP
	printf("poll method support: %s.\n", poll_support);
#endif
	printf("svnrevision: %s\n", 
#ifdef SVNREVISION
	SVNREVISION
#else
	"unknown"
#endif
	);
}


/* global vars */

int own_pgid = 0; /* whether or not we have our own pgid (and it's ok
					 to use kill(0, sig) */
char* cfg_file = 0;
unsigned int maxbuffer = MAX_RECV_BUFFER_SIZE; /* maximum buffer size we do
												  not want to exceed during the
												  auto-probing procedure; may 
												  be re-configured */
int children_no = 0;			/* number of children processing requests */
#ifdef USE_TCP
int tcp_children_no = 0;
int tcp_disable = 0; /* 1 if tcp is disabled */
#endif
#ifdef USE_TLS
int tls_disable = 1; /* 1 if tls is disabled */
#endif
#ifdef USE_SCTP
int sctp_disable = 0; /* 1 if sctp is disabled */
#endif
int sig_flag = 0;              /* last signal received */
#ifdef CHANGEABLE_DEBUG_LEVEL
int debug_init = L_NOTICE;
int *debug = &debug_init;
#else
int debug = L_NOTICE;
#endif
int dont_fork = 0;
/* start by logging to stderr */
int log_stderr = 1;
/* log facility (see syslog(3)) */
int log_facility = LOG_DAEMON;
/* the id to be printed in syslog */
char *log_name = 0;
int config_check = 0;
/* check if reply first via host==us */
int check_via =  0;        
/* debugging level for memory stats */
/* FIXME this uses still the old logging system */
int memlog = L_DBG;
/* should replies include extensive warnings? by default yes,
   good for trouble-shooting
*/
int sip_warning = 0;
/* should localy-generated messages include server's signature?
   be default yes, good for trouble-shooting
*/
int server_signature=1;
/* Server header to be used when proxy generates request as UAS.
   Default is to use SERVER_HDR CRLF (assigned later).
*/
str server_header = {SERVER_HDR,sizeof(SERVER_HDR)-1};
/* User-Agent header to be used when proxy generates request as UAC.
   Default is to use USER_AGENT CRLF (assigned later).
*/
str user_agent_header = {USER_AGENT,sizeof(USER_AGENT)-1};
/* should openser try to locate outbound interface on multihomed
 * host? by default not -- too expensive
 */
int mhomed=0;
/* use dns and/or rdns or to see if we need to add 
   a ;received=x.x.x.x to via: */
int received_dns = 0;      
char* working_dir = 0;
char* chroot_dir = 0;
char* user=0;
char* group=0;
int uid = 0;
int gid = 0;

/* more config stuff */
int disable_core_dump=0; /* by default enabled */
int open_files_limit=-1; /* don't touch it by default */
/* a hint to reply modules whether they should send reply
   to IP advertised in Via or IP from which a request came
*/
int reply_to_via=0;

#ifdef USE_MCAST
int mcast_loopback = 0;
int mcast_ttl = -1; /* if -1, don't touch it, use the default (usually 1) */
#endif /* USE_MCAST */

int tos = IPTOS_LOWDELAY;

struct socket_info* udp_listen=0;
#ifdef USE_TCP
struct socket_info* tcp_listen=0;
#endif
#ifdef USE_TLS
struct socket_info* tls_listen=0;
#endif
#ifdef USE_SCTP
struct socket_info* sctp_listen=0;
#endif

struct socket_info* bind_address=0; /* pointer to the crt. proc.
									 listening address*/
struct socket_info* sendipv4; /* ipv4 socket to use when msg. comes from ipv6*/
struct socket_info* sendipv6; /* same as above for ipv6 */
#ifdef USE_TCP
struct socket_info* sendipv4_tcp; 
struct socket_info* sendipv6_tcp; 
#endif
#ifdef USE_TLS
struct socket_info* sendipv4_tls;
struct socket_info* sendipv6_tls;
#endif
#ifdef USE_SCTP
struct socket_info* sendipv4_sctp;
struct socket_info* sendipv6_sctp;
#endif


/* if aliases should be automatically discovered and added 
 * during fixing listening sockets */
int auto_aliases=1;

unsigned short port_no=0; /* default port*/
#ifdef USE_TLS
unsigned short tls_port_no=0; /* default port */
#endif

struct host_alias* aliases=0; /* name aliases list */

/* process number - 0 is the main process */
int process_no = 0;

/* cfg parsing */
int cfg_errors=0;

/* shared memory (in MB) */
unsigned long shm_mem_size=SHM_MEM_SIZE * 1024 * 1024;

/* export command-line to anywhere else */
int my_argc;
char **my_argv;

#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse();


int is_main = 1; /* flag = is this the  "main" process? */

char* pid_file = 0; /* filename as asked by user */
char* pgid_file = 0;


/**
 * Clean up on exit. This should be called before exiting.
 * \param show_status set to one to display the mem status 
 */
void cleanup(int show_status)
{
	/*clean-up*/
	if (mem_lock) 
		shm_unlock(); /* hack: force-unlock the shared memory lock in case
					 some process crashed and let it locked; this will 
					 allow an almost gracious shutdown */
	destroy_modules();
#ifdef USE_TCP
	destroy_tcp();
#endif
#ifdef USE_TLS
	destroy_tls();
#endif
	destroy_timer();
	destroy_stats_collector();
	destroy_script_cb();
	pv_free_extra_list();
	destroy_black_lists();
#ifdef PKG_MALLOC
	if (show_status){
		LOG(memlog, "Memory status (pkg):\n");
		pkg_status();
	}
#endif
#ifdef SHM_MEM
	if (pt) shm_free(pt);
	pt=0;
	if (show_status){
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
	}
#ifdef CHANGEABLE_DEBUG_LEVEL
	if (debug!=&debug_init) {
		debug_init = *debug;
		shm_free(debug);
		debug = &debug_init;
	}
#endif
	/* zero all shmem alloc vars that we still use */
	shm_mem_destroy();
#endif
	if (pid_file) unlink(pid_file);
	if (pgid_file) unlink(pgid_file);
}

#define MAX_LINES 500
struct filechunk {
   str lines[MAX_LINES];
   int numlines;
   struct filechunk* next;
};
struct chunked_file_ {
   long size;
   char *data;
   struct filechunk* chunks;
};
struct chunked_file_ chunked_file;
static void chunk_file(struct chunked_file_* file)
{
   char* prev;
   long len=file->size;
   long pos=0;
   struct filechunk* chunk;

   file->chunks = malloc(sizeof(struct filechunk));
   memset(file->chunks,0,sizeof(struct filechunk));
   chunk=file->chunks;
   prev=file->data;
   while (pos<len) {
      if(file->data[pos]=='\n'){
	 chunk->lines[chunk->numlines].s=prev;
	 chunk->lines[chunk->numlines].len=(int)(((file->data+pos)-prev)-1);
	 chunk->numlines++;
	 if(chunk->numlines == MAX_LINES){
	    chunk->next = malloc(sizeof(struct filechunk));
	    memset(chunk->next,0,sizeof(struct filechunk));
	    chunk=chunk->next;
	 }
	 prev = file->data+pos+1;
      }
      pos++;
   }
}


/** 
 * Tries to send a signal to all our processes
 * If daemonized  is ok to send the signal to all the process group,
 * however if not daemonized we might end up sending the signal also
 * to the shell which launched us => most signals will kill it if 
 * it's not in interactive mode and we don't want this. The non-daemonized 
 * case can occur when an error is encountered before daemonize is called 
 * (e.g. when parsing the config file) or when openser is started in 
 * "dont-fork" mode.
 * \param signum signal for killing the children
 */
static void kill_all_children(int signum)
{
	int r;
	if (own_pgid) kill(0, signum);
	else if (pt)
		for (r=1; r<counted_processes; r++)
			if (pt[r].pid) kill(pt[r].pid, signum);
}



/**
 * Timeout handler during wait for children exit. 
 * If this handler is called, a critical timeout has occured while
 * waiting for the children to finish => we should kill everything and exit
 * \param signo signal for killing the children
 */
static void sig_alarm_kill(int signo)
{
	kill_all_children(SIGKILL); /* this will kill the whole group
								  including "this" process;
								  for debugging replace with SIGABRT
								  (but warning: it might generate lots
								   of cores) */
}


/**
 * Timeout handler during wait for children exit. 
 * like sig_alarm_kill, but the timeout has occured when cleaning up,
 * try to leave a core for future diagnostics
 * \param signo signal for killing the children
 * \see sig_alarm_kill
 */
static void sig_alarm_abort(int signo)
{
	/* LOG is not signal safe, but who cares, we are abort-ing anyway :-) */
	LM_CRIT("BUG - shutdown timeout triggered, dying...");
	abort();
}


#define OPENSER_SHUTDOWN_TIME	60
/**
 * Signal handler for the server.
 */
void handle_sigs(void)
{
	pid_t  chld;
	int    chld_status;
	int    i;
	int    do_exit;

	switch(sig_flag){
		case 0: break; /* do nothing*/
		case SIGPIPE:
				/* SIGPIPE might be rarely received on use of
				   exec module; simply ignore it
				 */
				LM_WARN("SIGPIPE received and ignored\n");
				break;
		case SIGINT:
		case SIGTERM:
			/* we end the program in all these cases */
			if (sig_flag==SIGINT)
				LM_DBG("INT received, program terminates\n");
			else
				LM_DBG("SIGTERM received, program terminates\n");
				
			/* first of all, kill the children also */
			kill_all_children(SIGTERM);
			if (signal(SIGALRM, sig_alarm_kill) == SIG_ERR ) {
				LM_ERR("could not install SIGALARM handler\n");
				/* continue, the process will die anyway if no
				 * alarm is installed which is exactly what we want */
			}
			alarm(OPENSER_SHUTDOWN_TIME); /* 1 minute close timeout */

			while(wait(0) > 0); /* Wait for all the children to terminate */
			signal(SIGALRM, sig_alarm_abort);

			cleanup(1); /* cleanup & show status*/
			alarm(0);
			signal(SIGALRM, SIG_IGN);
			dprint("Thank you for flying " NAME "\n");
			exit(0);
			break;
			
		case SIGUSR1:
#ifdef PKG_MALLOC
			LOG(memlog, "Memory status (pkg):\n");
			pkg_status();
#endif
#ifdef SHM_MEM
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
#endif
			break;
			
		case SIGCHLD:
			do_exit = 0;
			while ((chld=waitpid( -1, &chld_status, WNOHANG ))>0) {
				/* is it a process we know about? */
				for( i=0 ; i<counted_processes ; i++ )
					if (pt[i].pid==chld) break;
				if (i==counted_processes) {
					LM_DBG("unkown child process %d ended. Ignoring\n",chld);
					continue;
				}
				do_exit = 1;
				/* process the signal */
				if (WIFEXITED(chld_status)) 
					LM_INFO("child process %d exited normally,"
							" status=%d\n", chld, 
							WEXITSTATUS(chld_status));
				else if (WIFSIGNALED(chld_status)) {
					LM_INFO("child process %d exited by a signal"
							" %d\n", chld, WTERMSIG(chld_status));
#ifdef WCOREDUMP
					LM_INFO("core was %sgenerated\n",
							 WCOREDUMP(chld_status) ?  "" : "not " );
#endif
				}else if (WIFSTOPPED(chld_status)) 
					LM_INFO("child process %d stopped by a"
								" signal %d\n", chld,
								 WSTOPSIG(chld_status));
			}
			if (!do_exit)
				break;
			LM_INFO("terminating due to SIGCHLD\n");
			/* exit */
			kill_all_children(SIGTERM);
			if (signal(SIGALRM, sig_alarm_kill) == SIG_ERR ) {
				LM_ERR("could not install SIGALARM handler\n");
				/* continue, the process will die anyway if no
				 * alarm is installed which is exactly what we want */
			}
			alarm(OPENSER_SHUTDOWN_TIME); /* 1 minute close timeout */
			while(wait(0) > 0); /* wait for all the children to terminate*/
			signal(SIGALRM, sig_alarm_abort);
			cleanup(1); /* cleanup & show status*/
			alarm(0);
			signal(SIGALRM, SIG_IGN);
			LM_DBG("terminating due to SIGCHLD\n");
			exit(0);
			break;
		
		case SIGHUP: /* ignoring it*/
			LM_DBG("SIGHUP received, ignoring it\n");
			break;
		default:
			LM_CRIT("unhandled signal %d\n", sig_flag);
	}
	sig_flag=0;
}



/**
 * Exit regulary on a specific signal.
 * This is good for profiling which only works if exited regularly
 * and not by default signal handlers
 * \param signo The signal that should be handled
 */
static void sig_usr(int signo)
{
	if (is_main){
		if (sig_flag==0) sig_flag=signo;
		else /*  previous sig. not processed yet, ignoring? */
			return; ;
		if (dont_fork) 
				/* only one proc, doing everything from the sig handler,
				unsafe, but this is only for debugging mode*/
			handle_sigs();
	}else{
		/* process the important signals */
		switch(signo){
			case SIGPIPE:
					LM_INFO("signal %d received\n", signo);
				break;
			case SIGINT:
			case SIGTERM:
					LM_INFO("signal %d received\n", signo);
					/* print memory stats for non-main too */
					#ifdef PKG_MALLOC
					LOG(memlog, "Memory status (pkg):\n");
					pkg_status();
					#endif
					exit(0);
					break;
			case SIGUSR1:
					/* statistics -> show only pkg mem */
					#ifdef PKG_MALLOC
					LOG(memlog, "Memory status (pkg):\n");
					pkg_status();
					#endif
					break;
				/* ignored*/
			case SIGUSR2:
			case SIGHUP:
					break;
			case SIGCHLD:
					LM_DBG("SIGCHLD received: "
						"we do not worry about grand-children\n");
		}
	}
}



/**
 * Install the signal handlers.
 * \return 0 on success, -1 on error 
 */
int install_sigs(void)
{
	/* added by jku: add exit handler */
	if (signal(SIGINT, sig_usr) == SIG_ERR ) {
		LM_ERR("no SIGINT signal handler can be installed\n");
		goto error;
	}
	/* if we debug and write to a pipe, we want to exit nicely too */
	if (signal(SIGPIPE, sig_usr) == SIG_ERR ) {
		LM_ERR("no SIGINT signal handler can be installed\n");
		goto error;
	}
	
	if (signal(SIGUSR1, sig_usr)  == SIG_ERR ) {
		LM_ERR("no SIGUSR1 signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGCHLD , sig_usr)  == SIG_ERR ) {
		LM_ERR("no SIGCHLD signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGTERM , sig_usr)  == SIG_ERR ) {
		LM_ERR("no SIGTERM signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGHUP , sig_usr)  == SIG_ERR ) {
		LM_ERR("no SIGHUP signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGUSR2 , sig_usr)  == SIG_ERR ) {
		LM_ERR("no SIGUSR2 signal handler can be installed\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


/**
 * Main loop, forks the children, bind to addresses,
 * handle signals.
 * \return don't return on sucess, -1 on error
 */
static int main_loop(void)
{
	static int chd_rank;
	int  i;
	pid_t pid;
	struct socket_info* si;

	chd_rank=0;

	if (dont_fork){
		if (udp_listen==0){
			LM_ERR("no fork mode requires at least one"
					" udp listen address, exiting...\n");
			goto error;
		}
		/* only one address, we ignore all the others */
		if (udp_init(udp_listen)==-1) goto error;
		bind_address=udp_listen;
		sendipv4=bind_address;
		sendipv6=bind_address; /*FIXME*/
		if (udp_listen->next){
			LM_WARN("using only the first listen address (no fork)\n");
		}

		/* try to drop privileges */
		if (do_suid()==-1)
			goto error;

		/* we need another process to act as the timer*/
		if (start_timer_processes()!=0) {
			LM_CRIT("cannot start timer process(es)\n");
			goto error;
		}

		if (start_module_procs()!=0) {
			LM_ERR("failed to fork module processes\n");
			goto error;
		}

		/* main process, receive loop */
		set_proc_attrs("stand-alone SIP receiver %.*s", 
			 bind_address->sock_str.len, bind_address->sock_str.s );

		/* We will call child_init even if we
		 * do not fork - and it will be called with rank 1 because
		 * in fact we behave like a child, not like main process */
		if (init_child(1) < 0) {
			LM_ERR("init_child failed in don't fork\n");
			goto error;
		}

		is_main=1;
		return udp_rcv_loop();

	} else {  /* don't fork */

		for(si=udp_listen;si;si=si->next){
			/* create the listening socket (for each address)*/
			/* udp */
			if (udp_init(si)==-1) goto error;
			/* get first ipv4/ipv6 socket*/
			if ((si->address.af==AF_INET)&&
					((sendipv4==0)||(sendipv4->flags&SI_IS_LO)))
				sendipv4=si;
			#ifdef USE_IPV6
			if((sendipv6==0)&&(si->address.af==AF_INET6))
				sendipv6=si;
			#endif
		}
		#ifdef USE_TCP
		if (!tcp_disable){
			for(si=tcp_listen; si; si=si->next){
				/* same thing for tcp */
				if (tcp_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tcp==0)||(sendipv4_tcp->flags&SI_IS_LO)))
					sendipv4_tcp=si;
				#ifdef USE_IPV6
				if((sendipv6_tcp==0)&&(si->address.af==AF_INET6))
					sendipv6_tcp=si;
				#endif
			}
		}
		#ifdef USE_TLS
		if (!tls_disable){
			for(si=tls_listen; si; si=si->next){
				/* same as for tcp*/
				if (tls_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tls==0)||(sendipv4_tls->flags&SI_IS_LO)))
					sendipv4_tls=si;
				#ifdef USE_IPV6
				if((sendipv6_tls==0)&&(si->address.af==AF_INET6))
					sendipv6_tls=si;
				#endif
			}
		}
		#endif /* USE_TLS */
		#endif /* USE_TCP */
		#ifdef USE_SCTP
		if (!sctp_disable){
			for(si=sctp_listen; si; si=si->next){
				/* same thing for sctp */
				if (sctp_server_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_sctp==0)||(sendipv4_sctp->flags&SI_IS_LO)))
					sendipv4_sctp=si;
			#ifdef USE_IPV6
				if((sendipv6_sctp==0)&&(si->address.af==AF_INET6))
					sendipv6_sctp=si;
			#endif
			}
		}
		#endif /* USE_SCTP */

		/* all processes should have access to all the sockets (for sending)
		 * so we open all first*/
		if (do_suid()==-1) goto error; /* try to drop privileges */

		/* udp processes */
		for(si=udp_listen; si; si=si->next){
			for(i=0;i<children_no;i++){
				chd_rank++;
				if ( (pid=openser_fork( "UDP receiver"))<0 ) {
					LM_CRIT("cannot fork UDP process\n");
					goto error;
				} else if (pid==0){
					/* new UDP process */
					/* set a more detailed description */
					set_proc_attrs("SIP receiver %.*s ",
						si->sock_str.len, si->sock_str.s);
					bind_address=si; /* shortcut */
					if (init_child(chd_rank) < 0) {
						LM_ERR("init_child failed for UDP listener\n");
						exit(-1);
					}
					udp_rcv_loop();
					exit(-1);
				}
			}
			/*parent*/
			/*close(udp_sock)*/; /*if it's closed=>sendto invalid fd errors?*/
		}
	}

	#ifdef USE_SCTP
	if(!sctp_disable){
		for(si=sctp_listen; si; si=si->next){
			for(i=0;i<children_no;i++){
				chd_rank++;
				if ( (pid=openser_fork( "SCTP receiver"))<0 ) {
					LM_CRIT("cannot fork SCTP process\n");
					goto error;
				} else if (pid==0){
					/* new SCTP process */
					/* set a more detailed description */
					set_proc_attrs("SIP receiver %.*s ",
						si->sock_str.len, si->sock_str.s);
					bind_address=si; /* shortcut */
					if (init_child(chd_rank) < 0) {
						LM_ERR("init_child failed\n");
						exit(-1);
					}
					sctp_server_rcv_loop();
					exit(-1);
				}
			}
		}
	}
	#endif /* USE_SCTP */

	/* this is the main process -> it shouldn't send anything */
	bind_address=0;

	/* fork for the timer process*/
	if (start_timer_processes()!=0) {
		LM_CRIT("cannot start timer process(es)\n");
		goto error;
	}

	if (start_module_procs()!=0) {
		LM_ERR("failed to fork module processes\n");
		goto error;
	}

	#ifdef USE_TCP
	if (!tcp_disable){
		/* start tcp  & tls receivers */
		if (tcp_init_children(&chd_rank)<0) goto error;
		/* start tcp+tls master proc */
		if ( (pid=openser_fork( "TCP main"))<0 ) {
			LM_CRIT("cannot fork tcp main process\n");
			goto error;
		}else if (pid==0){
			/* child */
			/* close the TCP inter-process sockets */
			close(unix_tcp_sock);
			unix_tcp_sock = -1;
			close(pt[process_no].unix_sock);
			pt[process_no].unix_sock = -1;
			/* init modules */
			if (init_child(PROC_TCP_MAIN) < 0) {
				LM_ERR("error in init_child for tcp main\n");
				exit(-1);
			}
			tcp_main_loop();
			exit(-1);
		}
	}
	#endif

	/* main process left */
	is_main=1;
	set_proc_attrs("attendant");

	if (init_child(PROC_MAIN) < 0) {
		LM_ERR("error in init_child for PROC_MAIN\n");
		goto error;
	}

	for(;;){
			handle_sigs();
			pause();
	}

	/*return 0; */
error:
	is_main=1;  /* if we are here, we are the "main process",
				  any forked children should exit with exit(-1) and not
				  ever use return */
	return -1;

}



/**
 * Main routine, start of the program execution.
 * \param argc the number of arguments
 * \param argv pointer to the arguments array
 * \return don't return on sucess, -1 on error
 * \see main_loop
 */
int main(int argc, char** argv)
{
	/* configure by default logging to syslog */
	int cfg_log_stderr = 0;
	FILE* cfg_stream;
	int c,r;
	char *tmp;
	int tmp_len;
	int port;
	int proto;
	char *options;
	int ret;
	unsigned int seed;
	int rfd;

	/*init*/
	ret=-1;
	my_argc=argc; my_argv=argv;

	/*init pkg mallocs (before parsing cfg or cmd line !)*/
	if (init_pkg_mallocs()==-1)
		goto error;

	init_route_lists();
	/* process command line (get port no, cfg. file path etc) */
	opterr=0;
	options="f:cCm:b:l:n:N:rRvdDETSVhw:t:u:g:P:G:W:";

	while((c=getopt(argc,argv,options))!=-1){
		switch(c){
			case 'f':
					cfg_file=optarg;
					break;
			case 'C':
					config_check |= 2;
			case 'c':
					if (config_check==3)
						break;
					config_check |= 1;
					cfg_log_stderr=1; /* force stderr logging */
					break;
			case 'm':
					shm_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						LM_ERR("bad shmem size number: -m %s\n", optarg);
						goto error;
					};

					break;
			case 'b':
					maxbuffer=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						LM_ERR("bad max buffer size number: -b %s\n", optarg);
						goto error;
					}
					break;
			case 'l':
					if (parse_phostport(optarg, strlen(optarg), &tmp, &tmp_len,
											&port, &proto)<0){
						LM_ERR("bad -l address specifier: %s\n", optarg);
						goto error;
					}
					tmp[tmp_len]=0; /* null terminate the host */
					/* add a new addr. to our address list */
					if (add_listen_iface(tmp, port, proto, 0)!=0){
						LM_ERR("failed to add new listen address\n");
						goto error;
					}
					break;
			case 'n':
					children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						LM_ERR("bad process number: -n %s\n", optarg);
						goto error;
					}
					break;
			case 'v':
					check_via=1;
					break;
			case 'r':
					received_dns|=DO_DNS;
					break;
			case 'R':
					received_dns|=DO_REV_DNS;
			case 'd':
#ifdef CHANGEABLE_DEBUG_LEVEL
					(*debug)++;
#else
					debug++;
#endif
					break;
			case 'D':
					dont_fork=1;
					break;
			case 'E':
					cfg_log_stderr=1;
					break;
			case 'T':
#ifdef USE_TCP
					tcp_disable=1;
#else
					LM_WARN("tcp support not compiled in\n");
#endif
					break;
			case 'S':
#ifdef USE_SCTP
					sctp_disable=1;
#else
					LM_WARN("sctp support not compiled in\n");
#endif
			break;
			case 'N':
#ifdef USE_TCP
					tcp_children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						LM_ERR("bad process number: -N %s\n", optarg);
						goto error;
					}
#else
					LM_WARN("tcp support not compiled in\n");
#endif
					break;
			case 'W':
#ifdef USE_TCP
					tcp_poll_method=get_poll_type(optarg);
					if (tcp_poll_method==POLL_NONE){
						LM_ERR("bad poll method name: -W %s\ntry "
							"one of %s.\n", optarg, poll_support);
						goto error;
					}
#else
					LM_WARN("tcp support not compiled in\n");
#endif
					break;
			case 'V':
					printf("version: %s\n", version);
					printf("flags: %s\n", flags );
					print_ct_constants();
					printf("%s\n",id);
					printf("%s compiled on %s with %s\n", __FILE__,
							compiled, COMPILER );
					
					exit(0);
					break;
			case 'h':
					printf("version: %s\n", version);
					printf("%s",help_msg);
					exit(0);
					break;
			case 'w':
					working_dir=optarg;
					break;
			case 't':
					chroot_dir=optarg;
					break;
			case 'u':
					user=optarg;
					break;
			case 'g':
					group=optarg;
					break;
			case 'P':
					pid_file=optarg;
					break;
			case 'G':
					pgid_file=optarg;
					break;
			case '?':
					if (isprint(optopt))
						LM_ERR("Unknown option `-%c`.\n", optopt);
					else
						LM_ERR("Unknown option character `\\x%x`.\n", optopt);
					goto error;
			case ':':
					LM_ERR("Option `-%c` requires an argument.\n", optopt);
					goto error;
			default:
					abort();
		}
	}

	log_stderr = cfg_log_stderr;

	/* fill missing arguments with the default values*/
	if (cfg_file==0) cfg_file=CFG_FILE;

	/* load config file or die */
	cfg_stream=fopen (cfg_file, "r");
	if (cfg_stream==0){
		LM_ERR("loading config file(%s): %s\n", cfg_file,
				strerror(errno));
		goto error;
	}

	/* seed the prng, try to use /dev/urandom if possible */
	/* no debugging information is logged, because the standard
	   log level prior the config file parsing is L_NOTICE */
	seed=0;
	if ((rfd=open("/dev/urandom", O_RDONLY))!=-1){
try_again:
		if (read(rfd, (void*)&seed, sizeof(seed))==-1){
			if (errno==EINTR) goto try_again; /* interrupted by signal */
			LM_WARN("could not read from /dev/urandom (%d)\n", errno);
		}
		LM_DBG("initialize the pseudo random generator from "
			"/dev/urandom\n");
		LM_DBG("read %u from /dev/urandom\n", seed);
			close(rfd);
	}else{
		LM_WARN("could not open /dev/urandom (%d)\n", errno);
		LM_WARN("using a unsafe seed for the pseudo random number generator");
	}
	seed+=getpid()+time(0);
	LM_DBG("seeding PRNG with %u\n", seed);
	srand(seed);
	LM_DBG("test random number %u\n", rand());

	/*register builtin  modules*/
	register_builtin_modules();

#ifdef USE_TLS
	/* initialize default TLS domains,
	   must be done before reading the config */
	if (pre_init_tls()<0){
		LM_CRIT("could not pre_init_tls, exiting...\n");
		goto error;
	}
#endif /* USE_TLS */

	if (preinit_black_lists()!=0) {
		LM_CRIT("failed to alloc black list's anchor\n");
		goto error;
	}

	/* parse the config file, prior to this only default values
	   e.g. for debugging settings will be used */
	yyin=cfg_stream;
	if ((yyparse()!=0)||(cfg_errors)){
		LM_ERR("bad config file (%d errors)\n", cfg_errors);
		goto error;
	}

	if (config_check>1 && check_rls()!=0) {
		LM_ERR("bad function call in config file\n");
		return ret;
	}
#ifdef EXTRA_DEBUG
	print_rl();
#endif
#ifdef SUPER_DEBUG
	chunked_file.size=ftell(cfg_stream);
	chunked_file.data=malloc(chunked_file.size);
	rewind(cfg_stream);
	fread(chunked_file.data,1,chunked_file.size,cfg_stream);
	chunk_file(&chunked_file);
#endif


	/* init the resolver, before fixing the config */
	resolv_init();

	/* fix parameters */
	if (port_no<=0) port_no=SIP_PORT;
#ifdef USE_TLS
	if (tls_port_no<=0) tls_port_no=SIPS_PORT;
#endif
	
	
	if (children_no<=0) children_no=CHILD_NO;
#ifdef USE_TCP
	if (!tcp_disable){
		if (tcp_children_no<=0) tcp_children_no=children_no;
	}
#endif
	
	if (working_dir==0) working_dir="/";

	/* get uid/gid */
	if (user){
		if (user2uid(&uid, &gid, user)<0){
			LM_ERR("bad user name/uid number: -u %s\n", user);
			goto error;
		}
	}
	if (group){
		if (group2gid(&gid, group)<0){
			LM_ERR("bad group name/gid number: -u %s\n", group);
			goto error;
		}
	}
	if (fix_all_socket_lists()!=0){
		LM_ERR("failed to initialize list addresses\n");
		goto error;
	}
	/* print all the listen addresses */
	printf("Listening on \n");
	print_all_socket_lists();
	printf("Aliases: \n");
	/*print_aliases();*/
	print_aliases();
	printf("\n");
	
	if (dont_fork){
		LM_WARN("no fork mode %s\n", 
				(udp_listen)?(
				(udp_listen->next)?" and more than one listen address found"
				"(will use only the first one)":""
				):"and no udp listen address found" );
	}
	if (config_check){
		LM_NOTICE("config file ok, exiting...\n");
		return 0;
	}


	/*init shm mallocs
	 *  this must be here 
	 *     -to allow setting shm mem size from the command line
	 *       => if shm_mem should be settable from the cfg file move
	 *       everything after
	 *     -it must be also before init_timer and init_tcp
	 *     -it must be after we know uid (so that in the SYSV sems case,
	 *        the sems will have the correct euid)
	 * --andrei */
	if (init_shm_mallocs()==-1)
		goto error;
	/*init timer, before parsing the cfg!*/
	if (init_timer()<0){
		LM_CRIT("could not initialize timer, exiting...\n");
		goto error;
	}
	
#ifdef USE_TCP
	if (!tcp_disable){
		/*init tcp*/
		if (init_tcp()<0){
			LM_CRIT("could not initialize tcp, exiting...\n");
			goto error;
		}
	}
#ifdef USE_TLS
	if (!tls_disable){
		/* init tls*/
		if (init_tls()<0){
			LM_CRIT("could not initialize tls, exiting...\n");
			goto error;
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */

	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize((log_name==0)?argv[0]:log_name) <0 )
			goto error;
	}

	/* install signal handlers */
	if (install_sigs() != 0){
		LM_ERR("could not install the signal handlers\n");
		goto error;
	}

#ifdef CHANGEABLE_DEBUG_LEVEL
#ifdef SHM_MEM
	debug=shm_malloc(sizeof(int));
	if (debug==0) {
		LM_ERR("ERROR: out of memory\n");
		goto error;
	}
	*debug = debug_init;
#else
	LM_WARN("no shm mem support compiled -> changeable debug "
		"level turned off\n");
#endif
#endif

	if (disable_core_dump) set_core_dump(0, 0);
	else set_core_dump(1, shm_mem_size+PKG_MEM_POOL_SIZE+4*1024*1024);
	if (open_files_limit>0){
		if(increase_open_fds(open_files_limit)<0){ 
			LM_ERR("ERROR: error could not increase file limits\n");
			goto error;
		}
	}

	/* print OpenSER version to log for history tracking */
	LM_NOTICE("version: %s\n", version);
	
	/* print some data about the configuration */
#ifdef SHM_MEM
	LM_INFO("using %ld Mb shared memory\n", ((shm_mem_size/1024)/1024));
#endif
	LM_INFO("using %i Mb private memory per process\n", ((PKG_MEM_POOL_SIZE/1024)/1024));

	/* init serial forking engine */
	if (init_serialization()!=0) {
		LM_ERR("failed to initialize serialization\n");
		goto error;
	}
	/* Init statistics */
	if (init_stats_collector()<0) {
		LM_ERR("failed to initialize statistics\n");
		goto error;
	}
	/* Init MI */
	if (init_mi_core()<0) {
		LM_ERR("failed to initialize MI core\n");
		goto error;
	}
	/* init black list engine */
	if (init_black_lists()!=0) {
		LM_CRIT("failed to init black lists\n");
		goto error;
	}
	/* init resolver's blacklist */
	if (resolv_blacklist_init()!=0) {
		LM_CRIT("failed to create DNS blacklist\n");
		goto error;
	}

	/* init modules */
	if (init_modules() != 0) {
		LM_ERR("error while initializing modules\n");
		goto error;
	}

	/* init multi processes support */
	if (init_multi_proc_support()!=0) {
		LM_ERR("failed to init multi-proc support\n");
		goto error;
	}

	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		LM_ERR("failed to fix configuration with err code %d\n", r);
		goto error;
	};


	ret=main_loop();

error:
	/*kill everything*/
	kill_all_children(SIGTERM);
	/*clean-up*/
	cleanup(0);
	return ret;
}
