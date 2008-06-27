/* 
 * $Id: reg_mod.h 2924 2007-10-16 13:46:58Z henningw $ 
 *
 * registrar module interface
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
 *
 * History:
 * --------
 *
 * 2005-07-11  added sip_natping_flag for nat pinging with SIP method
 *             instead of UDP package (bogdan)
 * 2006-11-28  Added statistics tracking for the number of accepted/rejected
 *             registrations, as well as for the max expiry time, max contacts,
 *             and default expiry time. (Jeffrey Magder - SOMA Networks)
 * 2007-02-24  sip_natping_flag moved into branch flags, so migrated to 
 *             nathelper module (bogdan)
 */


#ifndef REG_MOD_H
#define REG_MOD_H

#include "../../parser/msg_parser.h"
#include "../../qvalue.h"
#include "../../usr_avp.h"
#include "../usrloc/usrloc.h"
#include "../sl/sl_api.h"

/* if DB support is used, this values must not exceed the 
 * storage capacity of the DB columns! See db/schema/entities.xml */
#define CONTACT_MAX_SIZE       255
#define RECEIVED_MAX_SIZE      255
#define USERNAME_MAX_SIZE      64
#define DOMAIN_MAX_SIZE        128
#define CALLID_MAX_SIZE        255
#define UA_MAX_SIZE            255

#define PATH_MODE_STRICT	2
#define PATH_MODE_LAZY		1
#define PATH_MODE_OFF		0

#define REG_SAVE_MEM_FL     (1<<0)
#define REG_SAVE_NORPL_FL   (1<<1)
#define REG_SAVE_ALL_FL     ((1<<2)-1)

extern int default_expires;
extern qvalue_t default_q;
extern int append_branches;
extern int case_sensitive;
extern int nat_flag;
extern int tcp_persistent_flag;
extern int min_expires;
extern int max_expires;
extern int received_avp;
extern int use_domain;
extern str realm_prefix;
extern float def_q;

extern unsigned short aor_avp_type;
extern int_str aor_avp_name;
extern unsigned short rcv_avp_type;
extern int_str rcv_avp_name;

extern str rcv_param;
extern int max_contacts;
extern int retry_after;
extern int method_filtering;
extern int path_enabled;
extern int path_mode;
extern int path_use_params;

extern str sock_hdr_name;
extern int sock_flag;

usrloc_api_t ul;  /* Structure containing pointers to usrloc functions */

extern struct sl_binds slb;

extern stat_var *accepted_registrations;
extern stat_var *rejected_registrations;

#endif /* REG_MOD_H */
