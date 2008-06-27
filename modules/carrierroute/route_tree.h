/*
 * $Id: route_tree.h 2828 2007-09-27 14:02:31Z henningw $
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
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
 *
 */

/**
 * @file route_tree.h
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief contains the functions to manage routing tree
 * data in a digital tree
 *
 */

#ifndef SP_ROUTE_ROUTE_TREE_H
#define SP_ROUTE_ROUTE_TREE_H

#include "route.h"

/**
 * Adds the given route information to the route tree identified by
 * route_tree. scan_prefix identifies the number for which the information
 * is and the rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * Note that this is a recursive function. It strips off digits from the
 * beginning of scan_prefix and calls itself.
 *
 * @param rt the current route tree node
 * @param scan_prefix the prefix at the current position
 * @param full_prefix the whole scan prefix
 * @param max_targets the number of targets
 * @param prob the weight of the rule
 * @param rewrite_hostpart the rewrite_host of the rule
 * @param strip the number of digits to be stripped off userpart before prepending prefix
 * @param rewrite_local_prefix the rewrite prefix
 * @param rewrite_local_suffix the rewrite suffix
 * @param status the status of the rule
 * @param hash_index the hash index of the rule
 * @param backup indicates if the route is backed up by another. only 
                 useful if status==0, if set, it is the hash value
                 of another rule
 * @param backed_up an -1-termintated array of hash indices of the route 
                    for which this route is backup
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_route()
 */
int add_route_to_tree(struct route_tree_item * rt, const char * scan_prefix,
                             const char * full_prefix, int max_targets, double prob,
                             const char * rewrite_hostpart, int strip, const char * rewrite_local_prefix,
                             const char * rewrite_local_suffix, int status, int hash_index,
                             int backup, int * backed_up, const char * comment);

/**
 * Create a new route tree root in shared memory and set it up.
 *
 * @param domain the domain name of the route tree
 * @param id the domain id of the route tree
 *
 * @return a pointer to the newly allocated route tree or NULL on
 * error, in which case it LOGs an error message.
 */
struct route_tree * create_route_tree(const char * domain, int id);



/**
 * Tries to add a domain to the domain map. If the given domain doesn't
 * exist, it is added. Otherwise, nothing happens.
 *
 * @param domain the domain to be added
 *
 * @return values: on succcess the numerical index of the given domain,
 * -1 on failure
 */
int add_domain(const char * domain);

struct route_tree * get_route_tree_by_id(struct carrier_tree * ct, int id);

void destroy_route_tree(struct route_tree *route_tree);

void destroy_route_map(void);

#endif
