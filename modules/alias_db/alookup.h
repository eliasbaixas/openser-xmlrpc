/* 
 * $Id: alookup.h 1088 2006-09-06 10:53:32Z bogdan_iancu $
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for openser, a free SIP server.
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
 * 2004-09-01: first version (ramona)
 */


#ifndef _ALOOKUP_H_
#define _ALOOKUP_H_

#include "../../parser/msg_parser.h"

int alias_db_lookup(struct sip_msg* _msg, char* _table, char* _str2);

#endif /* _ALOOKUP_H_ */
