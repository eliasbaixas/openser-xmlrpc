/*
 * $Id: strcommon.c 4130 2008-05-08 07:41:29Z dan_pascu $
 *
 * Copyright (C) 2007 voice-system.ro
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
 */

#include "ut.h"
#include "strcommon.h"

/*
 * add backslashes to special characters
 */
int escape_common(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	for(i=0; i<src_len; i++)
	{
		switch(src[i])
		{
			case '\'':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '"':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\\':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\0':
				dst[j++] = '\\';
				dst[j++] = '0';
				break;
			default:
				dst[j++] = src[i];
		}
	}
	return j;
}
/*
 * remove backslashes to special characters
 */
int unescape_common(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	i = 0;
	while(i<src_len)
	{
		if(src[i]=='\\' && i+1<src_len)
		{
			switch(src[i+1])
			{
				case '\'':
					dst[j++] = '\'';
					i++;
					break;
				case '"':
					dst[j++] = '"';
					i++;
					break;
				case '\\':
					dst[j++] = '\\';
					i++;
					break;
				case '0':
					dst[j++] = '\0';
					i++;
					break;
				default:
					dst[j++] = src[i];
			}
		} else {
			dst[j++] = src[i];
		}
		i++;
	}
	return j;
}

void compute_md5(char *dst, char *src, int src_len)
{
	MD5_CTX context;
	unsigned char digest[16];
	MD5Init (&context);
  	MD5Update (&context, src, src_len);
	MD5Final (digest, &context);
	string2hex(digest, 16, dst);
}

/* unscape all printable ascii characters */
int unescape_user(str *sin, str *sout)
{
	char *at, *p, c;

	if(sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL
			|| sin->len<0 || sout->len < sin->len+1)
		return -1;

	at = sout->s;
	p  = sin->s;
	while(p < sin->s+sin->len)
	{
	    if (*p == '%')
		{
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c = (*p - '0') << 4;
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = (*p - 'a' + 10) << 4;
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = (*p - 'A' + 10) << 4;
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c =  c + (*p - '0');
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = c + (*p - 'a' + 10);
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = c + (*p - 'A' + 10);
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			if ((c < 32) || (c > 126))
			{
			    LM_ERR("invalid escaped character <%u>\n", (unsigned int)c);
			    return -1;
			}
			*at++ = c;
	    } else {
			*at++ = *p;
	    }
		p++;
	}

	*at = 0;
	sout->len = at - sout->s;
	
	LM_DBG("unescaped string is <%s>\n", sout->s);
	return 0;
}

int escape_user(str *sin, str *sout)
{

	/* escape all printable characters that are not valid in user
	 * part of request uri
	 * no_need_to_escape = unreserved | user-unreserved
	 * unreserved = aplhanum | mark
	 * mark = - | _ | . | ! | ~ | * | ' | ( | )
	 * user-unreserved = & | = | + | $ | , | ; | ? | /
	 */

	char *at, *p;
	unsigned char x;

	if(sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL
			|| sin->len<0 || sout->len < 3*sin->len+1)
		return -1;


	at = sout->s;
	p  = sin->s;
	while (p < sin->s+sin->len)
	{
	    if (*p < 32 || *p > 126)
		{
			LM_ERR("invalid escaped character <%u>\n", (unsigned int)*p);
			return -1;
	    }
	    if (isdigit((int)*p) || ((*p >= 'A') && (*p <= 'Z')) ||
				((*p >= 'a') && (*p <= 'z')))
		{
			*at = *p;
	    } else {
			switch (*p) {
				case '-':
				case '_':
				case '.':
				case '!':
				case '~':
				case '*':
				case '\'':
				case '(':
				case ')':
				case '&':
				case '=':
				case '+':
				case '$':
				case ',':
				case ';':
				case '?':
				    *at = *p;
				break;
				default:
				    *at++ = '%';
				    x = (*p) >> 4;
				    if (x < 10)
					{
						*at++ = x + '0';
				    } else {
						*at++ = x - 10 + 'a';
				    }
				    x = (*p) & 0x0f;
				    if (x < 10) {
						*at = x + '0';
				    } else {
						*at = x - 10 + 'a';
				    }
			}
	    }
	    at++;
	    p++;
	}
	*at = 0;
	sout->len = at - sout->s;
	LM_DBG("escaped string is <%s>\n", sout->s);
	return 0;
}

