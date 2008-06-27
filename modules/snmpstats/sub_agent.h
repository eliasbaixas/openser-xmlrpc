/*
 * $Id: sub_agent.h 2625 2007-08-21 16:17:18Z bogdan_iancu $
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
 * This file defines all functions required to establish a relationship with a
 * master agent.  
 */

#ifndef _SNMPSTATS_SUB_AGENT_
#define _SNMPSTATS_SUB_AGENT_

#define AGENT_PROCESS_NAME   "snmpstats_sub_agent"

/* Run the AgentX sub-agent as a separate process.  The child will
 * insulate itself from the rest of OpenSER by overriding most of signal
 * handlers. */
void agentx_child(int rank);

/* This function opens up a connection with the master agent specified in
 * the snmpstats modules configuration file */
void register_with_master_agent(char *name_to_register_under);

#endif
