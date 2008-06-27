
#include <string.h>             /* memcpy */
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "xmlrpc_ul_mod.h"
#include "xmlrpc_urecord.h"
#include "xmlrpc_ucontact.h"


#include "sha1.h"

/*
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact, ucontact_info_t* _ci)
{
   ucontact_t *c;

   c = (ucontact_t*)shm_malloc(sizeof(ucontact_t));
   if (!c) {
      LM_ERR("no more shm memory\n");
      return 0;
   }
   memset(c, 0, sizeof(ucontact_t));

   if (shm_str_dup( &c->c, _contact) < 0) goto error;
   if (shm_str_dup( &c->callid, _ci->callid) < 0) goto error;
   if (shm_str_dup( &c->user_agent, _ci->user_agent) < 0) goto error;

   if (_ci->received.s && _ci->received.len) {
      if (shm_str_dup( &c->received, &_ci->received) < 0) goto error;
   }
   if (_ci->path && _ci->path->len) {
      if (shm_str_dup( &c->path, _ci->path) < 0) goto error;
   }

   c->domain = _dom;
   c->aor = _aor;
   c->expires = _ci->expires;
   c->q = _ci->q;
   c->sock = _ci->sock;
   c->cseq = _ci->cseq;
   c->state = CS_NEW;
   c->flags = _ci->flags;
   c->cflags = _ci->cflags;
   c->methods = _ci->methods;
   c->last_modified = _ci->last_modified;

   return c;
error:
   LM_ERR("no more shm memory\n");
   if (c->path.s) shm_free(c->path.s);
   if (c->received.s) shm_free(c->received.s);
   if (c->user_agent.s) shm_free(c->user_agent.s);
   if (c->callid.s) shm_free(c->callid.s);
   if (c->c.s) shm_free(c->c.s);
   shm_free(c);
   return 0;
}

/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c)
{
   if (!_c) return;
   if (_c->path.s) shm_free(_c->path.s);
   if (_c->received.s) shm_free(_c->received.s);
   if (_c->user_agent.s) shm_free(_c->user_agent.s);
   if (_c->callid.s) shm_free(_c->callid.s);
   if (_c->c.s) shm_free(_c->c.s);
   shm_free( _c );
}

/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c)
{
   time_t t = time(0);
   char* st;

   switch(_c->state) {
      case CS_NEW:   st = "CS_NEW";     break;
      case CS_SYNC:  st = "CS_SYNC";    break;
      case CS_DIRTY: st = "CS_DIRTY";   break;
      default:       st = "CS_UNKNOWN"; break;
   }

   fprintf(_f, "~~~Contact(%p)~~~\n", _c);
   fprintf(_f, "domain    : '%.*s'\n", _c->domain->len, ZSW(_c->domain->s));
   fprintf(_f, "aor       : '%.*s'\n", _c->aor->len, ZSW(_c->aor->s));
   fprintf(_f, "Contact   : '%.*s'\n", _c->c.len, ZSW(_c->c.s));
   fprintf(_f, "Expires   : ");
   if (_c->expires == 0) {
      fprintf(_f, "Permanent\n");
   } else if (_c->expires == UL_EXPIRED_TIME) {
      fprintf(_f, "Deleted\n");
   } else if (t > _c->expires) {
      fprintf(_f, "Expired\n");
   } else {
      fprintf(_f, "%u\n", (unsigned int)(_c->expires - t));
   }
   fprintf(_f, "q         : %s\n", q2str(_c->q, 0));
   fprintf(_f, "Call-ID   : '%.*s'\n", _c->callid.len, ZSW(_c->callid.s));
   fprintf(_f, "CSeq      : %d\n", _c->cseq);
   fprintf(_f, "User-Agent: '%.*s'\n",
	 _c->user_agent.len, ZSW(_c->user_agent.s));
   fprintf(_f, "received  : '%.*s'\n",
	 _c->received.len, ZSW(_c->received.s));
   fprintf(_f, "Path      : '%.*s'\n",
	 _c->path.len, ZSW(_c->path.s));
   fprintf(_f, "State     : %s\n", st);
   fprintf(_f, "Flags     : %u\n", _c->flags);
   if (_c->sock) {
      fprintf(_f, "Sock      : %.*s:%d (%p)\n",
	    _c->sock->address_str.len,_c->sock->address_str.s,
	    _c->sock->port_no,_c->sock);
   } else {
      fprintf(_f, "Sock      : none (null)\n");
   }
   fprintf(_f, "Methods   : %u\n", _c->methods);
   fprintf(_f, "next      : %p\n", _c->next);
   fprintf(_f, "prev      : %p\n", _c->prev);
   fprintf(_f, "~~~/Contact~~~~\n");
}

/*
 * Update ucontact structure in memory
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t* _ci)
{
#define update_str(_old,_new) \
   do{\
      if ((_old)->len < (_new)->len) { \
	 ptr = (char*)shm_malloc((_new)->len); \
	 if (ptr == 0) { \
	    LM_ERR("no more shm memory\n"); \
	    return -1; \
	 }\
	 memcpy(ptr, (_new)->s, (_new)->len);\
	 if ((_old)->s) shm_free((_old)->s);\
	 (_old)->s = ptr;\
      } else {\
	 memcpy((_old)->s, (_new)->s, (_new)->len);\
      }\
      (_old)->len = (_new)->len;\
   } while(0)

   char* ptr;

   /* No need to update Callid as it is constant 
    * per ucontact (set at insert time)  -bogdan */

   update_str( &_c->user_agent, _ci->user_agent);

   if (_ci->received.s && _ci->received.len) {
      update_str( &_c->received, &_ci->received);
   } else {
      if (_c->received.s) shm_free(_c->received.s);
      _c->received.s = 0;
      _c->received.len = 0;
   }

   if (_ci->path) {
      update_str( &_c->path, _ci->path);
   } else {
      if (_c->path.s) shm_free(_c->path.s);
      _c->path.s = 0;
      _c->path.len = 0;
   }

   _c->sock = _ci->sock;
   _c->expires = _ci->expires;
   _c->q = _ci->q;
   _c->cseq = _ci->cseq;
   _c->methods = _ci->methods;
   _c->last_modified = _ci->last_modified;
   _c->flags = _ci->flags;
   _c->cflags = _ci->cflags;

   return 0;
}

/* ============== Database related functions ================ */

/*
 * Insert contact into the database
 * The format for records in the overlay is:
 * 1-contact uri
 * 2-expires
 * 3-q
 * 4-cseq
 * 5-call-id
 * 6-flags
 * 7-cflags
 * 8-user_agent
 * 9-received_socket
 * 10-path
 * 11-socket
 * 12-method
 * 13-last_modified
 * return 1 on success
 * -1 on "not enough capacity" error
 * -2 on "try again" error
 */
int xmlrpc_insert_ucontact(ucontact_t* _c)
{
#define MAX_BUFS 40
   char* dom;
   char expires[MAX_BUFS];
   char q[MAX_BUFS];
   char cseq[MAX_BUFS];
   char flags[MAX_BUFS];
   char cflags[MAX_BUFS];
   char methods[MAX_BUFS];
   char lastmod[MAX_BUFS];
   char* where;
   int expiresl,ql,cseql,flagsl,cflagsl,methodsl,lastmodl;
   int len;
   enum xmlrpc_result xmlrpc_res;
   xmlrpc_value *result;

   len = 14 + /* 13 fields with one LF each + 1 extra LF*/
      _c->c.len + /*contact */
      (expiresl=snprintf(expires,MAX_BUFS,"%d",_c->expires)) +  /*expires*/
      (ql=snprintf(q,MAX_BUFS,"%d",_c->q))+ /* q value*/
      (cseql=snprintf(cseq,MAX_BUFS,"%d",_c->cseq)) +
      _c->callid.len + 
      (flagsl=snprintf(flags,MAX_BUFS,"%d",_c->flags))+
      (cflagsl=snprintf(cflags,MAX_BUFS,"%d",_c->cflags)) +
      _c->user_agent.len + 
      (_c->received.s==0?0:_c->received.len) +
      (_c->path.s==0?0:_c->path.len) +
      (_c->sock?_c->sock->sock_str.len:0) +
      (methodsl=snprintf(methods,MAX_BUFS,"%d",_c->methods)) +
      (lastmodl=snprintf(lastmod,MAX_BUFS,"%d",_c->last_modified)) +
      RECORD_END_LEN;
   where=pkg_malloc(len+1);
   len=snprintf(where,len+1,"%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n%.*s\n",
	 _c->c.len,_c->c.s,
	 expiresl,expires,
	 ql,q,
	 cseql,cseq,
	 _c->callid.len,_c->callid.s,
	 flagsl,flags,
	 cflagsl,cflags,
	 _c->user_agent.len,_c->user_agent.s,
	 (_c->received.s==0?0:_c->received.len),_c->received.s,
	 (_c->path.s==0?0:_c->path.len),_c->path.s,
	 ((_c->sock)?(_c->sock->sock_str.len):0),((_c->sock)?(_c->sock->sock_str.s):""),
	 methodsl,methods,
	 lastmodl,lastmod,
         RECORD_END_LEN,RECORD_END);
   LM_DBG("Putting \"%.*s\"\n",_c->aor->len,_c->aor->s);
   result = xmlrpc_client_call(&env,server_url,"put_removable","(66s6is)",
	 _c->aor->s,_c->aor->len,where,len,"SHA",secret_sha1_bin,20,_c->expires,"openser_xmlrpc_usrloc");
   LM_DBG("Put \"%.*s\"\n",_c->aor->len,_c->aor->s);
   die_if_fault_occurred(&env);
   xmlrpc_read_int(&env,result,&xmlrpc_res);
   die_if_fault_occurred(&env);

   switch(xmlrpc_res){
      case Success:
	 LM_DBG("Successfully inserted \"%.*s\"\n",_c->aor->len,_c->aor->s);
	 return 1;
      case Capacity:
	 LM_ERR("Not enough capacity in overlay!\n");
	 return -1;
      case Again:
	 LM_ERR("Overlay temporarily out of service, try again !\n");
	 return -2;
   }
}

/*
   TODO
 * Update contact in the database
 */
int xmlrpc_update_ucontact(ucontact_t* _c)
{
   LM_DBG("Updating ucontact %.*s\n",_c->c.len,_c->c.s);
   xmlrpc_delete_ucontact(_c);
   return xmlrpc_insert_ucontact(_c);
}

/*
   TODO
 * Delete contact from the database
 */
int xmlrpc_delete_ucontact(ucontact_t* _c)
{
   xmlrpc_value *result;
   int res;
   result = xmlrpc_client_call(&env,server_url,"rm","(66s6i)",_c->aor->s,_c->aor->len,_c->path.s,20,"SHA",secret,strlen(secret),_c->expires,"openser_xmlrpc_usrloc");
   xmlrpc_read_int(&env,result,&res);
   switch(res){
      case Success:
	 LM_DBG("Successfully deleted contact for \"%.*s\"\n",_c->aor->len,_c->aor->s);
	 return 1;
      case Capacity:
	 LM_ERR("Not enough capacity in overlay!\n");
	 return -1;
      case Again:
	 LM_ERR("Overlay temporarily out of service, try again !\n");
	 return -2;
   }

   return 0;
}

static inline void unlink_contact(struct urecord* _r, ucontact_t* _c)
{
   if (_c->prev) {
      _c->prev->next = _c->next;
      if (_c->next) {
	 _c->next->prev = _c->prev;
      }
   } else {
      _r->contacts = _c->next;
      if (_c->next) {
	 _c->next->prev = 0;
      }
   }
}

static inline void update_contact_pos(struct urecord* _r, ucontact_t* _c)
{
	ucontact_t *pos, *ppos;

	if (desc_time_order) {
		/* order by time - first the newest */
		if (_c->prev==0)
			return;
		unlink_contact(_r, _c);
		/* insert it at the beginning */
		_c->next = _r->contacts;
		_c->prev = 0;
		_r->contacts->prev = _c;
		_r->contacts = _c;
	} else {
		/* order by q - first the smaller q */
		if ( (_c->prev==0 || _c->q<=_c->prev->q)
		&& (_c->next==0 || _c->q>=_c->next->q)  )
			return;
		/* need to move , but where? */
		unlink_contact(_r, _c);
		_c->next = _c->prev = 0;
		for(pos=_r->contacts,ppos=0;pos&&pos->q<_c->q;ppos=pos,pos=pos->next);
		if (pos) {
			if (!pos->prev) {
				pos->prev = _c;
				_c->next = pos;
				_r->contacts = _c;
			} else {
				_c->next = pos;
				_c->prev = pos->prev;
				pos->prev->next = _c;
				pos->prev = _c;
			}
		} else if (ppos) {
			ppos->next = _c;
			_c->prev = ppos;
		} else {
			_r->contacts = _c;
		}
	}
}

/*
 * Update ucontact with new values
 */
int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci)
{
   /* we have to update memory in any case, but database directly
    * only in db_mode 1 */
   if (mem_update_ucontact( _c, _ci) < 0) {
      LM_ERR("failed to update memory\n");
      return -1;
   }
   if (xmlrpc_update_ucontact(_c) < 0) {
      LM_ERR("failed to update database\n");
   }
   return 0;
}
