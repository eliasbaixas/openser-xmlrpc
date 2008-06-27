/* $Id: nathelper.c 1808 2007-03-10 17:36:19Z bogdan_iancu $
 *
 * Copyright (C) 2003 Porta Software Ltd
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
 * ---------
 * 2007-04-13   splitted from nathelper.c (ancuta)
*/


#ifndef _NATHELPER_NATHELPER_H
#define _NATHELPER_NATHELPER_H

#include "../../str.h"

struct rtpp_node {
	unsigned int		idx;			/* overall index */
	str					rn_url;			/* unparsed, deletable */
	int					rn_umode;
	char				*rn_address;	/* substring of rn_url */
	int					rn_disabled;	/* found unaccessible? */
	unsigned			rn_weight;		/* for load balancing */
	unsigned int		rn_recheck_ticks;
	struct rtpp_node	*rn_next;
};


struct rtpp_set{
	unsigned int 		id_set;
	unsigned			weight_sum;
	unsigned int		rtpp_node_count;
	int 				set_disabled;
	unsigned int		set_recheck_ticks;
	struct rtpp_node	*rn_first;
	struct rtpp_node	*rn_last;
	struct rtpp_set     *rset_next;
};


struct rtpp_set_head{
	struct rtpp_set		*rset_first;
	struct rtpp_set		*rset_last;
};


#endif
