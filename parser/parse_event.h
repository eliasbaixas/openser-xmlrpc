/*
 * $Id: parse_event.h 2606 2007-08-16 08:37:29Z anca_vamanu $
 *
 * Event header field body parser
 * This parser was written for Presence Agent module only.
 * it recognizes presence package only, no subpackages, no parameters
 * It should be replaced by a more generic parser if subpackages or
 * parameters should be parsed too.
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


#ifndef PARSE_EVENT_H
#define PARSE_EVENT_H

#include "../str.h"
#include "hf.h"
#include "parse_param.h"

#define EVENT_OTHER          0
#define EVENT_PRESENCE       1
#define EVENT_PRESENCE_WINFO 2
#define EVENT_SIP_PROFILE    3
#define EVENT_XCAP_CHANGE    4
#define EVENT_DIALOG         5
#define EVENT_MWI            6

typedef struct event {
	str text;       /* Original string representation */
	int parsed;     /* Parsed variant */
	param_t* params;
} event_t;


/*
 * Parse Event HF body
 */
int parse_event(struct hdr_field* _h);


/*
 * Release memory
 */
void free_event(event_t** _e);


/*
 * Print structure, for debugging only
 */
void print_event(event_t* _e);

int event_parser(char* _s, int _l, event_t* _e);

#endif /* PARSE_EVENT_H */
