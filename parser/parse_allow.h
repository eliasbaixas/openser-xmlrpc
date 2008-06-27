/*
 * $Id: parse_allow.h 709 2006-03-02 15:56:12Z bogdan_iancu $
 *
 * Copyright (c) 2004 Juha Heinanen
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
 * History:
 * -------
 * 2006-03-02  parse_allow() parses and cumulates all ALLOW headers (bogdan)
 */

#ifndef PARSE_ALLOW_H
#define PARSE_ALLOW_H

#include "../mem/mem.h"
#include "msg_parser.h"

 
/* 
 * casting macro for accessing Allow body 
 */
#define get_allow_methods(p_msg) \
	(((struct allow_body*)(p_msg)->allow->parsed)->allow_all)


struct allow_body {
	unsigned int allow;            /* allow mask for the current hdr */
	unsigned int allow_all;        /* allow mask for the all allow hdr - it's
	                                * set only for the first hdr in sibling
	                                * list*/
};

/*
 * Parse all Allow HFs
 */
int parse_allow( struct sip_msg *msg);


static inline void free_allow(struct allow_body **ab)
{
	if (ab && *ab) {
		pkg_free(*ab);
		*ab = 0;
	}
}

#endif /* PARSE_ALLOW_H */
