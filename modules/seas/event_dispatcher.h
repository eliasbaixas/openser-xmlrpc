/* $Id: event_dispatcher.h 2188 2007-05-10 15:34:09Z henningw $
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
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


#include "../../ip_addr.h"

struct unc_as{
   char valid;
   int fd;
   char name[MAX_AS_NAME];
   char flags;
   union sockaddr_union su;
};

/*uncomplete as table, from 0 to MAX_UNC_AS_NR are event, from then on are action*/
/*should only be modified by the dispatcher process, or we should add a lock*/
extern struct unc_as unc_as_t[];

int process_unbind_action(as_p as,char *payload,int len);
int process_bind_action(as_p as,char *payload,int len);
int dispatcher_main_loop();
int spawn_action_dispatcher(struct as_entry *as);
