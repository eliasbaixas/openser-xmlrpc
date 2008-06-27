/*
 * $Id: udp_server.h 2565 2007-08-01 21:40:18Z miconda $
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
 */


#ifndef udp_server_h
#define udp_server_h

#include <sys/types.h>
#include <sys/socket.h>
#include "ip_addr.h"


int udp_init(struct socket_info* si);
int udp_send(struct socket_info* source,char *buf, unsigned len,
				union sockaddr_union*  to);
int udp_rcv_loop();


#endif
