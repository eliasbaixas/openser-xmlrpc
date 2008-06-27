/* $Id: encode_content_disposition.c 1819 2007-03-11 19:20:59Z miconda $
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

/*
 * =====================================================================================
 * 
 *        Filename:  encode_content_disposition.c
 * 
 *     Description:  [en|de]encodes content disposition
 * 
 *         Version:  1.0
 *         Created:  21/11/05 20:36:19 CET
 *        Revision:  none
 *        Compiler:  gcc
 * 
 *          Author:  Elias Baixas (EB), elias@conillera.net
 *         Company:  VozTele.com
 * 
 * =====================================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include "../../parser/parse_disposition.h"
#include "encode_parameters.h"

/*
struct disposition_param { str name; str body; int is_quoted; struct disposition_param *next; };
struct disposition { str type; struct disposition_param *params; };
*/

int encode_content_disposition(char *hdrstart,int hdrlen,struct disposition *body,unsigned char *where)
{
   unsigned char i=3;

   /*where[0] reserved flags for future use*/
   where[1]=(unsigned char)(body->type.s-hdrstart);
   where[2]=(unsigned char)body->type.len;
   i+=encode_parameters(&where[3],(void *)body->params,hdrstart,body,'d');
   return i;
}

int print_encoded_content_disposition(FILE* fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix)
{
   int i=3;/* flags + urilength */
   unsigned char flags=0;

   flags=payload[0];
   fprintf(fd,"%s",prefix);
   for(i=0;i<paylen;i++)
      fprintf(fd,"%s%d%s",i==0?"ENCODED CONTENT-DISPOSITION=[":":",payload[i],i==paylen-1?"]\n":"");
   fprintf(fd,"%sCONTENT DISPOSITION:[%.*s]\n",prefix,payload[2],&hdr[payload[1]]);
   print_encoded_parameters(fd,&payload[3],hdr,paylen-3,prefix);
   return 0;
}


