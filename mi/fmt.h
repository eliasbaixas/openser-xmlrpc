/*
 * $Id: fmt.h 2786 2007-09-17 17:11:18Z bogdan_iancu $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 *  2006-09-08  first version (bogdan)
 */


#ifndef _MI_FMT_H_
#define _MI_FMT_H_

#include <stdarg.h>
#include <errno.h>

/* size of the buffer used for printing the FMT */
#define DEFAULT_MI_FMT_BUF_SIZE 2048

extern char *mi_fmt_buf;
extern int  mi_fmt_buf_len;

int mi_fmt_init( unsigned int size );

static inline char* mi_print_fmt(char *fmt, va_list ap, int *len)
{
	int n;

	if (mi_fmt_buf==NULL) {
		if (mi_fmt_init(DEFAULT_MI_FMT_BUF_SIZE)!=0) {
			LM_ERR("failed to init\n");
			return 0;
		}
	}

	n = vsnprintf( mi_fmt_buf, mi_fmt_buf_len, fmt, ap);
	if (n<0 || n>=mi_fmt_buf_len) {
		LM_ERR("formatting failed with n=%d, %s\n",n,strerror(errno));
		return 0;
	}

	*len = n;
	return mi_fmt_buf;
}

#endif
