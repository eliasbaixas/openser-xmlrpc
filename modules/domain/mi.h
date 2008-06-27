/*
 * $Id: mi.h 1286 2006-11-29 13:13:18Z bogdan_iancu $
 *
 * Header file for domain MI functions
 *
 * Copyright (C) 2006 Voice Sistem SRL
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


#ifndef _DOMAIN_MI_H_
#define _DOMAIN_MI_H_

#include "../../mi/mi.h"

#define MI_DOMAIN_RELOAD "domain_reload"
#define MI_DOMAIN_DUMP   "domain_dump"


struct mi_root* mi_domain_reload(struct mi_root *cmd, void *param);

struct mi_root* mi_domain_dump(struct mi_root *cmd, void *param);


#endif
