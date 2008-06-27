/*
 * $Id: hash.h 1304 2006-12-05 13:48:23Z juhe $
 *
 * Header file for trusted and address hash table functions
 *
 * Copyright (C) 2003-2006 Juha Heinanen
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
 */

#ifndef _PERM_HASH_H_
#define _PERM_HASH_H_

#include <stdio.h>
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../usr_avp.h"
#include "../../mi/mi.h"

#define PERM_HASH_SIZE 128

/*
 * Structure stored in trusted hash table
 */
struct trusted_list {
	str src_ip;                 /* Source IP of SIP message */
	int proto;                  /* Protocol -- UDP, TCP, TLS, or SCTP */
	char *pattern;              /* Pattern matching From header field */
	str tag;                    /* Tag to be assigned to AVP */
	struct trusted_list *next;  /* Next element in the list */
};


/*
 * Parse and init tag avp specification
 */
int init_tag_avp(char *tag_avp_param);


/*
 * Gets tag avp specs
 */
void get_tag_avp(int_str *tag_avp_p, int *tag_avp_type_p);


/*
 * Create and initialize a hash table
 */
struct trusted_list** new_hash_table(void);


/*
 * Release all memory allocated for a hash table
 */
void free_hash_table(struct trusted_list** table);


/*
 * Destroy a hash table
 */
void destroy_hash_table(struct trusted_list** table);


/* 
 * Add <src_ip, proto, pattern> into hash table, where proto is integer
 * representation of string argument proto.
 */
int hash_table_insert(struct trusted_list** hash_table, char* src_ip,
		      char* proto, char* pattern, char* tag);


/* 
 * Check if an entry exists in hash table that has given src_ip and protocol
 * value and pattern that matches to From URI.
 */
int match_hash_table(struct trusted_list** table, struct sip_msg* msg);


/* 
 * Print entries stored in hash table 
 */
void hash_table_print(struct trusted_list** hash_table, FILE* reply_file);
int hash_table_mi_print(struct trusted_list **hash_table, struct mi_node* rpl);

/* 
 * Empty hash table
 */
void empty_hash_table(struct trusted_list** hash_table);


/*
 * Structure stored in address hash table
 */
struct addr_list {
    unsigned int grp;
    unsigned int ip_addr;
    unsigned int port;
    struct addr_list *next;  /* Next element in the list */
};


/*
 * Create and initialize a hash table
 */
struct addr_list** new_addr_hash_table(void);


/*
 * Release all memory allocated for a hash table
 */
void free_addr_hash_table(struct addr_list** table);


/*
 * Destroy a hash table
 */
void destroy_addr_hash_table(struct addr_list** table);


/* 
 * Add <group, ip_addr, port> into hash table
 */
int addr_hash_table_insert(struct addr_list** hash_table, unsigned int grp,
			   unsigned int ip_addr, unsigned int port);


/* 
 * Check if an entry exists in hash table that has given group, ip_addr, and
 * port.  Port 0 in hash table matches any port.
 */
int match_addr_hash_table(struct addr_list** table, unsigned int grp,
			  unsigned int ip_addr, unsigned int port);


/* 
 * Print addresses stoed in hash table
 */
void addr_hash_table_print(struct addr_list** hash_table, FILE* reply_file);
int addr_hash_table_mi_print(struct addr_list** hash_table,
			     struct mi_node* rpl);


/* 
 * Empty hash table
 */
void empty_addr_hash_table(struct addr_list** hash_table);


#define PERM_MAX_SUBNETS 128 


/*
 * Structure used to store a subnet
 */
struct subnet {
    unsigned int grp;        /* address group, subnet count in last record */
    unsigned int subnet;     /* IP subnet in host byte order with host bits shifted out */
    unsigned int port;       /* port or 0 */
    unsigned int mask;       /* how many bits belong to network part */
};


/*
 * Create a subnet table
 */
struct subnet* new_subnet_table(void);


/* 
 * Check if an entry exists in subnet table that matches given group, ip_addr,
 * and port.  Port 0 in subnet table matches any port.
 */
int match_subnet_table(struct subnet* table, unsigned int group,
		       unsigned int ip_addr, unsigned int port);


/* 
 * Empty contents of subnet table
 */
void empty_subnet_table(struct subnet *table);


/*
 * Release memory allocated for a subnet table
 */
void free_subnet_table(struct subnet* table);


/* 
 * Add <grp, subnet, mask, port> into subnet table so that table is
 * kept ordered according to subnet, port, grp.
 */
int subnet_table_insert(struct subnet* table, unsigned int grp,
			unsigned int subnet, unsigned int mask,
			unsigned int port);


/* 
 * Print subnets stored in subnet table
 */
void subnet_table_print(struct subnet* table, FILE* reply_file);
int subnet_table_mi_print(struct subnet* table, struct mi_node* rpl);


#endif /* _PERM_HASH_H_ */
