/*
 * $Id: domainpolicy_mod.h 1187 2006-10-24 09:46:29Z klaus_darilion $
 *
 * Domain module headers
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#ifndef DOMAINPOLICY_MOD_H
#define DOMAINPOLICY_MOD_H


#include "../../db/db.h"
#include "../../str.h"
#include "../../usr_avp.h"



/*
 * Module parameters variables
 */
extern str domainpolicy_table;		/* Domainpolicy table name */
extern str domainpolicy_col_rule;   	/* Rule column name */
extern str domainpolicy_col_type;   	/* Type column name */
extern str domainpolicy_col_att;   	/* Attribute column name */
extern str domainpolicy_col_val;   	/* Value column name */


/*
 * Other module variables
 */

extern int_str port_override_name, transport_override_name, 
		domain_prefix_name, domain_suffix_name, domain_replacement_name,
		send_socket_name, target_name;

extern unsigned short port_override_avp_name_str;
extern unsigned short transport_override_avp_name_str;
extern unsigned short domain_prefix_avp_name_str;
extern unsigned short domain_suffix_avp_name_str;
extern unsigned short domain_replacement_avp_name_str;
extern unsigned short send_socket_avp_name_str;


#endif /* DOMAINPOLICY_MOD_H */
