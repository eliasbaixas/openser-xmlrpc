/*
 * $Id: notify_body.h 1337 2006-12-07 18:05:05Z bogdan_iancu $
 *
 * presence_xml module -  
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2007-04-11  initial version (anca)
 */

#ifndef _NBODY_H_
#define _NBODY_H_

str* pres_agg_nbody(str* pres_user, str* pres_domain, str** body_array,
		int n, int off_index);
int pres_apply_auth(str* notify_body, subs_t* subs, str** final_nbody);
void free_xml_body(char* body);

#endif
