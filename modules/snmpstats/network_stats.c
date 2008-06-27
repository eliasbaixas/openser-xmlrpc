/* 
 * $Id: network_stats.c 2943 2007-10-19 15:15:01Z anca_vamanu $
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
 * This file groups together functions for retrieving tcp and udp queue usage
 * information from the proc file system.  
 *
 * get_total_bytes_waiting() is the only externally accessible function.  It
 * relies on the other mostly implementation dependant, file scoped functions.
 * Currently, a value other than zero will be returned on only those systems 
 * supporting the /proc file system.  (ie. Linux). 
 *
 * Others are welcome to contribute by defining new private functions that allow
 * this statistic to be collected on other platforms.  For example,
 * get_used_waiting_queue() relies on the existence of /proc/net/udp and 
 * /proc/net/tcp.  A new function to replace this is a likely candidate.
 *
 * The private functions in this file make extensive use of ip address arrays,
 * which are detailed in get_socket_list_from_proto() in this file.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "network_stats.h"
#include "snmpstats_globals.h"

#include "../../socket_info.h"
#include "../../mem/mem.h"

/*
 * This function will retrieve a list of all ip addresses and ports that OpenSER
 * is listening on, with respect to the transport protocol specified with
 * 'protocol'. 
 *
 * The first parameter, ipList, is a pointer to a pointer. It will be assigned a
 * new block of memory holding the IP Addresses and ports being listened to with
 * respect to 'protocol'.  The array maps a 2D array into a 1 dimensional space,
 * and is layed out as follows:
 *
 * The first NUM_IP_OCTETS indices will be the IP address, and the next index
 * the port.  So if NUM_IP_OCTETS is equal to 4 and there are two IP addresses
 * found, then:
 *
 *  - ipList[0] will be the first octet of the first ip address
 *  - ipList[3] will be the last octet of the first ip address.
 *  - iplist[4] will be the port of the first ip address
 *  - 
 *  - iplist[5] will be the first octet of the first ip address, 
 *  - and so on.  
 *
 * The function will return the number of sockets which were found.  This can be
 * used to index into ipList.
 *
 * NOTE: This function assigns a block of memory equal to:
 *
 *            returnedValue * (NUM_IP_OCTETS + 1) * sizeof(int);
 *
 *       Therefore it is CRUCIAL that you free ipList when you are done with its
 *       contents, to avoid a nasty memory leak.
 */
int get_socket_list_from_proto(int **ipList, int protocol) {

	struct socket_info  *si;
	struct socket_info** list;

	int num_ip_octets   = 4;
	int numberOfSockets = 0;
	int currentRow      = 0;

	/* I hate to use #ifdefs, but this is necessary because of the way 
	 * get_sock_info_list() is defined.  */
#ifndef USE_TCP
	if (protocol == PROTO_TCP) 
	{
		return 0;
	}
#endif

#ifndef USE_TLS
	if (protocol == PROTO_TLS)
	{
		return 0;
	}
#endif

	/* Retrieve the list of sockets with respect to the given protocol. */
	list=get_sock_info_list(protocol);

	/* Find out how many sockets are in the list.  We need to know this so
	 * we can malloc an array to assign to ipList. */
	for(si=list?*list:0; si; si=si->next){
		/* We only support IPV4 at this point. */
		if (si->address.af == AF_INET) {
			numberOfSockets++;
		}
	}

	/* There are no open sockets with respect to the given protocol. */
	if (numberOfSockets == 0)
	{
		return 0;
	}

	*ipList = pkg_malloc(numberOfSockets * (num_ip_octets + 1) * sizeof(int));

	/* We couldn't allocate memory for the IP List.  So all we can do is
	 * fail. */
	if (*ipList == NULL) {
		LM_ERR("no more pkg memory");
		return 0;
	}


	/* We need to search the list again.  So find the front of the list. */
	list=get_sock_info_list(protocol);

	/* Extract out the IP Addresses and ports.  */
	for(si=list?*list:0; si; si=si->next){

		/* We currently only support IPV4. */
		if (si->address.af != AF_INET) {
			continue;
		}

		(*ipList)[currentRow*(num_ip_octets + 1)  ] = 
			si->address.u.addr[0];
		(*ipList)[currentRow*(num_ip_octets + 1)+1] = 
			si->address.u.addr[1];
		(*ipList)[currentRow*(num_ip_octets + 1)+2] = 
			si->address.u.addr[2];
		(*ipList)[currentRow*(num_ip_octets + 1)+3] = 
			si->address.u.addr[3];
		(*ipList)[currentRow*(num_ip_octets + 1)+4] = 
			si->port_no;
		
		currentRow++;
	}

	return numberOfSockets;
}

/* 
 * Takes a 'line' (from the proc file system), parses out the ipAddress,
 * address, and stores the number of bytes waiting in 'rx_queue'
 *
 * Returns 1 on success, and 0 on a failed parse.
 *
 * Note: The format of ipAddress is as defined in the comments of
 * get_socket_list_from_proto() in this file. 
 *
 */
static int parse_proc_net_line(char *line, int *ipAddress, int *rx_queue) 
{
	int i;

	int ipOctetExtractionMask = 0xFF;

	char *currColonLocation;
	char *nextNonNumericalChar;
	char *currentLocationInLine = line;

	int parsedInteger[4];

	/* Example line from /proc/net/tcp or /proc/net/udp:
	 *
	 *	sl  local_address rem_address   st tx_queue rx_queue  
	 *	21: 5A0A0B0A:CAC7 1C016E0A:0016 01 00000000:00000000
	 *
	 * Algorithm:
	 *
	 * 	1) Find the location of the first  ':'
	 * 	2) Parse out the IP Address into an integer
	 * 	3) Find the location of the second ':'
	 * 	4) Parse out the port number.
	 * 	5) Find the location of the fourth ':'
	 * 	6) Parse out the rx_queue.
	 */

	for (i = 0; i < 4; i++) {

		currColonLocation = strchr(currentLocationInLine, ':'); 

		/* We didn't find all the needed ':', so fail. */
		if (currColonLocation == NULL) {
			return 0;
		}

		/* Parse out the integer, keeping the location of the next 
		 * non-numerical character.  */
		parsedInteger[i] = 
			(int) strtol(++currColonLocation, &nextNonNumericalChar,
					16);

		/* strtol()'s specifications specify that the second parameter
		 * is set to the first parameter when a number couldn't be
		 * parsed out.  This means the parse was unsuccesful.  */
		if (nextNonNumericalChar == currColonLocation) {
			return 0;
		}
		
		/* Reset the currentLocationInLine to the last non-numerical 
		 * character, so that next iteration of this loop, we can find
		 * the next colon location. */
		currentLocationInLine = nextNonNumericalChar;

	}

	/* Extract out the segments of the IP Address.  They are stored in
	 * reverse network byte order. */
	for (i = 0; i < NUM_IP_OCTETS; i++) {
		
		ipAddress[i] = 
			parsedInteger[0] & (ipOctetExtractionMask << i*8); 

		ipAddress[i] >>= i*8;

	}

	ipAddress[NUM_IP_OCTETS] = parsedInteger[1];

	*rx_queue = parsedInteger[3];
	
	return 1;
 
}


/*
 * Returns 1 if ipOne was found in ipArray, and 0 otherwise. 
 *
 * The format of ipOne and ipArray are described in the comments of 
 * get_socket_list_from_proto() in this file.
 *
 * */
static int match_ip_and_port(int *ipOne, int *ipArray, int sizeOf_ipArray) 
{
	int curIPAddrIdx;
	int curOctetIdx;
	int ipArrayIndex;

	/* Loop over every IP Address */
	for (curIPAddrIdx = 0; curIPAddrIdx < sizeOf_ipArray; curIPAddrIdx++) {

		/* Check for octets that don't match.  If one is found, skip the
		 * rest.  */
		for (curOctetIdx = 0; curOctetIdx < NUM_IP_OCTETS + 1; curOctetIdx++) {
			
			/* We've encoded a 2D array as a 1D array.  So find out
			 * our position in the 1D array. */
			ipArrayIndex = 
				curIPAddrIdx * (NUM_IP_OCTETS + 1) + curOctetIdx;

			if (ipOne[curOctetIdx] != ipArray[ipArrayIndex]) {
				break;
			}
		}

		/* If the index from the inner loop is equal to NUM_IP_OCTETS
		 * + 1, then that means that every octet (and the port with the
		 * + 1) matched. */
		if (curOctetIdx == NUM_IP_OCTETS + 1) {
			return 1;
		}

	}

	return 0;
}


/*
 * Returns the number of bytes waiting to be consumed on the network interfaces
 * assigned the IP Addresses specified in interfaceList.  The check will be
 * limited to the TCP or UDP transport exclusively.  Specifically:
 *
 * - If forTCP is non-zero, the check involves only the TCP transport.
 * - if forTCP is zero, the check involves only the UDP transport.
 *
 * Note: This only works on linux systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned. 
 */
static int get_used_waiting_queue(
		int forTCP, int *interfaceList, int listSize) 
{
	FILE *fp;
	char *fileToOpen;
	
	char lineBuffer[MAX_PROC_BUFFER];
	int  ipAddress[NUM_IP_OCTETS+1];
	int  rx_queue;
	
	int  waitingQueueSize = 0;

	/* Set up the file we want to open. */
	if (forTCP) {
		fileToOpen = "/proc/net/tcp";
	} else {
		fileToOpen = "/proc/net/udp";
	}
	
	fp = fopen(fileToOpen, "r");

	if (fp == NULL) {
		LM_ERR("Could not open %s. openserMsgQueu eDepth and its related"
				" alarms will not be available.\n", fileToOpen);
		return 0;
	}

	/* Read in every line of the file, parse out the ip address, port, and
	 * rx_queue, and compare to our list of interfaces we are listening on.
	 * Add up rx_queue for those lines which match our known interfaces. */
	while (fgets(lineBuffer, MAX_PROC_BUFFER, fp)!=NULL) {

		/* Parse out the ip address, port, and rx_queue. */
		if(parse_proc_net_line(lineBuffer, ipAddress, &rx_queue)) {

			/* Only add rx_queue if the line just parsed corresponds 
			 * to an interface we are listening on.  We do this
			 * check because it is possible that this system has
			 * other network interfaces that OpenSER has been told
			 * to ignore. */
			if (match_ip_and_port(ipAddress, interfaceList, listSize)) {
				waitingQueueSize += rx_queue;
			}
		}
	}

	fclose(fp);

	return waitingQueueSize;
}

/*
 * Returns the sum of the number of bytes waiting to be consumed on all network
 * interfaces and transports that OpenSER is listening on. 
 *
 * Note: This currently only works on systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned.  To change
 *       this in the future, add an equivalent for get_used_waiting_queue(). 
 */
int get_total_bytes_waiting(void) 
{
	int bytesWaiting = 0;

	int *UDPList  = NULL;
	int *TCPList  = NULL;
	int *TLSList  = NULL;

	int numUDPSockets  = 0;
	int numTCPSockets  = 0; 
	int numTLSSockets  = 0;

	/* Extract out the IP address address for UDP, TCP, and TLS, keeping
	 * track of the number of IP addresses from each transport  */
	numUDPSockets  = get_socket_list_from_proto(&UDPList,  PROTO_UDP);
	numTCPSockets  = get_socket_list_from_proto(&TCPList,  PROTO_TCP);
	numTLSSockets  = get_socket_list_from_proto(&TLSList,  PROTO_TLS);

	/* Find out the number of bytes waiting on our interface list over all
	 * UDP and TCP transports. */
	bytesWaiting  += get_used_waiting_queue(0, UDPList,  numUDPSockets);
	bytesWaiting  += get_used_waiting_queue(1, TCPList,  numTCPSockets);
	bytesWaiting  += get_used_waiting_queue(1, TLSList,  numTLSSockets);

	/* get_socket_list_from_proto() allocated a chunk of memory, so we need
	 * to free it. */
	if (numUDPSockets > 0)
	{
		pkg_free(UDPList);
	}

	if (numTCPSockets > 0) 
	{
		pkg_free(TCPList);
	}

	if (numTLSSockets > 0)
	{
		pkg_free(TLSList);
	}

	return bytesWaiting;
}
