#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME "Xmlrpc-c Test Client"
#define VERSION "1.0"

static void 
die_if_fault_occurred (xmlrpc_env *env) {
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}

int 
main(int           const argc, 
     const char ** const argv ATTR_UNUSED) {

    xmlrpc_env env;
    xmlrpc_value *result;
    xmlrpc_int32 sum;
    char * const serverUrl = "http://localhost:8082/RPC2";
    char * methodName;
    char req_meth[10];
    char req_name[10];
    char req_val[10];
    char * req_res;

    if (argc-1 > 0) {
        fprintf(stderr, "This program has no arguments\n");
        exit(1);
    }

    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    /* Start up our XML-RPC client library. */
    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    die_if_fault_occurred(&env);


    while (scanf("%9s %9s %9s",req_meth,req_name,req_val) != EOF) {
       printf ("got '%s' '%s' '%s'\n",req_meth,req_name,req_val);
	if(!strcmp(req_meth,"put")){
           if((strlen(req_name)==0)||(strlen(req_val)==0)){
              printf("illegal name or value, they must have some characters !\n");
              goto error;
           }
	   printf("Making XMLRPC call to server url '%s' method '%s' "
		 "to put the key \"%s\" with value \"%s\"\n", serverUrl, req_meth,req_name,req_val);
	   result = xmlrpc_client_call(&env, serverUrl, req_meth, "(ss)", req_name, req_val);
	   die_if_fault_occurred(&env);
	   xmlrpc_read_string(&env, result,&req_res);
	   die_if_fault_occurred(&env);
	   printf("The result is '%s'\n", req_res);
	}else if(!strcmp(req_meth,"get")){
           if(strlen(req_name)==0){
              printf("illegal name, it must have some characters !\n");
              goto error;
           }
	   printf("Making XMLRPC call to server url '%s' method '%s' "
		 "to get value for key \"%s\"\n", serverUrl, req_meth,req_name);
	   result = xmlrpc_client_call(&env, serverUrl, req_meth, "(s)", req_name);
	   die_if_fault_occurred(&env);
	   printf ("received: %s type of thing \n",xmlrpc_typeName(xmlrpc_value_type(result)));
	   xmlrpc_read_string(&env, result,&req_res);
	   die_if_fault_occurred(&env);
	   printf("The response is '%s'\n", req_res);
	}else{
	   printf("Unimplemented method \"%s\"\n",req_meth);
	   goto error;
	}
    }
    /* Dispose of our result value. */
    xmlrpc_DECREF(result);
error:
    /* Clean up our error-handling environment. */
    xmlrpc_env_clean(&env);
    
    /* Shutdown our XML-RPC client library. */
    xmlrpc_client_cleanup();

    return 0;
}

