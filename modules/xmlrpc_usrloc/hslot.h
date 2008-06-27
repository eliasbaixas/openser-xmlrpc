
#ifndef XMLRPC_HSLOT_H
#define XMLRPC_HSLOT_H

#include "../../locking.h"

#include "xmlrpc_udomain.h"
#include "xmlrpc_urecord.h"


struct udomain;
struct urecord;


typedef struct hslot {
	int n;                  /* Number of elements in the collision slot */
	struct urecord* first;  /* First element in the list */
	struct urecord* last;   /* Last element in the list */
	struct udomain* d;      /* Domain we belong to */
#ifdef GEN_LOCK_T_PREFERED
	gen_lock_t *lock;       /* Lock for hash entry - fastlock */
#else
	int lockidx;            /* Lock index for hash entry - the rest*/
#endif
} hslot_t;

/*
 * Initialize slot structure
 */
int init_slot(struct udomain* _d, hslot_t* _s, int n);


/*
 * Deinitialize given slot structure
 */
void deinit_slot(hslot_t* _s);


/*
 * Add an element to slot linked list
 */
void slot_add(hslot_t* _s, struct urecord* _r);


/*
 * Remove an element from slot linked list
 */
void slot_rem(hslot_t* _s, struct urecord* _r);

int ul_init_locks();
void ul_unlock_locks();
void ul_destroy_locks();

#ifndef GEN_LOCK_T_PREFERED
void ul_lock_idx(int idx);
void ul_release_idx(int idx);
#endif

#endif /* HSLOT_H */
