/*
 * $Id: sdlookup.h 1089 2006-09-06 11:05:17Z bogdan_iancu $
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 * 
 */


#ifndef _SDLOOKUP_H_
#define _SDLOOKUP_H_

#include "../../parser/msg_parser.h"

int sd_lookup(struct sip_msg* _msg, char* _table, char* _str2);

#endif /* _SDLOOKUP_H_ */
