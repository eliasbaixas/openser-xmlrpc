/*
 * $Id: route_func.c 3823 2008-03-03 11:26:16Z henningw $
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
 * @file route_func.c
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief routing and balancing functions
 *
 */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include "route_func.h"
#include "route_tree.h"
#include "route_db.h"
#include "../../sr_module.h"
#include "../../action.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../mem/mem.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../prime_hash.h"
#include "carrierroute.h"


enum hash_algorithm {
    alg_crc32, /*!< hashing algorithm is CRC32 */
    alg_prime /*!< hashing algorithm is (right 18 digits of hash_source % prime_number) % max_targets + 1 */
};

static int determine_and_rewrite_uri(struct sip_msg* msg, int domain,
                                     enum hash_source hash,
                                     enum hash_algorithm alg);

static int determine_to_and_rewrite_uri(struct sip_msg* msg, int domain,
                                        enum hash_source hash,
                                        enum hash_algorithm alg);

static int determine_from_and_rewrite_uri(struct sip_msg* msg, int domain,
        enum hash_source hash,
        enum hash_algorithm alg);

static int rewrite_msg(int domain, str * uri, struct sip_msg * msg, str * user,
                       enum hash_source hash_source, enum hash_algorithm alg);

static int carrier_rewrite_msg(int carrier, int domain,
                               str * uri, struct sip_msg * msg, str * user,
                               enum hash_source hash_source,
                               enum hash_algorithm alg);

static int rewrite_uri_recursor(struct route_tree_item *route_tree, str *uri,
                                str *dest, struct sip_msg *msg, str * user,
                                enum hash_source hash_source,
                                enum hash_algorithm alg);

static int rewrite_on_rule(struct route_tree_item *route_tree, str *dest,
                           struct sip_msg *msg, str * user,
                           enum hash_source hash_source,
                           enum hash_algorithm alg);

static int actually_rewrite(struct route_rule *rs, str *dest,
                            struct sip_msg *msg, str * user);

static struct route_rule * get_rule_by_hash(struct route_tree_item * rt, int prob);

/**
 * rewrites the request URI of msg by calculating a rule, using
 * crc32 for hashing. The request URI is used to determine tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_uri(struct sip_msg* msg, char* domain_param, char* hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_and_rewrite_uri(msg, domain, my_hash_source, alg_crc32);
}

/**
 * rewrites the request URI of msg by calculating a rule, using
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The request URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_uri(struct sip_msg * msg, char * domain_param, char * hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_and_rewrite_uri(msg, domain, my_hash_source, alg_prime);
}

/*
 * Get To header field URI
 */
static inline int get_to_uri(struct sip_msg* _m, str* _u) {
	if (!_m->to && ((parse_headers(_m, HDR_TO_T, 0) == -1) || (!_m->to))) {
		LM_ERR("Can't get To header field\n");
		return -1;
	}

	_u->s = ((struct to_body*)_m->to->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->to->parsed)->uri.len;

	return 0;
}


/*
 * Get From header field URI
 */
static inline int get_from_uri(struct sip_msg* _m, str* _u) {
	if (parse_from_header(_m) < 0) {
		LM_ERR("Error while parsing From body\n");
		return -1;
	}

	_u->s = ((struct to_body*)_m->from->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->from->parsed)->uri.len;

	return 0;
}

/**
 * rewrites the request URI of msg by calculating a rule, using
 * crc32 for hashing. The request URI is used to determine tree node
 * the given _user is used to determine the routing tree.
 *
 * @param msg the current SIP message
 * @param _uri the URI to determine the route tree (string or pseudo-variable)
 * @param _domain the requested routing domain
 *
 * @return 1 on success, -1 on failure
 */
int user_route_uri(struct sip_msg * _msg, char * _uri, char * _domain) {
	pv_elem_t *model;
	str uri, user, str_domain, ruser, ruri;
	struct sip_uri puri;
	int carrier_id, domain, index;
	domain = (int)(long)_domain;
	struct rewrite_data * rd = NULL;
	struct carrier_tree * ct = NULL;

	if (!_uri) {
		LM_ERR("bad parameter\n");
		return -1;
	}
	
	if (parse_sip_msg_uri(_msg) < 0) {
		return -1;
	}

	/* Retrieve uri from parameter */
	model = (pv_elem_t*)_uri;
	if (pv_printf_s(_msg, model, &uri)<0)	{
		LM_ERR("cannot print the format\n");
		return -1;
	}

	if (parse_uri(uri.s, uri.len, &puri) < 0) {
		LM_ERR("Error while parsing URI\n");
		return -5;
	}
	user = puri.user;
	str_domain = puri.host;

	ruser.s = _msg->parsed_uri.user.s;
	ruser.len = _msg->parsed_uri.user.len;
	ruri.s = _msg->parsed_uri.user.s;
	ruri.len = _msg->parsed_uri.user.len;

	do {
		rd = get_data();
	} while (rd == NULL);

	if ((carrier_id = load_user_carrier(&user, &str_domain)) < 0) {
		release_data(rd);
		return -1;
	} else if (carrier_id == 0) {
		index = rd->default_carrier_index;
	} else {
		if ((ct = get_carrier_tree(carrier_id, rd)) == NULL) {
			if (fallback_default) {
				index = rd->default_carrier_index;
			} else {
				LM_ERR("desired routing tree with id %i doesn't exist\n",
					carrier_id);
				release_data(rd);
				return -1;
			}
		} else {
			index = ct->index;
		}
	}
	release_data(rd);
	return carrier_rewrite_msg(index, domain, &ruri, _msg, &ruser, shs_call_id, alg_crc32);
}

/**
 * rewrites the request URI of msg by calculating a rule, using
 * crc32 for hashing. The request URI is used to determine tree node
 * the given _tree is the used routing tree
 *
 * @param msg the current SIP message
 * @param _tree the routing tree to be used (string or pseudo-variable
 * @param _domain the requested routing domain
 *
 * @return 1 on success, -1 on failure
 */
int tree_route_uri(struct sip_msg * msg, char * _tree, char * _domain) {
	struct rewrite_data * rd = NULL;
	pv_elem_t *model;
	str carrier_name;
	int index;
	str ruser;
	str ruri;

	if (!_tree) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}

	/* Retrieve carrier name from parameter */
	model = (pv_elem_t*)_tree;
	if (pv_printf_s(msg, model, &carrier_name)<0)	{
		LM_ERR("cannot print the format\n");
		return -1;
	}
	if ((index = find_tree(carrier_name)) < 0)
		LM_WARN("could not find carrier %.*s\n",
				carrier_name.len, carrier_name.s);
	else
		LM_DBG("tree %.*s has id %i\n", carrier_name.len, carrier_name.s, index);
	
	ruser.s = msg->parsed_uri.user.s;
	ruser.len = msg->parsed_uri.user.len;
	ruri.s = msg->parsed_uri.user.s;
	ruri.len = msg->parsed_uri.user.len;
	do {
		rd = get_data();
	} while (rd == NULL);
	if (index < 0) {
		if (fallback_default) {
			LM_NOTICE("invalid tree id %i specified, use default tree\n", index);
			index = rd->default_carrier_index;
		} else {
			LM_ERR("invalid tree id %i specified and fallback deactivated\n", index);
			release_data(rd);
			return -1;
		}
	}
	release_data(rd);
	return carrier_rewrite_msg(index, (int)(long)_domain, &ruri, msg, &ruser,
			shs_call_id, alg_crc32);
}

/**
 * rewrites the request URI of msg by calculating a rule,
 * using crc32 for hashing. The to URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_by_to(struct sip_msg* msg, char* domain_param, char* hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_to_and_rewrite_uri(msg, domain, my_hash_source, alg_crc32);
}

/**
 * rewrites the request URI of msg by calculating a rule, using
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The to URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_by_to(struct sip_msg* msg, char* domain_param, char* hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_to_and_rewrite_uri(msg, domain, my_hash_source, alg_prime);
}

/**
 * rewrites the request URI of msg by calculating a rule,
 * using crc32 for hashing. The from URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_by_from(struct sip_msg* msg, char* domain_param, char* hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_from_and_rewrite_uri(msg, domain, my_hash_source, alg_crc32);
}

/**
 * rewrites the request URI of msg by calculating a rule, using
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The from URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param domain_param the requested routing domain
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_by_from(struct sip_msg* msg, char* domain_param, char* hash) {
	int domain;
	enum hash_source my_hash_source;

	domain = (int)(long)domain_param;
	my_hash_source = (enum hash_source)hash;
	return determine_from_and_rewrite_uri(msg, domain, my_hash_source, alg_prime);
}

/**
 * Like determine_and_rewrite_uri, except the difference that the
 * to URI is used instead of the request URI
 *
 * @param msg the current SIP message
 * @param domain the requested routing domain
 * @param hash the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 1 on success, -1 on failure
 *
 * @see determine_and_rewrite_uri()
 */
static int determine_to_and_rewrite_uri(struct sip_msg* msg, int domain,
                                        enum hash_source hash,
                                        enum hash_algorithm alg) {
	str user;
	str to_user;
	struct to_body * to;
	struct sip_uri to_uri;

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}

	if (!msg->to && ((parse_headers(msg, HDR_TO_T, 0) == -1) || !msg->to)) {
		LM_ERR("Message has no To header\n");
		return -1;
	}

	to = get_to(msg);

	if (parse_uri(to->uri.s, to->uri.len, &to_uri) < 0) {
		LM_ERR("Failed to parse To URI.\n");
		return -1;
	}
	to_user.s = to_uri.user.s;
	to_user.len = to_uri.user.len;

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}
	user.s = msg->parsed_uri.user.s;
	user.len = msg->parsed_uri.user.len;

	return rewrite_msg(domain, &to_user, msg, &user, hash, alg);
}

/**
 * Like determine_and_rewrite_uri, except the difference that the
 * from URI is used instead of the request URI
 *
 * @param msg the current SIP message
 * @param domain the requested routing domain
 * @param hash the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 1 on success, -1 on failure
 *
 * @see determine_and_rewrite_uri()
 */
static int determine_from_and_rewrite_uri(struct sip_msg* msg, int domain,
        enum hash_source hash,
        enum hash_algorithm alg) {
	str user;
	str from_user;
	struct to_body * from;
	struct sip_uri from_uri;

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}

	if (parse_from_header(msg) == -1) {
		LM_ERR("validate_msg: Message has no From header\n");
		return -1;
	}

	from = get_from(msg);

	if (parse_uri(from->uri.s, from->uri.len, &from_uri) < 0) {
		LM_ERR("Failed to parse From URI.\n");
		return -1;
	}
	from_user.s = from_uri.user.s;
	from_user.len = from_uri.user.len;

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}
	user.s = msg->parsed_uri.user.s;
	user.len = msg->parsed_uri.user.len;

	return rewrite_msg(domain, &from_user, msg, &user, hash, alg);
}

/**
 * extracts the request URI from msg and passes it to rewrite_msg
 *
 * @param msg the current SIP message
 * @param domain the requested routing domain
 * @param hash the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 1 on success, -1 on failure
 */
static int determine_and_rewrite_uri(struct sip_msg* msg, int domain,
                                     enum hash_source hash,
                                     enum hash_algorithm alg) {
	str user;
	str uri;

	if (parse_sip_msg_uri(msg) < 0) {
		return -1;
	}
	user.s = msg->parsed_uri.user.s;
	user.len = msg->parsed_uri.user.len;
	uri.s = msg->parsed_uri.user.s;
	uri.len = msg->parsed_uri.user.len;

	return rewrite_msg(domain, &uri, msg, &user, hash, alg);
}

static int rewrite_msg(int domain,
                       str * uri, struct sip_msg * msg, str * user,
                       enum hash_source hash_source,
                       enum hash_algorithm alg) {
	int index;
	struct rewrite_data * rd;
	do {
		rd = get_data();
	} while (rd == NULL);
	index = rd->default_carrier_index;
	release_data(rd);
	return carrier_rewrite_msg(index, domain, uri, msg, user, hash_source, alg);
}

/**
 * rewrites the request URI of msg after determining the
 * new destination URI
 *
 * @param carrier the requested carrier
 * @param domain the requested routing domain
 * @param uri the URI to be rewritten
 * @param msg the current SIP message
 * @param user the localpart of the URI to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 1 on success, -1 on failure
 */
static int carrier_rewrite_msg(int carrier, int domain,
                               str * uri, struct sip_msg * msg, str * user,
                               enum hash_source hash_source,
                               enum hash_algorithm alg) {
	struct rewrite_data *rd;
	struct route_tree * rt;
	struct action act;
	str dest;
	int ret;

	do {
		rd = get_data();
	} while (rd == NULL);

	if (carrier >= rd->tree_num) {
		LM_ERR("desired carrier doesn't exist. (We only have %ld carriers, "
			"you wanted %d.)\n", (long)(rd->tree_num) - 1, carrier);
		ret = -1;
		goto unlock_and_out;
	}
	if ((rt = get_route_tree_by_id(rd->carriers[carrier], domain)) == NULL) {
		LM_ERR("desired routing domain doesn't exist, uri %.*s, carrier %d, domain %d\n",
			user->len, user->s, carrier, domain);
		ret = -1;
		goto unlock_and_out;
	}
	if (rewrite_uri_recursor(rt->tree, uri, &dest, msg, user, hash_source, alg) != 0) {
		LM_ERR("during rewrite_uri_recursor, uri %.*s, carrier %d, domain %d\n", user->len,
			user->s, carrier, domain);
		ret = -1;
		goto unlock_and_out;
	}

	LM_INFO("uri %.*s was rewritten to %.*s\n", user->len, user->s, dest.len, dest.s);

	act.type = SET_URI_T;
	act.elem[0].type= STRING_ST;
	act.elem[0].u.string = dest.s;
	act.next = NULL;

	ret = do_action(&act, msg);
	if (ret < 0) {
		LM_ERR("Error in do_action()\n");
	}
	pkg_free(dest.s);
unlock_and_out:
	release_data(rd);
	return ret;
}

/**
 * traverses the routing tree until a matching rule is found
 * The longest match is taken, so it is possible to define
 * route rules for a single number
 *
 * @param route_tree the current routing tree node
 * @param uri the uri to be rewritten at the current position
 * @param dest the returned new destination URI
 * @param msg the sip message
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 0 on success, -1 on failure, 1 on no more matching child node and no rule list
 */
static int rewrite_uri_recursor(struct route_tree_item * route_tree, str * uri,
                                str * dest, struct sip_msg * msg, str * user,
                                enum hash_source hash_source,
                                enum hash_algorithm alg) {
	struct route_tree_item *re_tree;
	str re_uri;

	/* Skip over non-digits.  */
	while (uri->len > 0 && !isdigit(*uri->s)) {
		++uri->s;
		--uri->len;
	}
	if (uri->len == 0 || route_tree->nodes[*uri->s - '0'] == NULL) {
		if (route_tree->rule_list == NULL) {
			LM_INFO("URI or route tree nodes empty, empty rule list\n");
			return 1;
		} else {
			return rewrite_on_rule(route_tree, dest, msg, user, hash_source, alg);
		}
	} else {
		/* match, goto the next number of the uri and try again */
		re_tree = route_tree->nodes[*uri->s - '0'];
		re_uri.s = uri->s + 1;
		re_uri.len = uri->len - 1;
		switch (rewrite_uri_recursor(re_tree, &re_uri, dest, msg, user, hash_source, alg)) {
			case 0:
				return 0;
			case 1:
				if (route_tree->rule_list != NULL) {
					return rewrite_on_rule(route_tree, dest, msg, user, hash_source, alg);
				} else {
					LM_INFO("empty rule list for prefix [%.*s]%.*s\n", user->len - re_uri.len,
						user->s, re_uri.len, re_uri.s);
					return 1;
				}
			default:
				return -1;
		}
	}
}

/**
 * writes the uri dest using the rule list of route_tree
 *
 * @param route_tree the current routing tree node
 * @param dest the returned new destination URI
 * @param msg the sip message
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 *
 * @return 0 on success, -1 on failure
 */
static int rewrite_on_rule(struct route_tree_item * route_tree, str * dest,
                           struct sip_msg * msg, str * user,
                           enum hash_source hash_source,
                           enum hash_algorithm alg) {
	struct route_rule * rr;
	int prob;

	assert(route_tree != NULL);
	assert(route_tree->rule_list != NULL);

	switch (alg) {
		case alg_prime:
			if ((prob = prime_hash_func(msg, hash_source, route_tree->max_targets)) < 0) {
				return -1;
			}
			if ((rr = get_rule_by_hash(route_tree, prob)) == NULL) {
				LM_CRIT("no route found\n");
				return -1;
			}
			break;
		case alg_crc32:
			if(route_tree->dice_max == 0){
				return -1;
			}
			if ((prob = hash_func(msg, hash_source, route_tree->dice_max)) < 0) {
				return -1;
			}
			/* This auto-magically takes the last rule if anything is broken.
			 * Sometimes the hash result is zero. If the first rule is off
			 * (has a probablility of zero) then it has also a dice_to of
			 * zero and the message could not be routed at all if we use
			 * '<' here. Thus the '<=' is necessary.
			 */
			for (rr = route_tree->rule_list;
			        rr->next != NULL && rr->dice_to <= prob;
		        rr = rr->next) {}
			if (!rr->status) {
				if (!rr->backup) {
					LM_ERR("all routes are off\n");
					return -1;
				} else {
					if (!rr->backup->rr) {
						LM_ERR("all routes are off\n");
						return -1;
					}
					rr = rr->backup->rr;
				}
			}
			break;
		default: return -1;
	}
	return actually_rewrite(rr, dest, msg, user);
}

/**
 * does the work for rewrite_on_rule, writes the new URI into dest
 *
 * @param rs the route rule used for rewriting
 * @param dest the returned new destination URI
 * @param msg the sip message
 * @param user the localpart of the uri to be rewritten
 *
 * @return 0 on success, -1 on failure
 *
 * @see rewrite_on_rule()
 */
static int actually_rewrite(struct route_rule *rs, str *dest, struct sip_msg *msg, str * user) {
	size_t len;
	char *p;
	int strip = 0;

	strip = (rs->strip > user->len ? user->len : rs->strip);
	strip = (strip < 0 ? 0 : strip);

	len = rs->local_prefix.len + user->len + rs->local_suffix.len +
	      AT_SIGN_LEN + rs->host.len - strip;
	if (msg->parsed_uri.type == SIPS_URI_T) {
		len += SIPS_URI_LEN;
	} else {
		len += SIP_URI_LEN;
	}
	dest->s = (char *)pkg_malloc(len + 1);
	if (dest->s == NULL) {
		LM_ERR("out of private memory.\n");
		return -1;
	}
	dest->len = len;
	p = dest->s;
	if (msg->parsed_uri.type == SIPS_URI_T) {
		memcpy(p, SIPS_URI, SIPS_URI_LEN);
		p += SIPS_URI_LEN;
	} else {
		memcpy(p, SIP_URI, SIP_URI_LEN);
		p += SIP_URI_LEN;
	}
	if (user->len) {
		memcpy(p, rs->local_prefix.s, rs->local_prefix.len);
		p += rs->local_prefix.len;
		memcpy(p, user->s + strip, user->len - strip);
		p += user->len - strip;
		memcpy(p, rs->local_suffix.s, rs->local_suffix.len);
		p += rs->local_suffix.len;
		memcpy(p, AT_SIGN, AT_SIGN_LEN);
		p += AT_SIGN_LEN;
	}
	if (rs->host.len == 0) {
		*p = '\0';
               pkg_free(dest->s);
		return -1;
	}
	memcpy(p, rs->host.s, rs->host.len);
	p += rs->host.len;
	*p = '\0';
	return 0;
}

/**
 * searches for a rule int rt with hash_index prob - 1
 * If the rule with the desired hash index is deactivated,
 * the next working rule is used.
 *
 * @param rt the routing tree node to search for rule
 * @param prob the hash index
 *
 * @return pointer to route rule on success, NULL on failure
 */
static struct route_rule * get_rule_by_hash(struct route_tree_item * rt, int prob) {
	struct route_rule * act_hash = NULL;

	if (prob > rt->rule_num) {
		LM_WARN("too large desired hash, taking highest\n");
		act_hash = rt->rules[rt->rule_num - 1];
	}
	act_hash = rt->rules[prob - 1];

	if (!act_hash->status) {
		if (act_hash->backup && act_hash->backup->rr) {
			act_hash = act_hash->backup->rr;
		} else {
			act_hash = NULL;
		}
	}
	LM_INFO("desired hash was %i, return %i\n", prob, act_hash ? act_hash->hash_index : -1);
	return act_hash;
}
