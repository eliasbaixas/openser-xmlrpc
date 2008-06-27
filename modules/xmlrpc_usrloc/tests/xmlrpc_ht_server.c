/* A simple standalone XML-RPC server written in C. */

#include <stdlib.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <limits.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */

#include <string.h>/* for strcmp*/
#include <search.h>/* the libstdc hashtable*/

#define LOC_MAX_ENTRIES 5000
#define LOC_ENTRY_LEN_MAX 100

char locations[LOC_MAX_ENTRIES][LOC_ENTRY_LEN_MAX];
int loc_idx = 0;

static xmlrpc_value *
ht_get(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

   char *what;
   ENTRY item;
   ENTRY *found_item;

   xmlrpc_decompose_value(env, param_array, "(s)", &what);

   if (env->fault_occurred)
      return NULL;
   item.key=what;
   if (NULL != (found_item = hsearch(item, FIND))) {
      return xmlrpc_build_value(env, "s", (char *)found_item->data);
   }else
      return xmlrpc_build_value(env, "s", "ERROR");
}

static xmlrpc_value *
ht_put(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

   char *key,*what;
   ENTRY item;
   ENTRY *found_item;

   xmlrpc_decompose_value(env, param_array, "(ss)", &key, &what);

   if (env->fault_occurred)
      return NULL;
   item.key=strdup(key);
   strcpy(locations[loc_idx],what);
   item.data = &locations[loc_idx];
   loc_idx++;
   if (hsearch(item, ENTER)) {
      return xmlrpc_build_value(env, "s", "OK");
   }else
      return NULL;
}

int 
main(int           const argc, 
     const char ** const argv) {

    xmlrpc_server_abyss_parms serverparm;
    xmlrpc_registry * registryP;
    xmlrpc_env env;


    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port "
                "number on which the server will accept connections "
                "for RPCs.  You specified %d arguments.\n",  argc-1);
        exit(1);
    }
    
    xmlrpc_env_init(&env);
    hcreate(LOC_MAX_ENTRIES);
    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method(
        &env, registryP, NULL, "get", &ht_get, NULL);
    xmlrpc_registry_add_method(
        &env, registryP, NULL, "put", &ht_put, NULL);
    /* In the modern form of the Abyss API, we supply parameters in memory
       like a normal API.  We select the modern form by setting
       config_file_name to NULL: 
    */
    serverparm.config_file_name = NULL;
    serverparm.registryP        = registryP;
    serverparm.port_number      = atoi(argv[1]);
    serverparm.log_file_name    = "/tmp/xmlrpc_log";

    printf("Running XML-RPC server...\n");

    xmlrpc_server_abyss(&env, &serverparm, XMLRPC_APSIZE(log_file_name));

    /* xmlrpc_server_abyss() never returns */

    return 0;
}
