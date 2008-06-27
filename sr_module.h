/* $Id: sr_module.h 2845 2007-10-04 11:21:22Z miconda $
 *
 * modules/plug-in structures declarations
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
 *  2003-03-10  changed module exports interface: added struct cmd_export
 *               and param_export (andrei)
 *  2003-03-16  Added flags field to cmd_export_ (janakj)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2004-03-12  extra flag USE_FUNC_PARAM added to modparam type -
 *              instead of copying the param value, a func is called (bogdan)
 *  2004-09-19  switched to version.h for the module versions checks (andrei)
 *  2004-12-03  changed param_func_t to (modparam_t, void*), killed
 *               param_func_param_t   (andrei)
 *  2006-03-02  added find_cmd_export_t(), killed find_exportp() (bogdan)
 *  2006-11-28  added module_loaded() (Jeffrey Magder - SOMA Networks)
 */


#ifndef sr_module_h
#define sr_module_h

#include <dlfcn.h>

#include "parser/msg_parser.h" /* for sip_msg */
#include "statistics.h"
#include "mi/mi.h"
#include "pvar.h"
#include "version.h"
#include "route.h"

typedef  struct module_exports* (*module_register)();
typedef  int (*cmd_function)(struct sip_msg*, char*, char*);
typedef  int (*fixup_function)(void** param, int param_no);
typedef  int (*free_fixup_function)(void** param, int param_no);
typedef  int (*response_function)(struct sip_msg*);
typedef void (*destroy_function)();
typedef int (*init_function)(void);
typedef int (*child_init_function)(int rank);


#define STR_PARAM        (1U<<0)  /* String parameter type */
#define INT_PARAM        (1U<<1)  /* Integer parameter type */
#define USE_FUNC_PARAM   (1U<<(8*sizeof(int)-1))
#define PARAM_TYPE_MASK(_x)   ((_x)&(~USE_FUNC_PARAM))

typedef unsigned int modparam_t;

typedef int (*param_func_t)( modparam_t type, void* val);

typedef void (*mod_proc)(int no);

typedef int (*mod_proc_wrapper)();

/* Macros - used as rank in child_init function */
#define PROC_MAIN      0  /* Main openser process */
#define PROC_TIMER    -1  /* Timer attendant process */
#define PROC_TCP_MAIN -4  /* TCP main process */

#define DEFAULT_DLFLAGS	0 /* value that signals to module loader to
							use default dlopen flags in openser */
#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif

#define OPENSER_DLFLAGS	RTLD_NOW

#define MODULE_VERSION \
	char *module_version=OPENSER_FULL_VERSION; \
	char *module_flags=OPENSER_COMPILE_FLAGS;


struct cmd_export_ {
	char* name;             /* null terminated command name */
	cmd_function function;  /* pointer to the corresponding function */
	int param_no;           /* number of parameters used by the function */
	fixup_function fixup;   /* pointer to the function called to "fix" the
							   parameters */
	free_fixup_function
				free_fixup; /* pointer to the function called to free the
							   "fixed" parameters */
	int flags;              /* Function flags */
};


struct param_export_ {
	char* name;             /* null terminated param. name */
	modparam_t type;        /* param. type */
	void* param_pointer;    /* pointer to the param. memory location */
};


struct proc_export_ {
	char *name;
	mod_proc_wrapper pre_fork_function;
	mod_proc_wrapper post_fork_function;
	mod_proc function;
	unsigned int no;
};


typedef struct cmd_export_ cmd_export_t;
typedef struct param_export_ param_export_t;
typedef struct proc_export_ proc_export_t;

struct module_exports{
	char* name;                     /* null terminated module name */
	unsigned int dlflags;           /* flags for dlopen */
	
	cmd_export_t* cmds;             /* null terminated array of the exported
	                                   commands */
	param_export_t* params;         /* null terminated array of the exported
	                                   module parameters */

	stat_export_t* stats;           /* null terminated array of the exported
	                                   module statistics */

	mi_export_t* mi_cmds;           /* null terminated array of the exported
	                                   MI functions */

	pv_export_t* items;             /* null terminated array of the exported
	                                   module items (pseudo-variables) */

	proc_export_t* procs;           /* null terminated array of the additional
	                                   processes reqired by the module */

	init_function init_f;           /* Initialization function */
	response_function response_f;   /* function used for responses,
	                                   returns yes or no; can be null */
	destroy_function destroy_f;     /* function called when the module should
	                                   be "destroyed", e.g: on openser exit */
	child_init_function init_child_f;/* function called by all processes
	                                    after the fork */
};





struct sr_module{
	char* path;
	void* handle;
	struct module_exports* exports;
	struct sr_module* next;
};


struct sr_module* modules; /* global module list*/

int register_builtin_modules();
int register_module(struct module_exports*, char*,  void*);
int sr_load_module(char* path);
cmd_export_t* find_cmd_export_t(char* name, int param_no, int flags);
cmd_function find_export(char* name, int param_no, int flags);
cmd_function find_mod_export(char* mod, char* name, int param_no, int flags);
void destroy_modules();
int init_child(int rank);
int init_modules(void);

/*
 * Find a parameter with given type and return it's
 * address in memory
 * If there is no such parameter, NULL is returned
 */
void* find_param_export(char* mod, char* name, modparam_t type);

/* modules function prototypes:
 * struct module_exports* mod_register(); (type module_register)
 * int   foo_cmd(struct sip_msg* msg, char* param);
 *  - returns >0 if ok , <0 on error, 0 to stop processing (==DROP)
 * int   response_f(struct sip_msg* msg)
 *  - returns >0 if ok, 0 to drop message
 */

/* Returns 1 if the module with name 'name' is loaded, and zero otherwise. */
int module_loaded(char *name);

/* Counts the additional the number of processes requested by modules */
int count_module_procs();

/* Forks and starts the additional processes required by modules */
int start_module_procs();


#endif
