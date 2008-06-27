/* 
 * $Id: poll_types.h 513 2006-01-09 20:31:54Z bogdan_iancu $
 * 
 * Copyright (C) 2005 iptelorg GmbH
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
/* 
 * io wait poll methods (enum, strings, related function)
 * see io_wait.h for more details
 * 
 *  
 */
/* 
 * History:
 * --------
 *  2005-06-15  created by andrei
 */


#ifndef _poll_types_h
#define _poll_types_h

enum poll_types { POLL_NONE, POLL_POLL, POLL_EPOLL_LT, POLL_EPOLL_ET,
					POLL_SIGIO_RT, POLL_SELECT, POLL_KQUEUE, POLL_DEVPOLL,
					POLL_END};

/* all the function and vars are defined in io_wait.c */

extern char* poll_method_str[POLL_END];
extern char* poll_support; 


enum poll_types choose_poll_method();

/* returns 0 on success, and an error message on error */
char* check_poll_method(enum poll_types poll_method);

char* poll_method_name(enum poll_types poll_method);
enum poll_types get_poll_type(char* s);

#endif
