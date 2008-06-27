#ifndef UDOMAIN_H
#define UDOMAIN_H


#include <stdio.h>
#include "../../statistics.h"
#include "../../locking.h"
#include "../../str.h"
#include "xmlrpc_urecord.h"
#include "hslot.h"


struct hslot;   /* Hash table slot */
struct urecord; /* Usrloc record */


/*
 * The structure represents a usrloc domain
 */
typedef struct udomain {
	str* name;                 /* Domain name (NULL terminated) */
	int size;                  /* Hash table size */
	struct hslot* table;       /* Hash table - array of collision slots */
	/* statistics */
	stat_var *users;           /* no of registered users */
	stat_var *contacts;        /* no of registered contacts */
	stat_var *expires;         /* no of expires */
} udomain_t;


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d);


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d);


/*
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d);

/*
 * Get lock
 */
typedef void (*lock_udomain_t)(udomain_t* _d, str *_aor);
void lock_udomain(udomain_t* _d, str *_aor);


/*
 * Release lock
 */
typedef void (*unlock_udomain_t)(udomain_t* _d, str *_aor);
void unlock_udomain(udomain_t* _d, str *_aor);


void lock_ulslot(udomain_t* _d, int i);
void unlock_ulslot(udomain_t* _d, int i);

/* ===== module interface ======= */


/*
 * Create and insert a new record
 */
typedef int (*insert_urecord_t)(udomain_t* _d, str* _aor, struct urecord** _r);
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
typedef int  (*get_urecord_t)(udomain_t* _d, str* _a, struct urecord** _r);
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*
 * Delete a urecord from domain
 */
typedef int  (*delete_urecord_t)(udomain_t* _d, str* _a, struct urecord* _r);
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r);


#endif /* UDOMAIN_H */
