/*
 *$Id: mod_fix.h 2849 2007-10-04 11:46:34Z miconda $
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
 */


#ifndef _mod_fix_h
#define _mod_fix_h

int str_fixup(void** param, int param_no);

int free_str_fixup(void** param, int param_no);

int fixup_str2int(void** param, int param_no);

int fixup_str2regexp(void** param, int param_no);

int free_fixup_str2regexp(void** param, int param_no);

#endif
