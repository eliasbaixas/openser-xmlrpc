/* $Id: resolve.c 3121 2007-11-13 15:42:39Z bogdan_iancu $
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005-2007 Voice Sistem S.R.L.
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
 *  2003-02-13  added proto to sip_resolvehost, for SRV lookups (andrei)
 *  2003-07-03  default port value set according to proto (andrei)
 *  2007-01-25  support for DNS failover added (bogdan)
 */ 


#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "resolve.h"
#include "dprint.h"
#include "ut.h"
#include "ip_addr.h"
#include "globals.h"
#include "blacklists.h"


/* stuff related to DNS failover */
#define DNS_NODE_SRV   1
#define DNS_NODE_A     2

struct dns_val {
	unsigned int ival;
	char *sval;
};

/* mallocs for local stuff */
#define local_malloc pkg_malloc
#define local_free   pkg_free

int dns_try_ipv6=0; /* default off */
/* declared in globals.h */
int dns_retr_time=-1;
int dns_retr_no=-1;
int dns_servers_no=-1;
int dns_search_list=-1;
int disable_dns_blacklist=1;

static struct bl_head *failover_bl=0;
#define DNS_REVOLVER_BL_ID    17
#define DNS_REVOLVER_BL_NAME  "dns"
#define DNS_BL_EXPIRE         4*60

/* init. the resolver
 * params: retr_time  - time before retransmitting (must be >0)
 *         retr_no    - retransmissions number
 *         servers_no - how many dns servers will be used
 *                      (from the one listed in /etc/resolv.conf)
 *         search     - if 0 the search list in /etc/resolv.conf will
 *                      be ignored (HINT: even if you don't have a
 *                      search list in resolv.conf, it's still better
 *                      to set search to 0, because an empty seachlist
 *                      means in fact search "" => it takes more time)
 * If any of the parameters <0, the default (system specific) value
 * will be used. See also resolv.conf(5).
 * returns: 0 on success, -1 on error
 */
int resolv_init(void)
{
	res_init();
#ifdef HAVE_RESOLV_RES
	if (dns_retr_time>0)
		_res.retrans=dns_retr_time;
	if (dns_retr_no>0)
		_res.retry=dns_retr_no;
	if (dns_servers_no>=0)
		_res.nscount=dns_servers_no;
	if (dns_search_list==0)
		_res.options&=~(RES_DEFNAMES|RES_DNSRCH);
#else
#warning "no resolv timeout support"
	LM_WARN("no resolv options support - resolv options will be ignored\n");
#endif
	return 0;
}



int resolv_blacklist_init(void)
{
	str name = str_init(DNS_REVOLVER_BL_NAME);

	if (!disable_dns_blacklist) {
		failover_bl = create_bl_head( DNS_REVOLVER_BL_ID,
			BL_DO_EXPIRE|BL_BY_DEFAULT, 0, 0, &name);
		if (failover_bl==NULL) {
			LM_ERR("failed to create blacklist\n");
			return -1;
		}
	}
	return 0;
}


 /*  skips over a domain name in a dns message
 *  (it can be  a sequence of labels ending in \0, a pointer or
 *   a sequence of labels ending in a pointer -- see rfc1035
 *   returns pointer after the domain name or null on error*/
unsigned char* dns_skipname(unsigned char* p, unsigned char* end)
{
	while(p<end){
		/* check if \0 (root label length) */
		if (*p==0){
			p+=1;
			break;
		}
		/* check if we found a pointer */
		if (((*p)&0xc0)==0xc0){
			/* if pointer skip over it (2 bytes) & we found the end */
			p+=2;
			break;
		}
		/* normal label */
		p+=*p+1;
	}
	return (p>=end)?0:p;
}



/* parses the srv record into a srv_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   rdata - pointer  to the rdata part of the srv answer
 * returns 0 on error, or a dyn. alloc'ed srv_rdata structure */
/* SRV rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |     priority   |
 * |----------------|
 * |     weight     |
 * |----------------|
 * |   port number  |
 * |----------------|
 * |                |
 * ~      name      ~
 * |                |
 * +----------------+
 */
struct srv_rdata* dns_srv_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata)
{
	struct srv_rdata* srv;
	int len;
	
	srv=0;
	if ((rdata+6)>=end) goto error;
	srv=(struct srv_rdata*)local_malloc(sizeof(struct srv_rdata));
	if (srv==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	
	memcpy((void*)&srv->priority, rdata, 2);
	memcpy((void*)&srv->weight,   rdata+2, 2);
	memcpy((void*)&srv->port,     rdata+4, 2);
	rdata+=6;
	srv->priority=ntohs(srv->priority);
	srv->weight=ntohs(srv->weight);
	srv->port=ntohs(srv->port);
	if ((len=dn_expand(msg, end, rdata, srv->name, MAX_DNS_NAME-1))==-1)
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return srv;
error:
	if (srv) local_free(srv);
	return 0;
}


/* parses the naptr record into a naptr_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   rdata - pointer  to the rdata part of the naptr answer
 * returns 0 on error, or a dyn. alloc'ed naptr_rdata structure */
/* NAPTR rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |      order     |
 * |----------------|
 * |   preference   |
 * |----------------|
 * ~     flags      ~
 * |   (string)     |
 * |----------------|
 * ~    services    ~
 * |   (string)     |
 * |----------------|
 * ~    regexp      ~
 * |   (string)     |
 * |----------------|
 * ~  replacement   ~
   |    (name)      |
 * +----------------+
 */
struct naptr_rdata* dns_naptr_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata)
{
	struct naptr_rdata* naptr;
	
	naptr = 0;
	if ((rdata + 7) >= end) goto error;
	naptr=(struct naptr_rdata*)local_malloc(sizeof(struct naptr_rdata));
	if (naptr == 0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	
	memcpy((void*)&naptr->order, rdata, 2);
	naptr->order=ntohs(naptr->order);
	memcpy((void*)&naptr->pref, rdata + 2, 2);
	naptr->pref=ntohs(naptr->pref);
	naptr->flags_len = (int)rdata[4];
	if ((rdata + 7 +  naptr->flags_len) >= end) goto error;
	memcpy((void*)&naptr->flags, rdata + 5, naptr->flags_len);
	naptr->services_len = (int)rdata[5 + naptr->flags_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len) >= end) goto error;
	memcpy((void*)&naptr->services, rdata + 6 + naptr->flags_len, naptr->services_len);
	naptr->regexp_len = (int)rdata[6 + naptr->flags_len + naptr->services_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len +
					naptr->regexp_len) >= end) goto error;
	memcpy((void*)&naptr->regexp, rdata + 7 + naptr->flags_len +
				naptr->services_len, naptr->regexp_len);
	rdata = rdata + 7 + naptr->flags_len + naptr->services_len + 
			naptr->regexp_len;
	naptr->repl_len=dn_expand(msg, end, rdata, naptr->repl, MAX_DNS_NAME-1);
	if ( naptr->repl_len==(unsigned int)-1 )
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return naptr;
error:
	if (naptr) local_free(naptr);
	return 0;
}



/* parses a CNAME record into a cname_rdata structure */
struct cname_rdata* dns_cname_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct cname_rdata* cname;
	int len;
	
	cname=0;
	cname=(struct cname_rdata*)local_malloc(sizeof(struct cname_rdata));
	if(cname==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	if ((len=dn_expand(msg, end, rdata, cname->name, MAX_DNS_NAME-1))==-1)
		goto error;
	return cname;
error:
	if (cname) local_free(cname);
	return 0;
}



/* parses an A record rdata into an a_rdata structure
 * returns 0 on error or a dyn. alloc'ed a_rdata struct
 */
struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* end)
{
	struct a_rdata* a;
	
	if (rdata+4>=end) goto error;
	a=(struct a_rdata*)local_malloc(sizeof(struct a_rdata));
	if (a==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	memcpy(a->ip, rdata, 4);
	return a;
error:
	return 0;
}



/* parses an AAAA (ipv6) record rdata into an aaaa_rdata structure
 * returns 0 on error or a dyn. alloc'ed aaaa_rdata struct */
struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* end)
{
	struct aaaa_rdata* aaaa;
	
	if (rdata+16>=end) goto error;
	aaaa=(struct aaaa_rdata*)local_malloc(sizeof(struct aaaa_rdata));
	if (aaaa==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	memcpy(aaaa->ip6, rdata, 16);
	return aaaa;
error:
	return 0;
}

/* RFC1035:
 *
 * <character-string> is a single length octet followed by that number of characters.
 * TXT-DATA        One or more <character-string>s.
 *
 * We only take the first string here.
 */
/* parses a TXT record into a txt_rdata structure */
struct txt_rdata* dns_txt_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct txt_rdata* txt;
	unsigned int len;
	
	txt=0;
	txt=(struct txt_rdata*)local_malloc(sizeof(struct txt_rdata));
	if(txt==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	len = *rdata;
	if (rdata + 1 + len >= end) goto error;	/*  something fishy in the record */
	if (len >= sizeof(txt->txt)) goto error; /* not enough space? */
	memcpy(txt->txt, rdata+1, len);
	txt->txt[len] = 0;		/* 0-terminate string */
	return txt;

error:
	if (txt) local_free(txt);
	return 0;
}


/* EBL Record
 *
 *    0  1  2  3  4  5  6  7
 *    +--+--+--+--+--+--+--+--+
 *    |       POSITION        |
 *    +--+--+--+--+--+--+--+--+
 *    /       SEPARATOR       /
 *    +--+--+--+--+--+--+--+--+
 *    /         APEX          /
 *    +--+--+--+--+--+--+--+--+
 */
/* parses a EBL record into a ebl_rdata structure */
struct ebl_rdata* dns_ebl_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct ebl_rdata* ebl;
	int len;
	
	ebl=0;
	ebl=(struct ebl_rdata*)local_malloc(sizeof(struct ebl_rdata));
	if(ebl==0){
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	len = *rdata;
	if (rdata + 1 + len >= end) goto error;	/*  something fishy in the record */

	ebl->position = *rdata;
	if ( ebl->position > 15 ) goto error; /* doesn't make sense: E.164 numbers can't be longer */

	rdata++;

	ebl->separator_len = (int) *rdata;
	rdata++;
	if ((rdata + 1 +  ebl->separator_len) >= end) goto error;
	memcpy((void*)&ebl->separator, rdata, ebl->separator_len);
	rdata += ebl->separator_len;

	ebl->apex_len=dn_expand(msg, end, rdata, ebl->apex, MAX_DNS_NAME-1);
	if ( ebl->apex_len==(unsigned int)-1 )
		goto error;
	ebl->apex[ebl->apex_len] = 0; /* 0-terminate string */
	return ebl;

error:
	if (ebl) local_free(ebl);
	return 0;
}




/* frees completely a struct rdata list */
void free_rdata_list(struct rdata* head)
{
	struct rdata* l;
	struct rdata* next_l;

	for( l=head; l ; l=next_l) {
		next_l = l->next;
		/* free the parsed rdata*/
		if (l->rdata) local_free(l->rdata);
		local_free(l);
	}
}



/* gets the DNS records for name:type
 * returns a dyn. alloc'ed struct rdata linked list with the parsed responses
 * or 0 on error
 * see rfc1035 for the query/response format */
struct rdata* get_record(char* name, int type)
{
	int size;
	int qno, answers_no;
	int r;
	int ans_len;
	static union dns_query buff;
	unsigned char* p;
	unsigned char* t;
	unsigned char* end;
	static unsigned char answer[ANS_SIZE];
	unsigned short rtype, class, rdlength;
	unsigned int ttl;
	struct rdata* head;
	struct rdata** crt;
	struct rdata** last;
	struct rdata* rd;
	struct srv_rdata* srv_rd;
	struct srv_rdata* crt_srv;
	
	size=res_search(name, C_IN, type, buff.buff, sizeof(buff));
	if (size<0) {
		LM_DBG("lookup(%s, %d) failed\n", name, type);
		goto not_found;
	}
	else if ((unsigned int)size > sizeof(buff)) size=sizeof(buff);
	head=rd=0;
	last=crt=&head;
	
	p=buff.buff+DNS_HDR_SIZE;
	end=buff.buff+size;
	if (p>=end) goto error_boundary;
	qno=ntohs((unsigned short)buff.hdr.qdcount);

	for (r=0; r<qno; r++){
		/* skip the name of the question */
		if ((p=dns_skipname(p, end))==0) {
			LM_ERR("skipname==0\n");
			goto error;
		}
		p+=2+2; /* skip QCODE & QCLASS */
	#if 0
		for (;(p<end && (*p)); p++);
		p+=1+2+2; /* skip the ending  '\0, QCODE and QCLASS */
	#endif
		if (p>=end) {
			LM_ERR("p>=end\n");
			goto error;
		}
	};
	answers_no=ntohs((unsigned short)buff.hdr.ancount);
	ans_len=ANS_SIZE;
	t=answer;
	for (r=0; (r<answers_no) && (p<end); r++){
		/*  ignore it the default domain name */
		if ((p=dns_skipname(p, end))==0) {
			LM_ERR("skip_name=0 (#2)\n");
			goto error;
		}
		/*
		skip=dn_expand(buff.buff, end, p, t, ans_len);
		p+=skip;
		*/
		/* check if enough space is left for type, class, ttl & size */
		if ((p+2+2+4+2)>=end) goto error_boundary;
		/* get type */
		memcpy((void*) &rtype, (void*)p, 2);
		rtype=ntohs(rtype);
		p+=2;
		/* get  class */
		memcpy((void*) &class, (void*)p, 2);
		class=ntohs(class);
		p+=2;
		/* get ttl*/
		memcpy((void*) &ttl, (void*)p, 4);
		ttl=ntohl(ttl);
		p+=4;
		/* get size */
		memcpy((void*)&rdlength, (void*)p, 2);
		rdlength=ntohs(rdlength);
		p+=2;
		/* check for type */
		/*
		if (rtype!=type){
			LM_WAR("wrong type in answer (%d!=%d)\n", rtype, type);
			p+=rdlength;
			continue;
		}
		*/
		/* expand the "type" record  (rdata)*/
		
		rd=(struct rdata*) local_malloc(sizeof(struct rdata));
		if (rd==0){
			LM_ERR("out of pkg memory\n");
			goto error;
		}
		rd->type=rtype;
		rd->class=class;
		rd->ttl=ttl;
		rd->next=0;
		switch(rtype){
			case T_SRV:
				srv_rd= dns_srv_parser(buff.buff, end, p);
				rd->rdata=(void*)srv_rd;
				if (srv_rd==0) goto error_parse;
				
				/* insert sorted into the list */
				for (crt=&head; *crt; crt= &((*crt)->next)){
					crt_srv=(struct srv_rdata*)(*crt)->rdata;
					if ((srv_rd->priority <  crt_srv->priority) ||
					   ( (srv_rd->priority == crt_srv->priority) && 
							 (srv_rd->weight > crt_srv->weight) ) ){
						/* insert here */
						goto skip;
					}
				}
				last=&(rd->next); /*end of for => this will be the last elem*/
			skip:
				/* insert here */
				rd->next=*crt;
				*crt=rd;
				
				break;
			case T_A:
				rd->rdata=(void*) dns_a_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*last=rd; /* last points to the last "next" or the list head*/
				last=&(rd->next);
				break;
			case T_AAAA:
				rd->rdata=(void*) dns_aaaa_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_CNAME:
				rd->rdata=(void*) dns_cname_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_NAPTR:
				rd->rdata=(void*) dns_naptr_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_TXT:
				rd->rdata=(void*) dns_txt_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_EBL:
				rd->rdata=(void*) dns_ebl_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			default:
				LM_ERR("unknown type %d\n", rtype);
				rd->rdata=0;
				*last=rd;
				last=&(rd->next);
		}
		
		p+=rdlength;
		
	}
	return head;
error_boundary:
		LM_ERR("end of query buff reached\n");
		if(head)
			free_rdata_list(head);
		return 0;
error_parse:
		LM_ERR("rdata parse error \n");
		if (rd) local_free(rd); /* rd->rdata=0 & rd is not linked yet into
								   the list */
error:
		LM_ERR("get_record \n");
		if (head) free_rdata_list(head);
not_found:
	return 0;
}



static inline int get_naptr_proto(struct naptr_rdata *n)
{
#ifdef USE_TLS
	if (n->services[3]=='s' || n->services[3]=='S' )
		return PROTO_TLS;
#endif
	switch (n->services[n->services_len-1]) {
		case 'U':
		case 'u':
			return PROTO_UDP;
			break;
#ifdef USE_TCP
		case 'T':
		case 't':
			return PROTO_TCP;
			break;
#endif
#ifdef USE_SCTP
		case 'S':
		case 's':
			return PROTO_SCTP;
			break;
#endif

	}
	LM_CRIT("failed to detect proto\n");
	return PROTO_NONE;
}



static inline int srv2dns_node(struct rdata *head, struct dns_node **dn)
{
	unsigned int mem;
	unsigned int l;
	struct rdata *r;
	struct dns_node *n;
	char *p;

	/* calculate how much mem is required */
	mem = sizeof(struct dns_node);
	for( r=head,l=0 ; r ; r=r->next,l++ )
		mem +=sizeof(struct dns_val) + get_naptr(r)->repl_len + 1;

	n = (struct dns_node*)shm_malloc(mem);
	if (n==NULL) {
		LM_ERR("no more shm mem (%d)\n", mem);
		return -1;
	}

	n->type = DNS_NODE_SRV;
	n->size = mem;
	n->idx = 0;
	n->no = l;
	n->kids = *dn;
	*dn = n;

	n->vals = (struct dns_val*)(n+1);
	p = (char*)(n->vals+l);
	for( r=head,l=0 ; r ; r=r->next,l++ ) {
		n->vals[l].ival = get_naptr_proto( get_naptr(r) );
		n->vals[l].sval = p;
		memcpy( p, get_naptr(r)->repl, get_naptr(r)->repl_len );
		p += get_naptr(r)->repl_len;
		*(p++) = 0;
	}
	return 0;
}


static inline int a2dns_node(struct rdata *head, struct dns_node **dn)
{
	unsigned int mem;
	unsigned int l;
	struct rdata *r;
	struct dns_node *n;
	char *p;

	/* calculate how much mem is required */
	mem = sizeof(struct dns_node);
	for( r=head,l=0 ; r ; r=r->next,l++ ) {
		get_srv(r)->name_len = strlen(get_srv(r)->name);
		mem +=sizeof(struct dns_val) + get_srv(r)->name_len + 1;
		}

	n = (struct dns_node*)shm_malloc(mem);
	if (n==NULL) {
		LM_ERR("no more shm mem (%d)\n", mem);
		return -1;
	}

	n->type = DNS_NODE_A;
	n->size = mem;
	n->idx = 0;
	n->no = l;
	n->kids = 0;
	*dn = n;

	n->vals = (struct dns_val*)(n+1);
	p = (char*)(n->vals+l);
	for( r=head,l=0 ; r ; r=r->next,l++ ) {
		n->vals[l].ival = get_srv(r)->port;
		n->vals[l].sval = p;
		memcpy( p, get_srv(r)->name, get_srv(r)->name_len );
		LM_DBG("storing %.*s:%d\n", get_srv(r)->name_len,p,n->vals[l].ival);
		p += get_srv(r)->name_len;
		*(p++) = 0;
	}

	return 0;
}


static inline struct hostent* do_srv_lookup(char *name,
							unsigned short* port, struct dns_node **dn)
{
	struct hostent *he;
	struct srv_rdata *srv;
	struct rdata *head;
	struct rdata *rd;

	/* perform SRV lookup */
	head = get_record( name, T_SRV);
	for( rd=head; rd ; rd=rd->next ) {
		if (rd->type!=T_SRV)
			continue; /*should never happen*/
		srv = (struct srv_rdata*) rd->rdata;
		if (srv==0) {
			LM_CRIT("null rdata\n");
			free_rdata_list(head);
			return 0;
		}
		he = resolvehost(srv->name, 1);
		if ( he!=0 ) {
			LM_DBG("SRV(%s) = %s:%d\n",	name, srv->name, srv->port);
			*port=srv->port;
			if (dn && rd->next && a2dns_node( rd->next, dn)==-1)
					*dn = 0;
			free_rdata_list(head);
			return he;
		}
	}
	if (head)
		free_rdata_list(head);
	return 0;
}


#define naptr_prio(_naptr) \
	((unsigned int)((((_naptr)->order) << 16) + ((_naptr)->pref)))

static inline void filter_and_sort_naptr( struct rdata** head_p,
									struct rdata** filtered_p, int is_sips)
{
	struct naptr_rdata *naptr;
	struct rdata *head;
	struct rdata *last;
	struct rdata *out;
	struct rdata *l, *ln, *it, *itp;
	unsigned int prio;
	char p;

	head = 0;
	last = 0;
	out = 0;

	for( l=*head_p ; l ; l=ln ) {
		ln = l->next;

		if (l->type != T_NAPTR)
			goto skip0; /*should never happen*/

		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LM_CRIT("null rdata\n");
			goto skip0;
		}

		/* first filter out by flag and service */
		if (naptr->flags_len!=1||(naptr->flags[0]!='s'&&naptr->flags[0]!='S'))
			goto skip;
		if (naptr->repl_len==0 || naptr->regexp_len!=0 )
			goto skip;
		if ( (is_sips || naptr->services_len!=7 ||
			strncasecmp(naptr->services,"sip+d2",6) ) &&
		(
#ifdef USE_TLS
		tls_disable ||
#endif
		naptr->services_len!=8 || strncasecmp(naptr->services,"sips+d2",7)))
			goto skip;
		p = naptr->services[naptr->services_len-1];
		/* by default we do not support SCTP */
		if ( p!='U' && p!='u'
#ifdef USE_TCP
		&& (tcp_disable || (p!='T' && p!='t'))
#endif
#ifdef USE_SCTP
		&& (sctp_disable || (p!='S' && p!='s'))
#endif
		)
			goto skip;
		/* is it valid? (SIPS+D2U is not!) */
		if ( naptr->services_len==8 && (p=='U' || p=='u'))
			goto skip;

		LM_DBG("found valid %.*s -> %s\n",
			(int)naptr->services_len,naptr->services, naptr->repl);

		/* this is a supported service -> add it according to order to the 
		 * new head list */
		prio = naptr_prio(get_naptr(l));
		if (head==0) {
			head = last = l;
			l->next = 0;
		} else if ( naptr_prio(get_naptr(head)) >= prio ) {
			l->next = head;
			head = l;
		} else if ( prio >= naptr_prio(get_naptr(last)) ) {
			l->next = 0;
			last->next = l;
			last = l;
		} else {
			for( itp=head,it=head->next ; it && it->next ; itp=it,it=it->next ){
				if ( prio <= naptr_prio(get_naptr(it)))
					break;
			}
			l->next = itp->next;
			itp->next = l;
		}

		continue;
skip:
		LM_DBG("skipping %.*s -> %s\n",
			(int)naptr->services_len, naptr->services, naptr->repl);
skip0:
		l->next = out;
		out = l;
	}

	*head_p = head;
	*filtered_p = out;
}

#if 0
struct hostent* sip_resolvehost(str* name, unsigned short* port, int *proto,
																int is_sips)
{
	static char tmp[MAX_DNS_NAME];
	struct ip_addr *ip;
	struct rdata *head;
	struct rdata *rd;
	struct hostent* he;

	if ( (is_sips)
#ifdef USE_TLS
	&& (tls_disable)
#endif
	) {
		LM_ERR("cannot resolve SIPS as no TLS support is configured\n");
		return 0;
	}

	/* check if it's an ip address */
	if ( ((ip=str2ip(name))!=0)
#ifdef USE_IPV6
	|| ((ip=str2ip6(name))!=0)
#endif
	){
		/* we are lucky, this is an ip address */
		if (proto && *proto==PROTO_NONE)
			*proto = (is_sips)?PROTO_TLS:PROTO_UDP;
		if (port && *port==0)
			*port = (is_sips||((*proto)==PROTO_TLS))?SIPS_PORT:SIP_PORT;
		return ip_addr2he(name,ip);
	}

	/* do we have a port? */
	if ( !port || (*port)!=0 ) {
		/* have port -> no NAPTR, no SRV lookup, just A record lookup */
		LM_DBG("has port -> do A record lookup!\n");
		/* set default PROTO if not set */
		if (proto && *proto==PROTO_NONE)
			*proto = (is_sips)?PROTO_TLS:PROTO_UDP;
		goto do_a;
	}

	/* no port... what about proto? */
	if ( !proto || (*proto)!=PROTO_NONE ) {
		/* have proto, but no port -> do SRV lookup */
		LM_DBG("no port, has proto -> do SRV lookup!\n");
		if (is_sips && (*proto)!=PROTO_TLS) {
			LM_ERR("forced proto %d not matching sips uri\n", *proto);
			return 0;
		}
		goto do_srv;
	}

	LM_DBG("no port, no proto -> do NAPTR lookup!\n");
	/* no proto, no port -> do NAPTR lookup */
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return 0;
	}
	memcpy(tmp, name->s, name->len);
	tmp[name->len] = '\0';
	/* do NAPTR lookup */
	head = get_record( tmp, T_NAPTR);
	if (head) {
		/* filter and sort the records */
		filter_and_sort_naptr( &head, &rd, is_sips);
		/* free what is useless */
		free_rdata_list( rd );
		/* process the NAPTR records */
		for( rd=head ; rd ; rd=rd->next ) {
			he = do_srv_lookup( get_naptr(rd)->repl, port );
			if ( he ) {
				*proto = get_naptr_proto( get_naptr(rd) );
				LM_DBG("found!\n");
				free_rdata_list(head);
				return he;
			}
		}
		if (head)
			free_rdata_list(head);
	}
	LM_DBG("no valid NAPTR record found for %.*s," 
		" trying direct SRV lookup...\n", name->len, name->s);
	*proto = (is_sips)?PROTO_TLS:PROTO_UDP;

do_srv:
	if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME) {
		LM_WARN("domain name too long (%d),"
			" unable to perform SRV lookup\n", name->len);
		/* set defaults */
		*port = (is_sips)?SIPS_PORT:SIP_PORT;
		goto do_a;
	}

	switch (*proto) {
		case PROTO_UDP:
			memcpy(tmp, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
			memcpy(tmp+SRV_UDP_PREFIX_LEN, name->s, name->len);
			tmp[SRV_UDP_PREFIX_LEN + name->len] = '\0';
			break;
#ifdef USE_TCP
		case PROTO_TCP:
			if (tcp_disable) goto err_proto;
			memcpy(tmp, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
			memcpy(tmp+SRV_TCP_PREFIX_LEN, name->s, name->len);
			tmp[SRV_TCP_PREFIX_LEN + name->len] = '\0';
			break;
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			if (tls_disable) goto err_proto;
			memcpy(tmp, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
			memcpy(tmp+SRV_TLS_PREFIX_LEN, name->s, name->len);
			tmp[SRV_TLS_PREFIX_LEN + name->len] = '\0';
			break;
#endif
		default:
			goto err_proto;
	}

	he = do_srv_lookup( tmp, port );
	if (he)
		return he;
	
	LM_DBG("no valid SRV record found for %s," 
		" trying A record lookup...\n", tmp);
	/* set default port */
	*port = (is_sips||((*proto)==PROTO_TLS))?SIPS_PORT:SIP_PORT;

do_a:
	/* do A record lookup */
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return 0;
	}
	memcpy(tmp, name->s, name->len);
	tmp[name->len] = '\0';
	he = resolvehost(tmp,1);
	return he;
err_proto:
	LM_ERR("unsupported proto %d\n", *proto);
	return 0;
}
#endif



struct hostent* sip_resolvehost( str* name, unsigned short* port,
		unsigned short *proto, int is_sips, struct dns_node **dn)
{
	static char tmp[MAX_DNS_NAME];
	struct ip_addr *ip;
	struct rdata *head;
	struct rdata *rd;
	struct hostent* he;

	if ( (is_sips)
#ifdef USE_TLS
	&& (tls_disable)
#endif
	) {
		LM_ERR("cannot resolve SIPS as no TLS support is configured\n");
		return 0;
	}

	if (dn)
		*dn = 0;

	/* check if it's an ip address */
	if ( ((ip=str2ip(name))!=0)
#ifdef USE_IPV6
	|| ((ip=str2ip6(name))!=0)
#endif
	){
		/* we are lucky, this is an ip address */
		if (proto && *proto==PROTO_NONE)
			*proto = (is_sips)?PROTO_TLS:PROTO_UDP;
		if (port && *port==0)
			*port = (is_sips||((*proto)==PROTO_TLS))?SIPS_PORT:SIP_PORT;
		return ip_addr2he(name,ip);
	}

	/* do we have a port? */
	if ( !port || (*port)!=0 ) {
		/* have port -> no NAPTR, no SRV lookup, just A record lookup */
		LM_DBG("has port -> do A record lookup!\n");
		/* set default PROTO if not set */
		if (proto && *proto==PROTO_NONE)
			*proto = (is_sips)?PROTO_TLS:PROTO_UDP;
		goto do_a;
	}

	/* no port... what about proto? */
	if ( !proto || (*proto)!=PROTO_NONE ) {
		/* have proto, but no port -> do SRV lookup */
		LM_DBG("no port, has proto -> do SRV lookup!\n");
		if (is_sips && (*proto)!=PROTO_TLS) {
			LM_ERR("forced proto %d not matching sips uri\n", *proto);
			return 0;
		}
		goto do_srv;
	}

	LM_DBG("no port, no proto -> do NAPTR lookup!\n");
	/* no proto, no port -> do NAPTR lookup */
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return 0;
	}
	memcpy(tmp, name->s, name->len);
	tmp[name->len] = '\0';
	/* do NAPTR lookup */
	head = get_record( tmp, T_NAPTR);
	if (head) {
		/* filter and sort the records */
		filter_and_sort_naptr( &head, &rd, is_sips);
		/* free what is useless */
		free_rdata_list( rd );
		/* process the NAPTR records */
		for( rd=head ; rd ; rd=rd->next ) {
			*proto = get_naptr_proto( get_naptr(rd) );
			he = do_srv_lookup( get_naptr(rd)->repl, port, dn);
			if ( he ) {
				LM_DBG("valid SRV found!\n");
				if (dn) {
					/* save the state of the resolver for failure cases */
					if (*dn==NULL)
						rd = rd->next;
					if (rd && srv2dns_node( rd, dn)!=0) {
						shm_free(*dn);
						*dn = 0;
					}
				}
				free_rdata_list(head);
				return he;
			}
		}
		if (head)
			free_rdata_list(head);
	}
	LM_DBG("no valid NAPTR record found for %.*s," 
		" trying direct SRV lookup...\n", name->len, name->s);
	*proto = (is_sips)?PROTO_TLS:PROTO_UDP;

do_srv:
	if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME) {
		LM_WARN("domain name too long (%d),"
			" unable to perform SRV lookup\n", name->len);
		/* set defaults */
		*port = (is_sips)?SIPS_PORT:SIP_PORT;
		goto do_a;
	}

	switch (*proto) {
		case PROTO_UDP:
			memcpy(tmp, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
			memcpy(tmp+SRV_UDP_PREFIX_LEN, name->s, name->len);
			tmp[SRV_UDP_PREFIX_LEN + name->len] = '\0';
			break;
#ifdef USE_TCP
		case PROTO_TCP:
			if (tcp_disable) goto err_proto;
			memcpy(tmp, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
			memcpy(tmp+SRV_TCP_PREFIX_LEN, name->s, name->len);
			tmp[SRV_TCP_PREFIX_LEN + name->len] = '\0';
			break;
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			if (tls_disable) goto err_proto;
			memcpy(tmp, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
			memcpy(tmp+SRV_TLS_PREFIX_LEN, name->s, name->len);
			tmp[SRV_TLS_PREFIX_LEN + name->len] = '\0';
			break;
#endif
#ifdef USE_SCTP
		case PROTO_SCTP:
			if (sctp_disable) goto err_proto;
			memcpy(tmp, SRV_SCTP_PREFIX, SRV_SCTP_PREFIX_LEN);
			memcpy(tmp+SRV_SCTP_PREFIX_LEN, name->s, name->len);
			tmp[SRV_SCTP_PREFIX_LEN + name->len] = '\0';
			break;
#endif
		default:
			goto err_proto;
	}

	he = do_srv_lookup( tmp, port, dn);
	if (he)
		return he;
	
	LM_DBG("no valid SRV record found for %s,trying A record lookup...\n",
		tmp);
	/* set default port */
	*port = (is_sips||((*proto)==PROTO_TLS))?SIPS_PORT:SIP_PORT;

do_a:
	/* do A record lookup */
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return 0;
	}
	memcpy(tmp, name->s, name->len);
	tmp[name->len] = '\0';
	he = resolvehost(tmp,1);
	return he;
err_proto:
	LM_ERR("unsupported proto %d\n", *proto);
	return 0;
}



static inline struct hostent* get_next_he(struct dns_node **node,
							unsigned short *proto, unsigned short *port)
{
	struct hostent  *he;
	struct dns_node *n;
	struct dns_node *last_srv;
	struct dns_node *dn;

	if (node==NULL || *node==NULL)
		return 0;

	n = *node;
	last_srv = NULL;
	he = 0;

	do {
		switch (n->type) {
			case DNS_NODE_SRV:
				last_srv = n;
				if (n->kids==NULL) {
					/* need to resolve this SRV and get all the AAA records */
					do {
						dn = 0;
						he = do_srv_lookup( n->vals[n->idx].sval, port, &dn);
						if (he) {
							*proto = n->vals[n->idx].ival;
							break;
						}
						n->idx++;
					} while(n->idx<n->no);
					if (he==NULL || (he && n->idx+1==n->no) ) {
						/* colapse the SRV node */
						shm_free(n);
						*node = dn;
						return he;
					}
					n->kids = dn;
					return he;
				}
				/* go for the AAA records */
				n = n->kids;
				break;
			case DNS_NODE_A:
				/* do resolve until success */
				do {
					he = resolvehost(n->vals[n->idx].sval,1);
					if (he) {
						*port = n->vals[n->idx].ival;
						break;
					}
					n->idx++;
				}while(n->idx<n->no);
				/* found something? */
				if (he==NULL || (he && n->idx+1==n->no)) {
					shm_free(n);
					/* any SRV level? */
					if (last_srv==NULL) {
						/* nothing left */
						*node = 0;
						return he;
					}
					last_srv->kids = 0;
					/* increase the index on the SRV level */
					if (++last_srv->idx<last_srv->no)
						return he;
					/* colapse the SRV node also */
					shm_free(last_srv);
					*node = 0;
				}
				return he;
				break;
			default:
				LM_CRIT("unknown %d node type\n", n->type);
				return 0;
		}
	} while(1);
}



void free_dns_res( struct proxy_l *p )
{
	if (p==NULL || p->dn==NULL)
		return;

	if (p->dn->kids)
		shm_free(p->dn->kids);
	shm_free(p->dn);
	p->dn = 0;
}



int get_next_su(struct proxy_l *p, union sockaddr_union* su, int add_to_bl)
{
	struct hostent *he;
	struct bl_rule *list;
	struct net  ip_net;
	int n;

	if (failover_bl && add_to_bl) {
		memset( &ip_net, 0xff , sizeof(struct net));
		hostent2ip_addr( &ip_net.ip, &p->host, p->addr_idx);
		ip_net.mask.af = ip_net.ip.af;
		ip_net.mask.len = ip_net.ip.len;
		list = 0;
		n = add_rule_to_list( &list, &list, &ip_net, 0, p->port, p->proto, 0);
		if (n!=0) {
			LM_ERR("failed to build bl rule\n");
		} else {
			add_list_to_head( failover_bl, list, list, 0, DNS_BL_EXPIRE);
		}
	}

	/* any more available IPs in he ? */
	if ( p->host.h_addr_list[++p->addr_idx] ) {
		/* yes -> return the IP*/
		hostent2su( su, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);
		return 0;
	}

	/* get a new he from DNS */
	he = get_next_he( &p->dn, &p->proto, &p->port);
	if (he==NULL)
		return -1;

	/* replace the current he */
	if (p->flags&PROXY_SHM_FLAG) {
		free_shm_hostent( &p->host );
		n = hostent_shm_cpy(&(p->host), he);
	} else {
		free_hostent( &p->host );
		n = hostent_cpy(&(p->host), he);
	}
	if (n!=0) {
		free_dns_res( p );
		return -1;
	}

	hostent2su( su, &p->host, 0, (p->port)?p->port:SIP_PORT);
	p->addr_idx = 0;
	return 0;
}



static inline struct dns_node *dns_node_copy(struct dns_node *s)
{
	struct dns_node *d;
	unsigned int i;

	d = (struct dns_node*)shm_malloc(s->size);
	if (d==NULL) {
		LM_ERR("no more shm mem\n");
		return 0;
	}
	memcpy( d, s, s->size);
	d->vals = (struct dns_val*)((char*)d + ((char*)s->vals-(char*)s));
	for( i=0 ; i<s->no ; i++ )
		d->vals[i].sval = (char*)d + ((char*)s->vals[i].sval-(char*)s);
	return d;
}


struct dns_node *dns_res_copy(struct dns_node *s)
{
	struct dns_node *d;

	d = dns_node_copy(s);
	if (d==NULL)
		return 0;
	if (s->kids) {
		d->kids = dns_node_copy(s->kids);
		if (d->kids==NULL) {
			shm_free(d);
			return 0;
		}
	}
	return d;
}


