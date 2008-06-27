/*
 * $Id
 *
 * Copyright (C) 2008 Universitat Pompeu Fabra
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../socket_info.h"
#include "../../mi/tree.h"
#include "../../parser/contact/contact.h"

#include "../../sr_module.h"
#include "xmlrpc_ul_mod.h"
#include "xmlrpc_urecord.h"
#include "xmlrpc_ucontact.h"

#include "sha1.h"/*sha1*/

MODULE_VERSION

#define MAX_LOC_LEN 250

static int xmlrpc_loc_init(void);
static int xmlrpc_loc_exit(void);

/* parameters */
char *server_url = 0;
char *secret = 0;
char secret_sha1[41];
unsigned char secret_sha1_bin[20];

/*global variables*/
xmlrpc_env env;
int ul_hash_size = 9;
unsigned int init_flag = 0;
int desc_time_order = 0;
extern int ul_locks_no;

/* flag */
unsigned int nat_bflag = (unsigned int)-1;

static cmd_export_t cmds[] = {
	{"ul_bind_usrloc",        (cmd_function)xmlrpc_bind_usrloc,        1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"gateway_url", STR_PARAM, &server_url },
	{"secret", STR_PARAM, &secret },
	{"matching_mode",     INT_PARAM, &matching_mode   },
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "loc_get", xmlrpc_loc_get, 0, 0, 0},
	{ "loc_put", xmlrpc_loc_put, 0, 0, 0},
	{ 0, 0, 0, 0, 0}
};

struct module_exports exports= {
	"xmlrpc_usrloc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,  /* exported statistics */
	0,  /* exported MI functions */
	0,  /* exported pseudo-variables */
	0,  /* extra processes */
	xmlrpc_loc_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) xmlrpc_loc_exit,   /* module exit function */
	0 /* per-child init function */
};

struct mi_root* xmlrpc_loc_get(struct mi_root* cmd_tree, void *param)
{
   struct mi_node* node = NULL;
   struct mi_node* rpl = NULL;
   struct mi_root* rpl_tree= NULL;
   char* req_res;
   char holder[MAX_LOC_LEN];
   xmlrpc_value *result;

   rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
   if (rpl_tree==0)
      return NULL;
   rpl = &rpl_tree->node;
   node = cmd_tree->node.kids;
   if (node==NULL || node->value.s==NULL)
      init_mi_tree(400,MI_MISSING_PARM_S,MI_MISSING_PARM_LEN);
   if(node->value.len > (MAX_LOC_LEN-1))
      return init_mi_tree(400,"Parameter too long",sizeof("Parameter too long"));
   strncpy(holder,node->value.s,node->value.len);
   holder[node->value.len]='\0';
   result = xmlrpc_client_call(&env,server_url,"get","(s)",holder);
   die_if_fault_occurred(&env);
   xmlrpc_read_string(&env,result,&req_res);
   die_if_fault_occurred(&env);
   node = add_mi_node_child(rpl,0,"location",8,req_res,strlen(req_res) );
   if (node==0)
      goto error;
   xmlrpc_DECREF(result);
   return rpl_tree;
error:
   return NULL;
}

struct mi_root* xmlrpc_loc_put(struct mi_root* cmd_tree, void *param)
{
   struct mi_node* node = NULL;
   struct mi_node* rpl = NULL;
   struct mi_root* rpl_tree= NULL;
   char* req_res;
   char holder[MAX_LOC_LEN];
   char holder2[MAX_LOC_LEN];
   xmlrpc_value *result;

   rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
   if (rpl_tree==0)
      return NULL;
   rpl = &rpl_tree->node;
   node = cmd_tree->node.kids;
   if ((node == NULL) || 
	 (node->value.s == NULL) || 
	 (node->next == NULL) || 
	 (node->next->value.s == NULL))
      init_mi_tree(400,MI_MISSING_PARM_S,MI_MISSING_PARM_LEN);
   if(node->value.len > (MAX_LOC_LEN-1))
      return init_mi_tree(400,"Parameter too long",sizeof("Parameter too long"));
   strncpy(holder,node->value.s,node->value.len);
   holder[node->value.len]='\0';
   strncpy(holder2,node->next->value.s,node->next->value.len);
   holder2[node->next->value.len]='\0';
   result = xmlrpc_client_call(&env,server_url,"put","(ss)",holder,holder2);
   die_if_fault_occurred(&env);
   xmlrpc_read_string(&env,result,&req_res);
   die_if_fault_occurred(&env);
   node = add_mi_node_child(rpl,0,"status",6,req_res,strlen(req_res) );
   if (node==0)
      goto error;
   xmlrpc_DECREF(result);
   return rpl_tree;
error:
   return NULL;
}

void die_if_fault_occurred (xmlrpc_env *env)
{
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}

/*
   Initialization of the xmlrpc_env variable which is for error handling. This func
   is invoked previous to forking processes, so initializing once will make children 
   inherit an initialized environment which should be good.
 */
static int xmlrpc_loc_init(void)
{
   unsigned char *ptr;
   if(secret == 0){
      LM_ERR("You *must* specify a secret for storing/retrieving key-value pairs\n");
      return -1;
   }else{
      sha1(secret,strlen(secret),secret_sha1_bin);
      ptr=secret_sha1_bin;
      sprintf(secret_sha1,"%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++,*ptr++);
   }
   /* Initialize our error-handling environment. */
   xmlrpc_env_init(&env);
   if (!server_url) {
      LM_CRIT("You must specify the XML-RPC gateway if you want to use this module !!\n");
      return -1;
   }

   /* Start up our XML-RPC client library. */
   xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
   die_if_fault_occurred(&env);

   if(ul_hash_size<=1)
      ul_hash_size = 512;
   else
      ul_hash_size = 1<<ul_hash_size;
   ul_locks_no = ul_hash_size;

   /* check matching mode */
   switch (matching_mode) {
      case CONTACT_ONLY:
      case CONTACT_CALLID:
	 break;
      default:
	 LM_ERR("invalid matching mode %d\n", matching_mode);
   }

   if(ul_init_locks()!=0)
   {
      LM_ERR("locks array initialization failed\n");
      return -1;
   }

   /* Register cache timer */
   /*register_timer( timer, 0, timer_interval);*/

   /* init the callbacks list */
   if ( init_ulcb_list() < 0) {
      LM_ERR("usrloc/callbacks initialization failed\n");
      return -1;
   }

   if (nat_bflag==(unsigned int)-1) {
      nat_bflag = 0;
   } else if ( nat_bflag>=8*sizeof(nat_bflag) ) {
      LM_ERR("bflag index (%d) too big!\n", nat_bflag);
      return -1;
   } else {
      nat_bflag = 1<<nat_bflag;
   }

   init_flag=1;

   return 0;
}

static int xmlrpc_loc_exit(void)
{
   return 0;/* TODO we need some flag to check wether env has been initialized befor cleaning it !!*/
   /* Clean up our error-handling environment. */
   xmlrpc_env_clean(&env);
   /* Shutdown our XML-RPC client library. */
   xmlrpc_client_cleanup();
   return 0;
}
