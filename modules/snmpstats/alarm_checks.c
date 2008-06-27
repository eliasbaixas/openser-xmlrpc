/*
 * $Id: alarm_checks.c 1425 2006-12-18 22:22:51Z jmagder $
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

#include <signal.h>

#include "alarm_checks.h"
#include "sub_agent.h"
#include "network_stats.h"
#include "utilities.h"
#include "openserObjects.h"
#include "openserMIBNotifications.h"

/* Returns the number of bytes currently waiting in the msg queue if they exceed
 * the threshold, and zero otherwise.  If threshold_to_compare_to is < 0, then
 * no check will be performed and zero always returned. */
int check_msg_queue_alarm(int threshold_to_compare_to) 
{
	int bytesWaiting = 0;

	if (threshold_to_compare_to < 0)
	{
		return 0;
	}
	
	bytesWaiting = get_total_bytes_waiting(); 

	if (bytesWaiting > threshold_to_compare_to)
	{
		return bytesWaiting;
	}

	return 0;
}


/* Returns the number of active dialogs if they exceed the threshold, and zero
 * otherwise. */
int check_dialog_alarm(int threshold_to_compare_to) 
{
	int num_dialogs;

	if (threshold_to_compare_to < 0) 
	{
		return 0;
	}

	num_dialogs = get_statistic("active_dialogs");

	if (num_dialogs > threshold_to_compare_to) 
	{
		return num_dialogs;
	}

	return 0;
}

/* This function will be called periodically from an OpenSER timer.  The first
 * time it is called, it will query OPENSER-MIB for configured thresholds.
 */
void run_alarm_check(unsigned int ticks, void * attr) 
{
	static int msg_queue_minor_threshold;
	static int msg_queue_major_threshold;		

	static int dialog_minor_threshold;
	static int dialog_major_threshold;

	static char firstRun = 1;
	
	int bytesInMsgQueue;
	int numActiveDialogs;

	/* We only need to retrieve our thresholds the first time around */
	if (firstRun) 
	{
		register_with_master_agent(ALARM_AGENT_NAME);

		msg_queue_minor_threshold = get_msg_queue_minor_threshold();
		msg_queue_major_threshold = get_msg_queue_major_threshold();

		dialog_minor_threshold = get_dialog_minor_threshold();
		dialog_major_threshold = get_dialog_major_threshold();

		firstRun = 0;
	}
	
	/* We need to have this here in case the master agent fails and is
	 * restarted.  Without it, we won't be able to re-establish or AgentX
	 * connection */
	agent_check_and_process(0); 

	/* Check for MsgQueue alarm conditions */

	/* The retrieved number of bytes will be zero unless
	 * there is an alarm condition.  In this case the number
	 * of bytes will be returned. */
	bytesInMsgQueue = check_msg_queue_alarm(msg_queue_minor_threshold);

	if (bytesInMsgQueue != 0) 
	{
		send_openserMsgQueueDepthMinorEvent_trap(bytesInMsgQueue, 
						msg_queue_minor_threshold);
	}

	bytesInMsgQueue = check_msg_queue_alarm(msg_queue_major_threshold);


	if (bytesInMsgQueue != 0) 
	{
		send_openserMsgQueueDepthMajorEvent_trap(bytesInMsgQueue, 
						msg_queue_major_threshold);
	}

	/* Check for Dialog alarm conditions: */

	numActiveDialogs = 	check_dialog_alarm(dialog_minor_threshold);

	if (numActiveDialogs != 0)
	{
		send_openserDialogLimitMinorEvent_trap(numActiveDialogs,
						dialog_minor_threshold);
	}
	
	numActiveDialogs = check_dialog_alarm(dialog_major_threshold);

	if (numActiveDialogs != 0)
	{
		send_openserDialogLimitMajorEvent_trap(numActiveDialogs,
						dialog_major_threshold);
	}
}


