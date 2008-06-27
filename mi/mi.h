/*
 * $Id: mi.h 1356 2006-12-11 17:59:42Z bogdan_iancu $
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
 *  2006-09-08  first version (bogdan)
 */



#ifndef _MI_MI_H_
#define _MI_MI_H_

#include "../str.h"
#include "tree.h"

#define MI_ASYNC_RPL_FLAG   (1<<0)
#define MI_NO_INPUT_FLAG    (1<<1)

#define MI_ROOT_ASYNC_RPL   ((struct mi_root*)-1)

struct mi_handler;

typedef struct mi_root* (mi_cmd_f)(struct mi_root*, void *param);
typedef int (mi_child_init_f)(void);
typedef void (mi_handler_f)(struct mi_root *, struct mi_handler *, int);


struct mi_handler {
	mi_handler_f *handler_f;
	void * param;
};


struct mi_cmd {
	int id;
	str name;
	mi_child_init_f *init_f;
	mi_cmd_f *f;
	unsigned int flags;
	void *param;
};


typedef struct mi_export_ {
	char *name;
	mi_cmd_f *cmd;
	unsigned int flags;
	void *param;
	mi_child_init_f *init_f;
}mi_export_t;


int register_mi_cmd( mi_cmd_f f, char *name, void *param,
		mi_child_init_f in, unsigned int flags);

int register_mi_mod( char *mod_name, mi_export_t *mis);

int init_mi_child();

struct mi_cmd* lookup_mi_cmd( char *name, int len);

static inline struct mi_root* run_mi_cmd(struct mi_cmd *cmd, struct mi_root *t)
{
	return cmd->f( t, cmd->param);
}

void get_mi_cmds( struct mi_cmd** cmds, int *size);

#endif

