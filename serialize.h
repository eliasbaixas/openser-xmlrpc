/*
 * $Id: serialize.h 413 2005-11-29 20:33:29Z bogdan_iancu $
 *
 * sequential forking implementation
 *
 * Copyright (C) 2005 Juha Heinanen
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
 *  2005-11-29 splitted from lcr module (bogdan)
 */

#ifndef _CORE_SERIALIZE_H_
#define _CORE_SERIALIZE_H_

#include "parser/msg_parser.h"


int init_serialization();


/* converts the destination set (for parallel forking) into AVPS used
 * for serial forking.
 */
int serialize_branches(struct sip_msg *msg, int clean_before );


/* gets the next branches for serial foking
 */
int next_branches( struct sip_msg *msg );

#endif
