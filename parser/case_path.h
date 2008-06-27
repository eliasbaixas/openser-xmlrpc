/*
 * $Id: case_path.h 567 2006-01-26 18:00:38Z bogdan_iancu $
 *
 * Supported parser.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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


#ifndef CASE_PATH_H
#define CASE_PATH_H

#define path_CASE				\
	p += 4;						\
	switch(LOWER_BYTE(*p)) {	\
	case ':':					\
		hdr->type = HDR_PATH_T;	\
		goto dc_end;			\
								\
	default:					\
		goto other;				\
	}

#endif /* CASE_PATH_H */
