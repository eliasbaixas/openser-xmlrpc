/*
 * $Id: mi_core.c 3167 2007-11-19 10:36:08Z bogdan_iancu $
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
 * history:
 * ---------
 *  2006-09-08  first version (bogdan)
 */


#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include "../dprint.h"
#include "../globals.h"
#include "../ut.h"
#include "../pt.h"
#include "../mem/mem.h"
#include "mi.h"


static time_t up_since;
static str    up_since_ctime;

static int init_mi_uptime(void)
{
	char *p;

	time(&up_since);
	p = ctime(&up_since);
	up_since_ctime.len = strlen(p)-1;
	up_since_ctime.s = (char*)pkg_malloc(up_since_ctime.len);
	if (up_since_ctime.s==0) {
		LM_ERR("no more pkg mem\n");
		return -1;
	}
	memcpy(up_since_ctime.s, p , up_since_ctime.len);
	return 0;
}


static struct mi_root *mi_uptime(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	time_t now;
	char   *p;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	time(&now);
	p = ctime(&now);
	node = add_mi_node_child( rpl, MI_DUP_VALUE, MI_SSTR("Now"),
		p, strlen(p)-1);
	if (node==0)
		goto error;

	node = add_mi_node_child( rpl, 0, MI_SSTR("Up since"),
		up_since_ctime.s, up_since_ctime.len);
	if (node==0)
		goto error;

	node = addf_mi_node_child( rpl, 0, MI_SSTR("Up time"),
		"%lu [sec]", (unsigned long)difftime(now, up_since) );
	if (node==0)
		goto error;

	return rpl_tree;
error:
	LM_ERR("failed to add node\n");
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_version(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	node = add_mi_node_child( rpl, 0, MI_SSTR("Server"), SERVER_HDR+8,
		SERVER_HDR_LEN-8);
	if (node==0) {
		LM_ERR("failed to add node\n");
		free_mi_tree(rpl_tree);
		return 0;
	}

	return rpl_tree;
}



static struct mi_root *mi_pwd(struct mi_root *cmd, void *param)
{
	static int max_len = 0;
	static char *cwd_buf = 0;
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;

	if (cwd_buf==NULL) {
		max_len = pathmax();
		cwd_buf = pkg_malloc(max_len);
		if (cwd_buf==NULL) {
			LM_ERR("no more pkg mem\n");
			return 0;
		}
	}

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	if (getcwd(cwd_buf, max_len)==0) {
		LM_ERR("getcwd failed = %s\n",strerror(errno));
		goto error;
	}

	node = add_mi_node_child( rpl, 0, MI_SSTR("WD"), cwd_buf,strlen(cwd_buf));
	if (node==0) {
		LM_ERR("failed to add node\n");
		goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_arg(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	int n;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for ( n=0; n<my_argc ; n++ ) {
		node = add_mi_node_child(rpl, 0, 0, 0, my_argv[n], strlen(my_argv[n]));
		if (node==0) {
			LM_ERR("failed to add node\n");
			free_mi_tree(rpl_tree);
			return 0;
		}
	}

	return rpl_tree;
}



static struct mi_root *mi_which(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_cmd  *cmds;
	struct mi_node *rpl;
	struct mi_node *node;
	int size;
	int i;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	get_mi_cmds( &cmds, &size);
	for ( i=0 ; i<size ; i++ ) {
		node = add_mi_node_child( rpl, 0, 0, 0, cmds[i].name.s,
			cmds[i].name.len);
		if (node==0) {
			LM_ERR("failed to add node\n");
			free_mi_tree(rpl_tree);
			return 0;
		}
	}

	return rpl_tree;
}



static struct mi_root *mi_ps(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	struct mi_attr *attr;
	char *p;
	int len;
	int i;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for ( i=0 ; i<counted_processes ; i++ ) {
		node = add_mi_node_child(rpl, 0, MI_SSTR("Process"), 0, 0 );
		if (node==0)
			goto error;

		p = int2str((unsigned long)i, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, MI_SSTR("ID"), p, len);
		if (attr==0)
			goto error;

		p = int2str((unsigned long)pt[i].pid, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, MI_SSTR("PID"), p, len);
		if (attr==0)
			goto error;

		attr = add_mi_attr( node, 0, MI_SSTR("Type"),
			pt[i].desc, strlen(pt[i].desc));
		if (attr==0)
			goto error;
	}

	return rpl_tree;
error:
	LM_ERR("failed to add node\n");
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_kill(struct mi_root *cmd, void *param)
{
	kill(0, SIGTERM);

	return 0;
}



static struct mi_root *mi_debug(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node;
	char *p;
	int len;
	int new_debug;

#ifdef CHANGEABLE_DEBUG_LEVEL
	node = cmd->node.kids;
	if (node!=NULL) {
		if (str2sint( &node->value, &new_debug) < 0)
			return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
	} else
		new_debug = *debug;
#else
		new_debug = debug;
#endif

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;

	p = sint2str((long)new_debug, &len);
	node = add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE,
		MI_SSTR("DEBUG"),p, len);
	if (node==0) {
		free_mi_tree(rpl_tree);
		return 0;
	}

#ifdef CHANGEABLE_DEBUG_LEVEL
	*debug = new_debug;
#endif

	return rpl_tree;
}



static mi_export_t mi_core_cmds[] = {
	{ "uptime",   mi_uptime,   MI_NO_INPUT_FLAG,  0,  init_mi_uptime },
	{ "version",  mi_version,  MI_NO_INPUT_FLAG,  0,  0 },
	{ "pwd",      mi_pwd,      MI_NO_INPUT_FLAG,  0,  0 },
	{ "arg",      mi_arg,      MI_NO_INPUT_FLAG,  0,  0 },
	{ "which",    mi_which,    MI_NO_INPUT_FLAG,  0,  0 },
	{ "ps",       mi_ps,       MI_NO_INPUT_FLAG,  0,  0 },
	{ "kill",     mi_kill,     MI_NO_INPUT_FLAG,  0,  0 },
	{ "debug",    mi_debug,                   0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};



int init_mi_core(void)
{
	if (register_mi_mod( "core", mi_core_cmds)<0) {
		LM_ERR("unable to register core MI cmds\n");
		return -1;
	}

	return 0;
}
