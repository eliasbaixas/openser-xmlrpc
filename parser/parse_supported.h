/*
 * $Id: parse_supported.h 856 2006-05-24 11:14:31Z agranig $
 *
 * Supported parser.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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
 *
 * History:
 * -------
 * 2006-03-02  parse_supported() parses and cumulates all SUPPORTED 
 *             headers (bogdan)
 */

#ifndef PARSE_SUPPORTED_H
#define PARSE_SUPPORTED_H

#include "msg_parser.h"
#include "../mem/mem.h"


#define F_SUPPORTED_PATH	(1 << 0)
#define F_SUPPORTED_100REL	(1 << 1)
#define F_SUPPORTED_TIMER	(1 << 2)

#define SUPPORTED_PATH_STR		"path"
#define SUPPORTED_PATH_LEN		(sizeof(SUPPORTED_PATH_STR)-1)

/* RFC 3262 */
#define SUPPORTED_100REL_STR	"100rel"
#define SUPPORTED_100REL_LEN	(sizeof(SUPPORTED_100REL_STR)-1)

/* RFC 4028 */
#define SUPPORTED_TIMER_STR		"timer"
#define SUPPORTED_TIMER_LEN		(sizeof(SUPPORTED_TIMER_STR)-1)


#define get_supported(p_msg) \
	((p_msg)->supported ? ((struct supported_body*)(p_msg)->supported->parsed)->supported_all : 0)


struct supported_body {
	unsigned int supported;        /* supported mask for the current hdr */
	unsigned int supported_all;    /* suppoted mask for the all "supported" hdr
	                                *  - it's set only for the first hdr in 
	                                *  sibling list*/
};


/*
 * Parse all Supported headers.
 */
int parse_supported( struct sip_msg *msg);


static inline void free_supported(struct supported_body **sb)
{
	if (sb && *sb) {
		pkg_free(*sb);
		*sb = 0;
	}
}

#endif /* PARSE_SUPPORTED_H */
