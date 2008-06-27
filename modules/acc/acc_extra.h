/*
 * $Id: acc_extra.h 2725 2007-09-09 20:55:56Z miconda $
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2004-10-28  first version (ramona)
 *  2005-05-30  acc_extra patch commited (ramona)
 *  2005-07-13  acc_extra specification moved to use pseudo-variables (bogdan)
 *  2006-09-08  flexible multi leg accounting support added,
 *              code cleanup for low level functions (bogdan)
 *  2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 */


#ifndef _ACC_EXTRA_H_
#define _ACC_EXTRA_H_

#include "../../str.h"
#include "../../pvar.h"
#include "../../parser/msg_parser.h"

struct acc_extra
{
	str        name;       /* name (log comment/ column name) */
	pv_spec_t  spec;       /* value's spec */
	struct acc_extra *next;
};


#define MAX_ACC_EXTRA 64
#define MAX_ACC_LEG   16


void init_acc_extra();

struct acc_extra *parse_acc_extra(char *extra);

struct acc_extra *parse_acc_leg(char *extra);

void destroy_extras( struct acc_extra *extra);

int extra2strar( struct acc_extra *extra, struct sip_msg *rq, str *val_arr);

int legs2strar( struct acc_extra *legs, struct sip_msg *rq, str *val_arr,
																	int start);

int extra2int( struct acc_extra *extra, int *attrs );

#ifdef RAD_ACC
#include "../../radius.h"
int extra2attrs( struct acc_extra *extra, struct attr *attrs, int offset);
#endif

#endif

