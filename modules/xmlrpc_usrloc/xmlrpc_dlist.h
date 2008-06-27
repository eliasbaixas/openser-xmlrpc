#ifndef DLIST_H
#define DLIST_H

#include <stdio.h>
#include "xmlrpc_udomain.h"
#include "../../str.h"


/*
 * List of all domains registered with usrloc
 */
typedef struct dlist {
	str name;            /* Name of the domain (null terminated) */
	udomain_t* d;        /* Payload */
	struct dlist* next;  /* Next element in the list */
} dlist_t;


extern dlist_t* root;

/*
 * Function registers a new domain with usrloc
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
typedef int (*register_udomain_t)(const char* _n, udomain_t** _d);
int register_udomain(const char* _n, udomain_t** _d);


/*
 * Free all registered domains
 */
void free_all_udomains(void);


/*
 * Just for debugging
 */
void print_all_udomains(FILE* _f);


/*
 * Called from timer
 */
int synchronize_all_udomains(void);


/*
 * Get contacts to all registered users
 */
typedef int  (*get_all_ucontacts_t) (void* buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max);
int get_all_ucontacts(void *, int, unsigned int,
		unsigned int part_idx, unsigned int part_max);


/* Sums up the total number of users in memory, over all domains. */
unsigned long get_number_of_users();


/*
 * Find a particular domain
 */
int find_domain(str* _d, udomain_t** _p);


#endif /* UDLIST_H */
