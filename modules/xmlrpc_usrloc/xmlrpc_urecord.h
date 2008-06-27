#ifndef XMLRPC_URECORD_H
#define XMLRPC_URECORD_H

#include <stdio.h>
#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "../../qvalue.h"
#include "xmlrpc_ucontact.h"

#define RECORD_END "===== end of record ======="
#define RECORD_END_LEN (sizeof(RECORD_END) - 1)

struct hslot;

/*
 * Basic hash table element
 */
typedef struct urecord
{
   str* domain;                   /* Pointer to domain we belong to 
				   * ( null terminated string) */
   str aor;                       /* Address of record */
   unsigned int aorhash;          /* Hash over address of record */
   ucontact_t* contacts;          /* One or more contact fields */

   struct hslot* slot;            /* Collision slot in the hash table 
				   * array we belong to */
   struct urecord* prev;          /* Next item in the hash entry */
   struct urecord* next;          /* Previous item in the hash entry */
} urecord_t;


/* Free all memory associated with the element */
void free_urecord(urecord_t* _r);


/*
 * Print an element, for debugging purposes only
 */
void print_urecord(FILE* _f, urecord_t* _r);


/*
 * Add a new contact
 */
ucontact_t* mem_insert_ucontact(urecord_t* _r, str* _c, ucontact_info_t* _ci);


/*
 * Remove the contact from lists
 */
void mem_remove_ucontact(urecord_t* _r, ucontact_t* _c);


/*
 * Remove contact from the list and delete 
 */
void mem_delete_ucontact(urecord_t* _r, ucontact_t* _c);


/*
 * Timer handler
 */
int timer_urecord(urecord_t* _r);


/*
 * Delete the whole record from database
 */
int db_delete_urecord(urecord_t* _r);


/* ===== Module interface ======== */


/*
 * Release urecord previously obtained
 * through get_urecord
 */
typedef void (*release_urecord_t)(urecord_t* _r);
void release_urecord(urecord_t* _r);


/*
 * Insert new contact
 */
typedef int (*insert_ucontact_t)(urecord_t* _r, str* _contact,
		ucontact_info_t* _ci, ucontact_t** _c);
int insert_ucontact(urecord_t* _r, str* _contact,
		ucontact_info_t* _ci, ucontact_t** _c);


/*
 * Delete ucontact from urecord
 */
typedef int (*delete_ucontact_t)(urecord_t* _r, struct ucontact* _c);
int delete_ucontact(urecord_t* _r, struct ucontact* _c);


/*
 * Get pointer to ucontact with given contact
 */
typedef int (*get_ucontact_t)(urecord_t* _r, str* _c, str* _callid, int _cseq,
		struct ucontact** _co);
int get_ucontact(urecord_t* _r, str* _c, str* _callid, int _cseq,
		struct ucontact** _co);


#endif /* URECORD_H */
