/*
 * $Id: re_group.h 386 2005-11-21 18:12:35Z bogdan_iancu $ 
 *
 * Copyright (C) 2005-2007 Voice Sistem SRL
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
 * --------
 *  2005-10-06 - created by bogdan
 */

#ifndef RE_GROUP_H
#define RE_GROUP_H

#include "../../str.h"
#include "../../usr_avp.h"
#include "../../parser/msg_parser.h"

typedef struct gid_spec {
	int     avp_type;
	int_str avp_name;
} gid_spec_t;


int load_re(str *table);

int get_user_group(struct sip_msg *req, char *user, char *avp);

#endif
