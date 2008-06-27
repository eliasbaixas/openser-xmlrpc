/* $Id: encode_cseq.h 1819 2007-03-11 19:20:59Z miconda $
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


#include "../../str.h"
#include "../../parser/msg_parser.h"
int encode_cseq(char *hdrstart,int hdrlen,struct cseq_body *body,unsigned char *where);
int print_encoded_cseq(FILE* fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
