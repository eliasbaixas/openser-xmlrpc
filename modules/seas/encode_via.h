/* $Id: encode_via.h 1819 2007-03-11 19:20:59Z miconda $
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


int encode_via_body(char *hdr,int hdrlen,struct via_body *via_parsed,unsigned char *where);
int encode_via(char *hdrstart,int hdrlen,struct via_body *body,unsigned char *where);
int print_encoded_via_body(FILE* fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int print_encoded_via(FILE* fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int dump_via_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,FILE* fd,char segregationLevel);
