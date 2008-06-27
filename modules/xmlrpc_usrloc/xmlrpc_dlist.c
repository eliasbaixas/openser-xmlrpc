
#include "xmlrpc_dlist.h"
#include <stdlib.h>	       /* abort */
#include <string.h>            /* strlen, memcmp */
#include <stdio.h>             /* printf */
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "xmlrpc_udomain.h"           /* new_udomain, free_udomain */
#include "xmlrpc_utime.h"
#include "xmlrpc_ul_mod.h"


/*
 * List of all registered domains
 */
dlist_t* root = 0;


/*
 * Find domain with the given name
 * Returns 0 if the domain was found
 * and 1 of not
 */
static inline int find_dlist(str* _n, dlist_t** _d)
{
   dlist_t* ptr;

   ptr = root;
   while(ptr) {
      if ((_n->len == ptr->name.len) &&
	    !memcmp(_n->s, ptr->name.s, _n->len)) {
	 *_d = ptr;
	 return 0;
      }

      ptr = ptr->next;
   }

   return 1;
}



static inline int get_all_db_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max)
{
   LM_WARN("Not implemented\n");
   return 0;
}



static inline int get_all_mem_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max)
{
   LM_WARN("Not implemented\n");
   return 0;
}



/*
 * Return list of all contacts for all currently registered
 * users in all domains. Caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------+-----+
 * |contact1.len|contact1.s|sock1|flags1|path1|
 * +------------+----------+-----+------+-----+
 * |contact2.len|contact2.s|sock2|flags2|path1|
 * +------------+----------+-----+------+-----+
 * |..........................................|
 * +------------+----------+-----+------+-----+
 * |contactN.len|contactN.s|sockN|flagsN|pathN|
 * +------------+----------+-----+------+-----+
 * |000000000000|
 * +------------+
 */
int get_all_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max)
{
   LM_WARN("Not implemented\n");
   return 0;
}



/*
 * Create a new domain structure
 * Returns 0 if everything went OK, otherwise value < 0
 * is returned
 *
 * The structure is NOT created in shared memory so the
 * function must be called before ser forks if it should
 * be available to all processes
 */
static inline int new_dlist(str* _n, dlist_t** _d)
{
   dlist_t* ptr;

   /* Domains are created before ser forks,
    * so we can create them using pkg_malloc
    */
   ptr = (dlist_t*)shm_malloc(sizeof(dlist_t));
   if (ptr == 0) {
      LM_ERR("no more share memory\n");
      return -1;
   }
   memset(ptr, 0, sizeof(dlist_t));

   /* copy domain name as null terminated string */
   ptr->name.s = (char*)shm_malloc(_n->len+1);
   if (ptr->name.s == 0) {
      LM_ERR("no more memory left\n");
      shm_free(ptr);
      return -2;
   }

   memcpy(ptr->name.s, _n->s, _n->len);
   ptr->name.len = _n->len;
   ptr->name.s[ptr->name.len] = 0;

   if (new_udomain(&(ptr->name), ul_hash_size, &(ptr->d)) < 0) {
      LM_ERR("creating domain structure failed\n");
      shm_free(ptr->name.s);
      shm_free(ptr);
      return -3;
   }

   *_d = ptr;
   return 0;
}


/*
 * Function registers a new domain with xmlrpc-usrloc
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
int register_udomain(const char* _n, udomain_t** _d)
{
   dlist_t* d;
   str s;
   int ver;

   s.s = (char*)_n;
   s.len = strlen(_n);

   if (find_dlist(&s, &d) == 0) {
      *_d = d->d;
      return 0;
   }

   if (new_dlist(&s, &d) < 0) {
      LM_ERR("failed to create new domain\n");
      return -1;
   }

   d->next = root;
   root = d;

   *_d = d->d;
   return 0;

err:
   free_udomain(d->d);
   shm_free(d->name.s);
   shm_free(d);
   return -1;
}


/*
 * Free all allocated memory
 */
void free_all_udomains(void)
{
	dlist_t* ptr;

	while(root) {
		ptr = root;
		root = root->next;

		free_udomain(ptr->d);
		shm_free(ptr->name.s);
		shm_free(ptr);
	}
}


/*
 * Just for debugging
 */
void print_all_udomains(FILE* _f)
{
	dlist_t* ptr;
	
	ptr = root;

	fprintf(_f, "===Domain list===\n");
	while(ptr) {
		print_udomain(_f, ptr->d);
		ptr = ptr->next;
	}
	fprintf(_f, "===/Domain list===\n");
}


/* Loops through all domains summing up the number of users. */
unsigned long get_number_of_users(void)
{
	int numberOfUsers = 0;

	dlist_t* current_dlist;
	
	current_dlist = root;

	while (current_dlist)
	{
		numberOfUsers += get_stat_val(current_dlist->d->users); 
		current_dlist  = current_dlist->next;
	}

	return numberOfUsers;
}


/*
 * Run timer handler of all domains
 */
int synchronize_all_udomains(void)
{
   LM_WARN("No-Op for xmlrpc-usrloc (not implemented)\n");
   return 0;
}


/*
 * Find a particular domain
 */
int find_domain(str* _d, udomain_t** _p)
{
	dlist_t* d;

	if (find_dlist(_d, &d) == 0) {
	        *_p = d->d;
		return 0;
	}

	return 1;
}
