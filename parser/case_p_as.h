/* 
 *
 * P-Asserted-Identity Header Field Name Parsing Macros
 *
 * Copyright (C) 2006 Juha Heinanen
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

#ifndef CASE_P_AS_H
#define CASE_P_AS_H


#define ITY_CASE                                \
	switch( LOWER_DWORD(val) ) {	        \
	case _ity1_:				\
		hdr->type = HDR_PAI_T;	        \
		hdr->name.len = 19;		\
		return p + 4;			\
	case _ity2_:				\
		hdr->type = HDR_PAI_T;	        \
		p += 4;				\
		goto dc_end;			\
	}

#define DENT_CASE                         \
        if (LOWER_DWORD(val) == _dent_) { \
	        p += 4;                   \
                val = READ(p);            \
                ITY_CASE;                 \
		goto other;               \
	}


#define ED_I_CASE                         \
        if (LOWER_DWORD(val) == _ed_i_) { \
	        p += 4;                   \
                val = READ(p);            \
                DENT_CASE;                 \
		goto other;               \
	}
	             

#define SERT_CASE                          \
        if (LOWER_DWORD(val) == _sert_) {  \
                p += 4;                    \
	        val = READ(p);             \
	        ED_I_CASE;                 \
                goto other;                \
        }


#define p_as_CASE      \
     p += 4;           \
     val = READ(p);    \
     SERT_CASE;        \
     goto other;


#endif /* CASE_P_AS_H */
