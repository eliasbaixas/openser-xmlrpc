/*
 * $Id: dlg_cb.c 2764 2007-09-14 09:26:24Z bogdan_iancu $
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 *  2006-04-14  initial version (bogdan)
 */


#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "dlg_hash.h"
#include "dlg_cb.h"

static struct dlg_head_cbl* create_cbs = 0;

int init_dlg_callbacks(void)
{
	create_cbs = (struct dlg_head_cbl*)shm_malloc(sizeof(struct dlg_head_cbl));
	if (create_cbs==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}
	create_cbs->first = 0;
	create_cbs->types = 0;
	return 0;
}


void destroy_dlg_callbacks_list(struct dlg_callback *cb)
{
	struct dlg_callback *cb_t;

	while(cb) {
		cb_t = cb;
		cb = cb->next;
		/* FIXME - what about parameters ? */
		shm_free(cb_t);
	}
}


void destroy_dlg_callbacks(void)
{
	if (create_cbs==0)
		return;

	destroy_dlg_callbacks_list(create_cbs->first);
	shm_free(create_cbs);
	create_cbs = 0;
}


int register_dlgcb(struct dlg_cell *dlg, int types, dialog_cb f, void *param )
{
	struct dlg_callback *cb;

	if ( types&DLGCB_CREATED ) {
		if (types!=DLGCB_CREATED) {
			LM_CRIT("DLGCB_CREATED type must be register alone!\n");
			return -1;
		}
	} else {
		if (dlg==0) {
			LM_CRIT("non-DLGCB_CREATED type "
				"must be register to a dialog (dlg missing)!\n");
			return -1;
		}
	}
	cb = (struct dlg_callback*)shm_malloc(sizeof(struct dlg_callback));
	if (cb==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}

	cb->types = types;
	cb->callback = f;
	cb->param = param;

	if ( types&DLGCB_CREATED ) {
		cb->next = create_cbs->first;
		create_cbs->first = cb;
		create_cbs->types |= types;
	} else {
		cb->next = dlg->cbs.first;
		dlg->cbs.first = cb;
		dlg->cbs.types |= types;
	}

	return 0;
}


void run_create_callbacks(struct dlg_cell *dlg, struct sip_msg *msg)
{
	struct dlg_callback *cb;

	if (create_cbs->first==0)
		return;

	for ( cb=create_cbs->first; cb; cb=cb->next)  {
		LM_DBG("dialog=%p\n",dlg);
		cb->callback( dlg, DLGCB_CREATED, msg, &cb->param );
	}
	return;
}


void run_dlg_callbacks(int type , struct dlg_cell *dlg, struct sip_msg *msg)
{
	struct dlg_callback *cb;

	if (dlg->cbs.first==0 || ((dlg->cbs.types)&type)==0 )
		return;

	for ( cb=dlg->cbs.first; cb; cb=cb->next)  {
		if ( (cb->types)&type ) {
			LM_DBG("dialog=%p, type=%d\n", dlg, type);
			cb->callback( dlg, type, msg, &cb->param );
		}
	}
	return;
}
