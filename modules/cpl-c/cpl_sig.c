/*
 * $Id: cpl_sig.c 3187 2007-11-21 19:44:45Z bogdan_iancu $
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


#include "../../action.h"
#include "../../dset.h"
#include "../tm/tm_load.h"
#include "loc_set.h"
#include "cpl_sig.h"
#include "cpl_env.h"


/* forwards the msg to the given location set; if flags has set the
 * CPL_PROXY_DONE, all locations will be added as branches, otherwise, the 
 * first one will set as RURI (this is ha case when this is the first proxy 
 * of the message)
 * The given list of location will be freed, returning 0 instead.
 * Returns:  0 - OK
 *          -1 - error */
int cpl_proxy_to_loc_set( struct sip_msg *msg, struct location **locs,
													unsigned char flag)
{
	struct location *foo;
	struct action act;
	int bflags;

	if (!*locs) {
		LM_ERR("empty loc set!!\n");
		goto error;
	}

	/* if it's the first time when this sip_msg is proxied, use the first addr
	 * in loc_set to rewrite the RURI */
	if (!(flag&CPL_PROXY_DONE)) {
		LM_DBG("rewriting Request-URI with <%s>\n",(*locs)->addr.uri.s);
		/* build a new action for setting the URI */
		act.type = SET_URI_T;
		act.elem[0].type = STRING_ST;
		act.elem[0].u.string = (*locs)->addr.uri.s;
		act.next = 0;
		/* push the action */
		if (do_action(&act, msg) < 0) {
			LM_ERR("do_action failed\n");
			goto error;
		}
		/* build a new action for setting the DSTURI */
		if((*locs)->addr.received.s && (*locs)->addr.received.len) {
			LM_DBG("rewriting Destination URI "
				"with <%s>\n",(*locs)->addr.received.s);
			act.type = SET_DSTURI_T;
			act.elem[0].type = STRING_ST;
			act.elem[0].u.s = (*locs)->addr.received;
			act.next = 0;
			/* push the action */
			if (do_action(&act, msg) < 0) {
				LM_ERR("do_action failed\n");
				goto error;
			}
		}
		/* is the location NATED? */
		if ((*locs)->flags&CPL_LOC_NATED)
			setbflag( 0, cpl_fct.ulb.nat_flag );
		/* free the location and point to the next one */
		foo = (*locs)->next;
		free_location( *locs );
		*locs = foo;
	}

	/* add the rest of the locations as branches */
	while(*locs) {
		bflags = ((*locs)->flags&CPL_LOC_NATED) ? cpl_fct.ulb.nat_flag : 0 ;
		LM_DBG("appending branch <%.*s>, flags %d\n",
			(*locs)->addr.uri.len, (*locs)->addr.uri.s, bflags);
		if(append_branch(msg, &(*locs)->addr.uri, &(*locs)->addr.received,0,
		Q_UNSPECIFIED, bflags, 0)==-1){
			LM_ERR("failed when appending branch <%s>\n",(*locs)->addr.uri.s);
			goto error;
		}
		/* free the location and point to the next one */
		foo = (*locs)->next;
		free_location( *locs );
		*locs = foo;
	}

	/* run what proxy route is set */
	if (cpl_env.proxy_route) {
		/* do not alter route type - it might be REQUEST or FAILURE */
		run_top_route( rlist[cpl_env.proxy_route], msg);
	}

	/* do t_forward */
	if (cpl_fct.tmb.t_relay(msg, 0, 0)==-1) {
		LM_ERR("t_relay failed !\n");
		goto error;
	}

	return 0;
error:
	return -1;
}


