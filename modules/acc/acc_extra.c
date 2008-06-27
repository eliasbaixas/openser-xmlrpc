/*
 * $Id: acc_extra.c 2793 2007-09-20 07:47:17Z bogdan_iancu $
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 *  2004-10-28  first version (ramona)
 *  2005-05-30  acc_extra patch commited (ramona)
 *  2005-07-13  acc_extra specification moved to use pseudo-variables (bogdan)
 *  2006-09-08  flexible multi leg accounting support added,
 *              code cleanup for low level functions (bogdan)
 *  2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 */



#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "acc_extra.h"

#define EQUAL '='
#define SEPARATOR ';'


#if MAX_ACC_EXTRA<MAX_ACC_LEG
	#define MAX_ACC_INT_BUF MAX_ACC_LEG
#else
	#define MAX_ACC_INT_BUF MAX_ACC_EXTRA
#endif
/* here we copy the strings returned by int2str (which uses a static buffer) */
static char int_buf[INT2STR_MAX_LEN*MAX_ACC_INT_BUF];

static char *static_detector = 0;

void init_acc_extra(void)
{
	int i;
	/* ugly trick to get the address of the static buffer */
	static_detector = int2str( (unsigned long)3, &i) + i;
}


struct acc_extra *parse_acc_leg(char *extra_str)
{
	struct acc_extra *legs;
	struct acc_extra *it;
	int n;

	legs = parse_acc_extra(extra_str);
	if (legs==0) {
		LM_ERR("failed to parse extra leg\n");
		return 0;
	}

	/* check the type and len */
	for( it=legs,n=0 ; it ; it=it->next ) {
		if (it->spec.type!=PVT_AVP) {
			LM_ERR("only AVP are accepted as leg info\n");
			destroy_extras(legs);
			return 0;
		}
		n++;
		if (n>MAX_ACC_LEG) {
			LM_ERR("too many leg info; MAX=%d\n", MAX_ACC_LEG);
			destroy_extras(legs);
			return 0;
		}
	}

	return legs;
}


struct acc_extra *parse_acc_extra(char *extra_str)
{
	struct acc_extra *head;
	struct acc_extra *tail;
	struct acc_extra *extra;
	char *foo;
	char *s;
	int  n;
	str stmp;

	n = 0;
	head = 0;
	extra = 0;
	tail = 0;
	s = extra_str;

	if (s==0) {
		LM_ERR("null string received\n");
		goto error;
	}

	while (*s) {
		/* skip white spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s==0)
			goto parse_error;
		if (n==MAX_ACC_EXTRA) {
			LM_ERR("too many extras -> please increase the internal buffer\n");
			goto error;
		}
		extra = (struct acc_extra*)pkg_malloc(sizeof(struct acc_extra));
		if (extra==0) {
			LM_ERR("no more pkg mem 1\n");
			goto error;
		}
		memset( extra, 0, sizeof(struct acc_extra));

		/* link the new extra at the end */
		if (tail==0) {
			head = extra;
		} else {
			tail->next = extra;
		}
		tail = extra;
		n++;

		/* get name */
		foo = s;
		while (*s && !isspace((int)*s) && EQUAL!=*s)  s++;
		if (*s==0)
			goto parse_error;
		if (*s==EQUAL) {
			extra->name.len = (s++) - foo;
		} else {
			extra->name.len = (s++) - foo;
			/* skip spaces */
			while (*s && isspace((int)*s))  s++;
			if (*s!=EQUAL)
				goto parse_error;
			s++;
		}
		extra->name.s = foo;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;

		/* get value type */
		stmp.s = s; stmp.len = strlen(s);
		if ( (foo=pv_parse_spec(&stmp, &extra->spec))==0 )
			goto parse_error;
		s = foo;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s && (*(s++)!=SEPARATOR || *s==0))
			goto parse_error;
	}

	/* go throught all extras and make the names null terminated */
	for( extra=head ; extra ; extra=extra->next)
		extra->name.s[extra->name.len] = 0;

	return head;
parse_error:
	LM_ERR("parse failed in <%s> "
		"around position %d\n",extra_str, (int)(long)(s-extra_str));
error:
	LM_ERR("error\n");
	destroy_extras(head);
	return 0;
}



void destroy_extras( struct acc_extra *extra)
{
	struct acc_extra *foo;

	while (extra) {
		foo = extra;
		extra = extra->next;
		pkg_free(foo);
	}
}


#ifdef RAD_ACC
/* extra name is moved as string part of an attribute; str.len will contain an
 * index to the corresponding attribute
 */
int extra2attrs( struct acc_extra *extra, struct attr *attrs, int offset)
{
	int i;

	for(i=0 ; extra ; i++, extra=extra->next) {
		attrs[offset+i].n = extra->name.s;
	}
	return i;
}
#endif


/* converts the name of the extra from str to integer 
 * and stores it over str.len ; str.s is freed and made zero
 */
int extra2int( struct acc_extra *extra, int *attrs )
{
	unsigned int ui;
	int i;

	for( i=0 ; extra ; i++,extra=extra->next ) {
		if (str2int( &extra->name, &ui)!=0) {
			LM_ERR("<%s> is not a number\n", extra->name.s);
			return -1;
		}
		attrs[i] = (int)ui;
	}
	return i;
}



int extra2strar( struct acc_extra *extra, struct sip_msg *rq, str *val_arr)
{
	pv_value_t value;
	int n;
	int r;

	n = 0;
	r = 0;

	while (extra) {
		/* get the value */
		if (pv_get_spec_value( rq, &extra->spec, &value)!=0) {
			LM_ERR("failed to get '%.*s'\n", extra->name.len,extra->name.s);
		}

		/* check for overflow */
		if (n==MAX_ACC_EXTRA) {
			LM_WARN("array to short -> ommiting extras for accounting\n");
			goto done;
		}

		if(value.flags&PV_VAL_NULL) {
			/* convert <null> to empty to have consistency */
			val_arr[n].s = 0;
			val_arr[n].len = 0;
		} else {
			/* set the value into the acc buffer */
			if (value.rs.s+value.rs.len==static_detector) {
				val_arr[n].s = int_buf + r*INT2STR_MAX_LEN;
				val_arr[n].len = value.rs.len;
				memcpy(val_arr[n].s, value.rs.s, value.rs.len);
				r++;
			} else {
				val_arr[n] = value.rs;
			}
		}
		n++;

		extra = extra->next;
	}

done:
	return n;
}


int legs2strar( struct acc_extra *legs, struct sip_msg *rq, str *val_arr,
																	int start)
{
	static struct usr_avp *avp[MAX_ACC_LEG];
	unsigned short name_type;
	int_str name;
	int_str value;
	int    n;
	int    found;
	int    r;

	found = 0;
	r = 0;

	for( n=0 ; legs ; legs=legs->next,n++ ) {
		/* search for the AVP */
		if (start) {
			if ( pv_get_avp_name( rq, &(legs->spec.pvp), &name, &name_type)<0 )
				goto exit;
			avp[n] = search_first_avp( name_type, name, &value, 0);
		} else {
			avp[n] = search_next_avp( avp[n], &value);
		}

		/* set new leg record */
		if (avp[n]) {
			found = 1;
			/* get its value */
			if(avp[n]->flags & AVP_VAL_STR) {
				val_arr[n] = value.s;
			} else {
				val_arr[n].s = int2bstr( value.n, int_buf+r*INT2STR_MAX_LEN,
					&val_arr[n].len);
				r++;
			}
		} else {
			val_arr[n].s = 0;
			val_arr[n].len = 0;
		}

	}

	if (found || start)
		return n;
exit:
	return 0;
}


