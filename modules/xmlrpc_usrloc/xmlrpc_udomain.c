/* 
 */

#include <string.h>
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "xmlrpc_ul_mod.h"            /* usrloc module parameters */
#include "xmlrpc_utime.h"
#include "xmlrpc_udomain.h"
#include "xmlrpc_urecord.h"

#ifdef STATISTICS
static char *build_stat_name( str* domain, char *var_name)
{
   int n;
   char *s;
   char *p;

   n = domain->len + 1 + strlen(var_name) + 1;
   s = (char*)shm_malloc( n );
   if (s==0) {
      LM_ERR("no more shm mem\n");
      return 0;
   }
   memcpy( s, domain->s, domain->len);
   p = s + domain->len;
   *(p++) = '-';
   memcpy( p , var_name, strlen(var_name));
   p += strlen(var_name);
   *(p++) = 0;
   return s;
}
#endif


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d)
{
   int i;
#ifdef STATISTICS
   char *name;
#endif

   /* Must be always in shared memory, since
    * the cache is accessed from timer which
    * lives in a separate process
    */
   *_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
   if (!(*_d)) {
      LM_ERR("new_udomain(): No memory left\n");
      goto error0;
   }
   memset(*_d, 0, sizeof(udomain_t));

   (*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
   if (!(*_d)->table) {
      LM_ERR("no memory left 2\n");
      goto error1;
   }

   (*_d)->name = _n;

   for(i = 0; i < _s; i++) {
      if (init_slot(*_d, &((*_d)->table[i]), i) < 0) {
	 LM_ERR("initializing hash table failed\n");
	 goto error2;
      }
   }

   (*_d)->size = _s;

#ifdef STATISTICS
   /* register the statistics */
   if ( (name=build_stat_name(_n,"users"))==0 || register_stat("usrloc",
	    name, &(*_d)->users, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
      LM_ERR("failed to add stat variable\n");
      goto error2;
   }
   if ( (name=build_stat_name(_n,"contacts"))==0 || register_stat("usrloc",
	    name, &(*_d)->contacts, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
      LM_ERR("failed to add stat variable\n");
      goto error2;
   }
   if ( (name=build_stat_name(_n,"expires"))==0 || register_stat("usrloc",
	    name, &(*_d)->expires, STAT_SHM_NAME)!=0 ) {
      LM_ERR("failed to add stat variable\n");
      goto error2;
   }
#endif

   return 0;
error2:
   shm_free((*_d)->table);
error1:
   shm_free(*_d);
error0:
   return -1;
}

/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d)
{
   int i;

   if (_d->table) {
      for(i = 0; i < _d->size; i++) {
	 lock_ulslot(_d, i);
	 deinit_slot(_d->table + i);
	 unlock_ulslot(_d, i);
      }
      shm_free(_d->table);
   }
   shm_free(_d);
}

/*
 * Returns a static dummy urecord for temporary usage
 */
static inline void get_static_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
   static struct urecord r;

   memset( &r, 0, sizeof(struct urecord) );
   r.aor = *_aor;
   r.domain = _d->name;
   *_r = &r;
}

/*
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d)
{
   int i;
   int max=0, slot=0, n=0;
   struct urecord* r;
   fprintf(_f, "---Domain---\n");
   fprintf(_f, "name : '%.*s'\n", _d->name->len, ZSW(_d->name->s));
   fprintf(_f, "size : %d\n", _d->size);
   fprintf(_f, "table: %p\n", _d->table);
   /*fprintf(_f, "lock : %d\n", _d->lock); -- can be a structure --andrei*/
   fprintf(_f, "\n");
   for(i=0; i<_d->size; i++)
   {
      r = _d->table[i].first;
      n += _d->table[i].n;
      if(max<_d->table[i].n){
	 max= _d->table[i].n;
	 slot = i;
      }
      while(r) {
	 print_urecord(_f, r);
	 r = r->next;
      }
   }
   fprintf(_f, "\nMax slot: %d (%d/%d)\n", max, slot, n);
   fprintf(_f, "\n---/Domain---\n");
}

/*
 * expects 12 rows (contact, expirs, q, callid, cseq, flags, 
 *   ua, received, path, socket, methods, last_modified)
 * separated by '\n'
 */
static inline ucontact_info_t* my_row2info( str *vals, str *contact)
{
   static ucontact_info_t ci;
   static str callid, ua, received, host, path,expires,q,cseqn,flags,cflags,methods,lastmod,hostport,end_of_record;
   int port, proto,len;
   char *p;

   memset( &ci, 0, sizeof(ucontact_info_t));
   p=vals->s;
   len=vals->len;

   for (;(len>0)&&(*p!='=');p++,len--);
   contact->s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   contact->len = (int) (p - contact->s);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   expires.s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   expires.len = (int) (p - expires.s);

   str2int(&expires,(unsigned int*)&ci.expires);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   q.s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   q.len = (int) (p - q.s);

   str2int(&q,(unsigned int*)&ci.q);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   cseqn.s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   cseqn.len = (int) (p - cseqn.s);

   str2int(&cseqn,(unsigned int*)&ci.cseq);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   callid.s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   callid.len = (int) (p - callid.s);

   ci.callid = &callid;

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   flags.s = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   flags.len = (int) (p - flags.s);
   str2int(&flags,(unsigned int*)&ci.flags);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   cflags.s=p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   cflags.len = (int) (p - cflags.s);
   str2int(&cflags,(unsigned int*)&ci.cflags);

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   ua.s  = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   ua.len = (int) (p - ua.s);
   ci.user_agent = &ua;

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   received.s  = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   received.len = (int) (p - received.s);

   ci.received = received;

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   path.s  = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   path.len = (int) (p - path.s);

   ci.path= &path;

   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   hostport.s  = p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   hostport.len = (int) (p - hostport.s);

   /* socket name */
   if (parse_phostport( hostport.s, hostport.len, &host.s, &host.len, &port, &proto)!=0) {
      LM_ERR("bad socket <%s>\n", p);
      return 0;
   }
   ci.sock = grep_sock_info( &host, (unsigned short)port, proto);
   if (ci.sock==0) {
      LM_WARN("non-local socket <%s>...ignoring\n", p);
   }

   /* supported methods */
   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   methods.s=p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   methods.len = (int) (p - methods.s);

   if (methods.len == 0) {
      ci.methods = ALL_METHODS;
   } else {
      str2int(&methods,(unsigned int*)&ci.methods);
   }

   /* last modified time */
   if ((*p=='\n')&&(len>0)) { p++;len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   lastmod.s=p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   lastmod.len = (int) (p - lastmod.s);
   if (lastmod.len>0) {
      ci.last_modified = str2int(&lastmod,(unsigned int*)&ci.last_modified);
   }
   if ((*p=='\n')&&(len>0)) { p++; len--; }
   for (;(len>0)&&(*p!='=');p++,len--);
   end_of_record.s=p;
   for (;(len>0)&&(*p!='\n');p++,len--);
   end_of_record.len=len;
   if (end_of_record.len != RECORD_END_LEN || memcmp(end_of_record.s,RECORD_END,RECORD_END_LEN)) {
      LM_WARN("End of record not matched !: [%.*s]!\n",vals->len,vals->s);
   }

   return &ci;
}

void print_xmlrpc_value(int spaces,xmlrpc_value* val)
{
#define SPACES "                      "
   int size,i;
   xmlrpc_value* tmp;
   if(xmlrpc_value_type(val)==XMLRPC_TYPE_ARRAY){
      LM_DBG("%.*s[\n",spaces,SPACES);
      size = xmlrpc_array_size(&env,val);
      for(i=0;i<size;i++){
	 xmlrpc_array_read_item(&env,val,i,&tmp);
	 print_xmlrpc_value(spaces+2,tmp);
      }
      LM_DBG("%.*s]\n",spaces,SPACES);
   }else
      LM_DBG("%.*s%s\n",spaces,SPACES,xmlrpc_typeName(xmlrpc_value_type(val)));
}
/*
 * loads from the string retrieved from XML-RPC all contacts for an AOR
 */
urecord_t* xmlrpc_load_urecord(udomain_t* _d, str *_aor)
{
#define MAX_VALS 10
   str contact,result_txt,result_hash;
   char *domain,*digest;
   int i,size,size2,holderlen,real_timeout;

   xmlrpc_value *result;
   xmlrpc_value *result_contact;
   xmlrpc_value *result_contacts;
   xmlrpc_value *result_stuff;

   ucontact_info_t *ci;
   urecord_t* r=0;
   ucontact_t* c;

   if(_aor->len > 40){
      LM_ERR("AoR too big !\n");
      return 0;
   }

   result = xmlrpc_client_call(&env,server_url,"get_details","(6i6s)",_aor->s,_aor->len,MAX_VALS,"",0,"openser_xmlrpc_usrloc");
   die_if_fault_occurred(&env);
   print_xmlrpc_value(0,result);
   if(0>(size = xmlrpc_array_size(&env,result))){
      LM_ERR("Array size <0 ?!");
      return 0;
   }
   xmlrpc_array_read_item(&env,result,0,&result_contacts);
   die_if_fault_occurred(&env);
   if(xmlrpc_value_type(result_contacts) != XMLRPC_TYPE_ARRAY){
      LM_ERR("Returned value is not of type array !\n");
      return 0;
   }
   if(0>(size = xmlrpc_array_size(&env,result_contacts))){
      LM_ERR("Array size <0 ?!");
      return 0;
   }
   LM_DBG("Contacts for %.*s =  %d\n",_aor->len,_aor->s,size);
   while(size--){
      xmlrpc_array_read_item(&env,result_contacts,size,&result_contact);
      die_if_fault_occurred(&env);
      if(!(xmlrpc_value_type(result_contact)==XMLRPC_TYPE_ARRAY))
	 continue;
      xmlrpc_array_read_item(&env,result_contact,0,&result_stuff);
      if(!(xmlrpc_value_type(result_stuff)==XMLRPC_TYPE_BASE64))
	 continue;
      xmlrpc_read_base64(&env,result_stuff,&(result_txt.len),&(result_txt.s));
      xmlrpc_array_read_item(&env,result_contact,1,&result_stuff);
      xmlrpc_read_int(&env,result_stuff,&real_timeout);
      xmlrpc_array_read_item(&env,result_contact,2,&result_stuff);
      xmlrpc_read_string(&env,result_stuff,&digest);
      xmlrpc_array_read_item(&env,result_contact,3,&result_stuff);
      xmlrpc_read_base64(&env,result_stuff,&(result_hash.len),&(result_hash.s));
      die_if_fault_occurred(&env);
      LM_DBG("Read contact: \n[%.*s]\n timeout:%d\ndigest:%s\nhash:%.*s\n",result_txt.len,result_txt.s,real_timeout,digest,result_hash.len,result_hash.s);
      if(result_txt.len == 0)
	 continue;
      r = 0;
      ci = my_row2info(&result_txt, &contact);

      if (ci==0) {
	 LM_ERR("skipping record for %.*s in table %s\n",
	       _aor->len, _aor->s, _d->name->s);
	 continue;
      }
      /* TODO dirt-hack: We over-write the *path* field in ucontact struct, so by now xmlrpc_usrloc does *not* implement Path: header handling*/
      ci->path->s=result_hash.s;
      ci->path->len=20;/* first we set it to 20 so that mem_insert_ucontact will copy the 20 bytes */
      if (r==0 )
	 get_static_urecord( _d, _aor, &r);
      if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
	 LM_ERR("mem_insert_ucontact failed\n");
	 free_urecord(r);
	 return 0;
      }
      c->path.len=0;
      ci->path->len=0;/* Then we set it to zero so that nobody else will use it wrongly, 
			 ... hmm but maybe id doesn't get freed because of that  TODO!!!*/
      /* We have to do this, because insert_ucontact sets state to CS_NEW
       * and we have the contact in the database already */
      c->state = CS_SYNC;
   }

   return r;
}


/*
 * NOOPs
 */
void lock_udomain(udomain_t* _d, str* _aor)
{
   ;/*noop*/
}
void unlock_udomain(udomain_t* _d, str* _aor)
{
   ;/*noop*/
}
void lock_ulslot(udomain_t* _d, int i)
{
   ;/*noop*/
}
void unlock_ulslot(udomain_t* _d, int i)
{
   ;/*noop*/
}

/*
 * Create and insert a new record
 */
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
   get_static_urecord( _d, _aor, _r);
   return 0;
}

/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	unsigned int sl, i, aorhash;
	urecord_t* r;

	r = xmlrpc_load_urecord(_d, _aor);
	if (r) {
	   print_urecord(stderr,r);
	   *_r = r;
	   return 0;
	}
	return 1;   /* Nothing found */
}

/*
 * Delete a urecord from domain
 */
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r)
{
   struct ucontact* c, *t;

   if (_r==0)
      get_static_urecord( _d, _aor, &_r);
   if (xmlrpc_delete_urecord(_r)<0) {
      LM_ERR("XMLRPC delete failed\n");
      return -1;
   }
   free_urecord(_r);
   return 0;
}
