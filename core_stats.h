/*
 * $Id: core_stats.h 1364 2006-12-12 10:15:32Z bogdan_iancu $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 *
 * History:
 * ---------
 *  2006-01-23  first version (bogdan)
 *  2006-11-28  Added statistics for the number of bad URI's, methods, and 
 *              proxy requests (Jeffrey Magder - SOMA Networks)
 */

#ifndef _CORE_STATS_H_
#define _CORE_STATS_H_

#include "statistics.h"

#ifdef STATISTICS
extern stat_export_t core_stats[];

/* received requests */
extern stat_var* rcv_reqs;

/* received replies */
extern stat_var* rcv_rpls;

/* forwarded requests */
extern stat_var* fwd_reqs;

/* forwarded replies */
extern stat_var* fwd_rpls;

/* dropped requests */
extern stat_var* drp_reqs;

/* dropped replies */
extern stat_var* drp_rpls;

/* error requests */
extern stat_var* err_reqs;

/* error replies */
extern stat_var* err_rpls;

/* Set in parse_uri() */
extern stat_var* bad_URIs;

/* Set in parse_method() */
extern stat_var* unsupported_methods;

/* Set in get_hdr_field(). */
extern stat_var* bad_msg_hdr;
 
#endif /*STATISTICS*/

#endif /*_CORE_STATS_H_*/
