
#ifndef XMLRPC_UCONTACT_H
#define XMLRPC_UCONTACT_H

#include <stdio.h>
#include <time.h>
#include "../../qvalue.h"
#include "../../str.h"

typedef enum cstate {
	CS_NEW,        /* New contact - not flushed yet */
	CS_SYNC,       /* Synchronized contact with the database */
	CS_DIRTY       /* Update contact - not flushed yet */
} cstate_t;
/*
 * Flags that can be associated with a Contact
 */
typedef enum flags {
	FL_NONE        = 0,          /* No flags set */
	FL_MEM         = 1 << 0,     /* Update memory only */
	FL_ALL         = (int)0xFFFFFFFF  /* All flags set */
} flags_t;


typedef struct ucontact {
	str* domain;            /* Pointer to domain name (NULL terminated) */
	str* aor;               /* Pointer to the AOR string in record structure*/
	str c;                  /* Contact address */
	str received;           /* IP+port+protocol we recved the REGISTER from */
	str path;               /* Path header */
	time_t expires;         /* expires parameter */
	qvalue_t q;             /* q parameter */
	str callid;             /* Call-ID header field */
	int cseq;               /* CSeq value */
	cstate_t state;         /* State of the contact */
	unsigned int flags;     /* Various flags (NAT, ping type, etc) */
	unsigned int cflags;    /* custom contact flags (from script) */
	str user_agent;         /* User-Agent header field */
	struct socket_info *sock; /* received soket */
	time_t last_modified;   /* when the record was last modified */
	unsigned int methods;   /* Supported methods */
	struct ucontact* next;  /* Next contact in the linked list */
	struct ucontact* prev;  /* Previous contact in the linked list */
} ucontact_t;

typedef struct ucontact_info {
	str received;
	str* path;
	time_t expires;
	qvalue_t q;
	str* callid;
	int cseq;
	unsigned int flags;
	unsigned int cflags;
	str *user_agent;
	struct socket_info *sock;
	unsigned int methods;
	time_t last_modified;
} ucontact_info_t;

/*
 * ancient time used for marking the contacts forced to expired
 */
#define UL_EXPIRED_TIME 10

/*
 * Valid contact is a contact that either didn't expire yet or is permanent
 */
#define VALID_CONTACT(c, t)   ((c->expires>t) || (c->expires==0))


/*
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact,
		ucontact_info_t* _ci);


/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c);


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c);


/*
 * Update existing contact in memory with new values
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t *_ci);


/* ==== XMLRPC related functions ====== */


/*
 * Insert contact into the database
 */
int xmlrpc_insert_ucontact(ucontact_t* _c);


/*
 * Update contact in the database
 */
int xmlrpc_update_ucontact(ucontact_t* _c);


/*
 * Delete contact from the database
 */
int xmlrpc_delete_ucontact(ucontact_t* _c);


/* ====== Module interface ====== */

struct urecord;

/*
 * Update ucontact with new values
 */
typedef int (*update_ucontact_t)(struct urecord* _r, ucontact_t* _c,
		ucontact_info_t* _ci);

int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci);

#endif /* UCONTACT_H */
