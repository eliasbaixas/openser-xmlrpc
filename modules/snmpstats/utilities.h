/* 
 * $Id: utilities.h 1425 2006-12-18 22:22:51Z jmagder $
 *
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 
 * This file was created to group together utility functions that were useful
 * throughout the SNMPStats module, without belonging to any file in particular.
 */

#ifndef _SNMP_UTILITIES_
#define _SNMP_UTILITIES_

#include <time.h>

#include "../../str.h"
#include "../../sr_module.h"

/*
 * This function copies an OpenSER "str" datatype into a '\0' terminated char*
 * string. 
 *
 * NOTE: Make sure to free the memory allocated to *copiedString, when you no
 *       longer have any use for it. (It is allocated with shm_malloc(), so make
 *       sure to deallocate it with shm_free()) 
 */
int convertStrToCharString(str *strToConvert, char **copiedString);

/* Performs sanity checks on the parameters passed to a string configuration
 * file parameter handler. */
int stringHandlerSanityCheck( modparam_t type, void *val, char *parameterName);

/* 
 * This function is a wrapper around the standard statistic framework.  It will
 * return the value of the statistic denoted with statName, or zero if the
 * statistic was not found. 
 */
int get_statistic(char *statName);

/* Returns a pointer to an SNMP DateAndTime OCTET STRING representation of the
 * time structure.  Note that the pointer is to static data, so it shouldn't be
 * counted on to be around if this function is called again. */
char * convertTMToSNMPDateAndTime(struct tm *timeStructure);

#endif
