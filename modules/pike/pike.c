/*
 * $Id: pike.c 2845 2007-10-04 11:21:22Z miconda $
 *
 * PIKE module
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
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-11  converted to the new locking interface: locking.h --
 *               major changes (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "../../locking.h"
#include "ip_tree.h"
#include "timer.h"
#include "pike_mi.h"
#include "pike_funcs.h"

MODULE_VERSION



static int pike_init(void);
static int pike_exit(void);



/* parameters */
static int time_unit = 2;
static int max_reqs  = 30;
int timeout   = 120;

/* global variables */
gen_lock_t*             timer_lock=0;
struct list_link*       timer = 0;


static cmd_export_t cmds[]={
	{"pike_check_req",  pike_check_req,  0,  0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sampling_time_unit",    INT_PARAM,  &time_unit},
	{"reqs_density_per_unit", INT_PARAM,  &max_reqs},
	{"remove_latency",        INT_PARAM,  &timeout},
	{0,0,0}
};


static mi_export_t mi_cmds [] = {
	{MI_PIKE_LIST,   mi_pike_list,   MI_NO_INPUT_FLAG,  0,  0 },
	{0,0,0,0,0}
};


struct module_exports exports= {
	"pike",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	0,           /* exported pseudo-variables */
	0,           /* extra processes */
	pike_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) pike_exit,   /* module exit function */
	0  /* per-child init function */
};




static int pike_init(void)
{
	LM_INFO("initializing...\n");

	/* alloc the timer lock */
	timer_lock=lock_alloc();
	if (timer_lock==0) {
		LM_ERR(" alloc locks failed!\n");
		goto error1;
	}
	/* init the lock */
	if (lock_init(timer_lock)==0){
		LM_ERR(" init lock failed\n");
		goto error1;
	}

	/* init the IP tree */
	if ( init_ip_tree(max_reqs)!=0 ) {
		LM_ERR(" ip_tree creation failed!\n");
		goto error2;
	}

	/* init timer list */
	timer = (struct list_link*)shm_malloc(sizeof(struct list_link));
	if (timer==0) {
		LM_ERR(" cannot alloc shm mem for timer!\n");
		goto error3;
	}
	timer->next = timer->prev = timer;

	/* registering timing functions  */
	register_timer( clean_routine , 0, 1 );
	register_timer( swap_routine , 0, time_unit );

	return 0;
error3:
	destroy_ip_tree();
error2:
	lock_destroy(timer_lock);
error1:
	if (timer_lock) lock_dealloc(timer_lock);
	timer_lock = 0;
	return -1;
}



static int pike_exit(void)
{
	LM_INFO("destroying...\n");

	/* destroy semaphore */
	if (timer_lock) {
		lock_destroy(timer_lock);
		lock_dealloc(timer_lock);
	}

	/* empty the timer list head */
	if (timer) {
		shm_free(timer);
		timer = 0;
	}

	/* destroy the IP tree */
	destroy_ip_tree();

	return 0;
}


