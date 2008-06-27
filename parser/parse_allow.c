/*
 * $Id: parse_allow.c 2802 2007-09-21 14:11:40Z bogdan_iancu $
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

#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_allow.h"
#include "parse_methods.h"
#include "msg_parser.h"


/*
 * This method is used to parse all Allow HF body.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int parse_allow(struct sip_msg *msg)
{
	unsigned int allow;
	struct hdr_field  *hdr;
	struct allow_body *ab = 0;

	/* maybe the header is already parsed! */
	if (msg->allow && msg->allow->parsed)
		return 0;

	/* parse to the end in order to get all ALLOW headers */
	if (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->allow)
		return -1;

	/* bad luck! :-( - we have to parse them */
	allow = 0;
	for( hdr=msg->allow ; hdr ; hdr=hdr->sibling) {
		if (hdr->parsed) {
			allow |= ((struct allow_body*)hdr->parsed)->allow;
			continue;
		}

		ab = (struct allow_body*)pkg_malloc(sizeof(struct allow_body));
		if (ab == 0) {
			LM_ERR("out of pkg_memory\n");
			return -1;
		}

		if (parse_methods(&(hdr->body), &(ab->allow))!=0) {
			LM_ERR("bad allow body header\n"); 
			goto error;
		}
		ab->allow_all = 0;
		hdr->parsed = (void*)ab;
		allow |= ab->allow;
	}

	((struct allow_body*)msg->allow->parsed)->allow_all = allow;
	return 0;

error:
	if(ab!=0)
		pkg_free(ab);
	return -1;
}

