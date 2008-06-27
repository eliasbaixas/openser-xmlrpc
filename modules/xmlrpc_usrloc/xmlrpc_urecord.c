
#include "xmlrpc_urecord.h"
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "xmlrpc_ul_mod.h"
#include "xmlrpc_utime.h"

int matching_mode = CONTACT_ONLY;

int cseq_delay = 20;

/*
 * Free all memory used by the given structure
 * The structure must be removed from all linked
 * lists first
 */
void free_urecord(urecord_t* _r)
{
   ucontact_t* ptr;

   while(_r->contacts) {
      ptr = _r->contacts;
      _r->contacts = _r->contacts->next;
      free_ucontact(ptr);
   }
}

/*
 * Print a record
 */
void print_urecord(FILE* _f, urecord_t* _r)
{
   ucontact_t* ptr;

   fprintf(_f, "...Record(%p)...\n", _r);
   fprintf(_f, "domain : '%.*s'\n", _r->domain->len, ZSW(_r->domain->s));
   fprintf(_f, "aor    : '%.*s'\n", _r->aor.len, ZSW(_r->aor.s));
   fprintf(_f, "aorhash: '%u'\n", (unsigned)_r->aorhash);

   if (_r->contacts) {
      ptr = _r->contacts;
      while(ptr) {
	 print_ucontact(_f, ptr);
	 ptr = ptr->next;
      }
   }

   fprintf(_f, ".../Record...\n");
}

/*
 * Add a new contact
 * Contacts are ordered by: 1) q 
 *                          2) descending modification time
 */
ucontact_t* mem_insert_ucontact(urecord_t* _r, str* _c, ucontact_info_t* _ci)
{
   ucontact_t* ptr, *prev = 0;
   ucontact_t* c;

   if ( (c=new_ucontact(_r->domain, &_r->aor, _c, _ci)) == 0) {
      LM_ERR("failed to create new contact\n");
      return 0;
   }
   if_update_stat( _r->slot, _r->slot->d->contacts, 1);

   ptr = _r->contacts;

   if (!desc_time_order) {
      while(ptr) {
	 if (ptr->q < c->q) break;
	 prev = ptr;
	 ptr = ptr->next;
      }
   }

   if (ptr) {
      if (!ptr->prev) {
	 ptr->prev = c;
	 c->next = ptr;
	 _r->contacts = c;
      } else {
	 c->next = ptr;
	 c->prev = ptr->prev;
	 ptr->prev->next = c;
	 ptr->prev = c;
      }
   } else if (prev) {
      prev->next = c;
      c->prev = prev;
   } else {
      _r->contacts = c;
   }

   return c;
}


/*
 * TODO implement deletion mechanism
 */
int xmlrpc_delete_urecord(urecord_t* _r)
{
   LM_WARN("not implemented\n");
   return 0;
}

/*
 * Release urecord previously obtained
 * through get_urecord
 */
void release_urecord(urecord_t* _r)
{
   free_urecord(_r);
}

/*
 * Create and insert new contact
 * into urecord
 */
int insert_ucontact(urecord_t* _r, str* _contact, ucontact_info_t* _ci, ucontact_t** _c)
{
   if ( ((*_c)=mem_insert_ucontact(_r, _contact, _ci)) == 0) {
      LM_ERR("failed to insert contact\n");
      return -1;
   }

   if (xmlrpc_insert_ucontact(*_c) < 0) {
      LM_ERR("failed to insert in xmlrpc\n");
   } else {
      (*_c)->state = CS_SYNC;
   }

   return 0;
}


/*
 * Delete ucontact from urecord
 */
int delete_ucontact(urecord_t* _r, struct ucontact* _c)
{
   if (xmlrpc_delete_ucontact(_c) < 0) {
      LM_ERR("failed to remove contact from database\n");
   }
   return 0;
}


static inline struct ucontact* contact_match( ucontact_t* ptr, str* _c)
{
   while(ptr) {
      if ((_c->len == ptr->c.len) && !memcmp(_c->s, ptr->c.s, _c->len)) {
	 return ptr;
      }

      ptr = ptr->next;
   }
   return 0;
}


static inline struct ucontact* contact_callid_match( ucontact_t* ptr, str* _c, str *_callid)
{
   while(ptr) {
      if ( (_c->len==ptr->c.len) && (_callid->len==ptr->callid.len)
	    && !memcmp(_c->s, ptr->c.s, _c->len)
	    && !memcmp(_callid->s, ptr->callid.s, _callid->len)
	 ) {
	 return ptr;
      }

      ptr = ptr->next;
   }
   return 0;
}


/*
 * Get pointer to ucontact with given contact
 * Returns:
 *      0 - found
 *      1 - not found
 *     -1 - invalid found
 *     -2 - found, but to be skipped (same cseq)
 */
int get_ucontact(urecord_t* _r, str* _c, str* _callid, int _cseq, struct ucontact** _co)
{
   ucontact_t* ptr;
   int no_callid;

   ptr = 0;
   no_callid = 0;
   *_co = 0;

   switch (matching_mode) {
      case CONTACT_ONLY:
	 ptr = contact_match( _r->contacts, _c);
	 break;
      case CONTACT_CALLID:
	 ptr = contact_callid_match( _r->contacts, _c, _callid);
	 no_callid = 1;
	 break;
      default:
	 LM_CRIT("unknown matching_mode %d\n", matching_mode);
	 return -1;
   }

   if (ptr) {
      /* found -> check callid and cseq */
      if ( no_callid || (ptr->callid.len==_callid->len
	       && memcmp(_callid->s, ptr->callid.s, _callid->len)==0 ) ) {
	 if (_cseq<ptr->cseq)
	    return -1;
	 if (_cseq==ptr->cseq) {
	    get_act_time();
	    return (ptr->last_modified+cseq_delay>act_time)?-2:-1;
	 }
      }
      *_co = ptr;
      return 0;
   }

   return 1;
}
