/*
 * $Id: pua_bla.h 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *
 * History:
 * --------
 *  2007-03-30  initial version (anca)
 */

#ifndef PUA_BLA_H
#define PUA_BLA_H

#include "../pua/pua_bind.h"

extern int is_bla_aor;
extern str default_domain;
extern str header_name;
extern str outbound_proxy;
extern str server_address;
extern str reg_from_uri;

extern send_publish_t pua_send_publish;
extern send_subscribe_t pua_send_subscribe;
extern query_dialog_t pua_is_dialog;
extern query_dialog_t pua_is_dialog;
extern int bla_handle_notify(struct sip_msg* msg, char* s1, char* s2);

#endif
