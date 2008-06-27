/*
 * $Id: alarm_checks.h 1425 2006-12-18 22:22:51Z jmagder $
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
 * This file groups together alarm checking and handling
 */

#ifndef _SNMPSTATS_ALARM_AGENT_
#define _SNMPSTATS_ALARM_AGENT_

#define ALARM_AGENT_FREQUENCY_IN_SECONDS 5
#define ALARM_AGENT_NAME                 "snmpstats_alarm_agent"

/* Returns the number of bytes currently waiting in the msg queue if they exceed
 * the threshold, and zero otherwise.  If threshold_to_compare_to is < 0, then
 * no check will be performed and zero always returned. */
int check_msg_queue_alarm(int threshold_to_compare_to);

/* Returns the number of active dialogs if they exceed the threshold, and zero
 * otherwise. */
int check_dialog_alarm(int threshold_to_compare_to);

/* This function will be called periodically from an OpenSER timer.  The first
 * time it is called, it will query OPENSER-MIB for configured thresholds.
 */
void run_alarm_check(unsigned int ticks, void * attr);

#endif
