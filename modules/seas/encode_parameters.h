/* $Id: encode_parameters.h 1819 2007-03-11 19:20:59Z miconda $
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


#include "../../parser/parse_param.h"  /*for param_t def*/
int encode_parameters(unsigned char *where,void *pars,char *hdrstart,void *_body,char to);
param_t *reverseParameters(param_t *p);
int print_encoded_parameters(FILE* fd,unsigned char *payload,char *hdr,int paylen,char *prefix);
