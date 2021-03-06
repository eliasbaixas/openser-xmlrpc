/*
 * $Id: save.h 1249 2006-11-22 12:16:11Z bogdan_iancu $
 *
 * Functions that process REGISTER message 
 * and store data in usrloc
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
 * -------
 * 2003-03-21  save_noreply added, provided by Maxim Sobolev 
 *             <sobomax@portaone.com> (janakj)
 * 2006-11-22  save_noreply and save_memory merged into save() (bogdan)
 */


#ifndef SAVE_H
#define SAVE_H


#include "../../parser/msg_parser.h"


/*
 * Process REGISTER request and save it's contacts
 */
int save(struct sip_msg* _m, char* _d, char* _cflags);


#endif /* SAVE_H */
