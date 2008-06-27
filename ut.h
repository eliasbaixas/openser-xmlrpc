/*
 *$Id: ut.h 2657 2007-08-28 15:03:09Z henningw $
 *
 * - various general purpose functions
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
 *
 * History
 * ------
 * 2003-01-18 un_escape function introduced for convenience of code needing
 *            the complex&slow feature of unescaping
 * 2003-01-28 scratchpad removed (jiri)
 * 2003-01-29 pathmax added (jiri)
 * 2003-02-13 strlower added (janakj)
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-03-30 str2int and str2float added (janakj)
 * 2003-04-26 ZSW (jiri)
 * 2004-03-08 updated int2str (64 bits, INT2STR_MAX_LEN used) (andrei)
 */


#ifndef ut_h
#define ut_h

#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "dprint.h"
#include "str.h"

#include "mem/mem.h"
#include "mem/shm_mem.h"

struct sip_msg;

/* zero-string wrapper */
#define ZSW(_c) ((_c)?(_c):"")

/* str initialization */
#define str_init(_string)  {_string, sizeof(_string) - 1}

/* returns string beginning and length without insignificant chars */
#define trim_len( _len, _begin, _mystr ) \
	do{ 	static char _c; \
		(_len)=(_mystr).len; \
		while ((_len) && ((_c=(_mystr).s[(_len)-1])==0 || _c=='\r' || \
					_c=='\n' || _c==' ' || _c=='\t' )) \
			(_len)--; \
		(_begin)=(_mystr).s; \
		while ((_len) && ((_c=*(_begin))==' ' || _c=='\t')) { \
			(_len)--;\
			(_begin)++; \
		} \
	}while(0)

#define trim_r( _mystr ) \
	do{	static char _c; \
		while( ((_mystr).len) && ( ((_c=(_mystr).s[(_mystr).len-1]))==0 ||\
									_c=='\r' || _c=='\n' ) \
				) \
			(_mystr).len--; \
	}while(0)


#define  translate_pointer( _new_buf , _org_buf , _p) \
	( (_p)?(_new_buf + (_p-_org_buf)):(0) )

#define via_len(_via) \
	((_via)->bsize-((_via)->name.s-\
		((_via)->hdr.s+(_via)->hdr.len)))


/* char to hex conversion table */
static char fourbits2char[16] = { '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };


/* converts a str to an u. short, returns the u. short and sets *err on
 * error and if err!=null
  */
static inline unsigned short str2s(const char* s, unsigned int len,
									int *err)
{
	unsigned short ret;
	int i;
	unsigned char *limit;
	unsigned char *init;
	unsigned char* str;

	/*init*/
	str=(unsigned char*)s;
	ret=i=0;
	limit=str+len;
	init=str;

	for(;str<limit ;str++){
		if ( (*str <= '9' ) && (*str >= '0') ){
				ret=ret*10+*str-'0';
				i++;
				if (i>5) goto error_digits;
		}else{
				//error unknown char
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_digits:
	LM_DBG("too many letters in [%.*s]\n", (int)len, init);
	if (err) *err=1;
	return 0;
error_char:
	LM_DBG("unexpected char %c in %.*s\n", *str, (int)len, init);
	if (err) *err=1;
	return 0;
}



static inline int btostr( char *p,  unsigned char val)
{
	unsigned int a,b,i =0;

	if ( (a=val/100)!=0 )
		*(p+(i++)) = a+'0';         /*first digit*/
	if ( (b=val%100/10)!=0 || a)
		*(p+(i++)) = b+'0';        /*second digit*/
	*(p+(i++)) = '0'+val%10;              /*third digit*/

	return i;
}


/* 2^64~= 16*10^18 => 19+1+1 sign + digits + \0 */
#define INT2STR_MAX_LEN  (1+19+1+1)

/* INTeger-TO-Buffer-STRing : convers an unsigned long to a string 
 * IMPORTANT: the provided buffer must be at least INT2STR_MAX_LEN size !! */
static inline char* int2bstr(unsigned long l, char *s, int* len)
{
	int i;

	i=INT2STR_MAX_LEN-2;
	s[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		s[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		LM_CRIT("overflow error\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &s[i+1];
}


/* INTeger-TO-STRing : convers an unsigned long to a string 
 * returns a pointer to a static buffer containing l in asciiz & sets len */
extern char int2str_buf[INT2STR_MAX_LEN];
static inline char* int2str(unsigned long l, int* len)
{
	return int2bstr( l, int2str_buf, len);
}


/* Signed INTeger-TO-STRing: convers a long to a string
 * returns a pointer to a static buffer containing l in asciiz & sets len */
static inline char* sint2str(long l, int* len)
{
	int sign;
	char *p;

	sign = 0;
	if(l<0) {
		sign = 1;
		l = -l;
	}
	p = int2str((unsigned long)l, len);
	if(sign) {
		*(--p) = '-';
		if (len) (*len)++;
	}
	return p;
}


/* faster memchr version */
static inline char* q_memchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size;
	for(;p<end;p++){
		if (*p==(unsigned char)c) return p;
	}
	return 0;
}
	

inline static int reverse_hex2int( char *c, int len )
{
	char *pc;
	int r;
	char mychar;

	r=0;
	for (pc=c+len-1; len>0; pc--, len--) {
		r <<= 4 ;
		mychar=*pc;
		if ( mychar >='0' && mychar <='9') r+=mychar -'0';
		else if (mychar >='a' && mychar <='f') r+=mychar -'a'+10;
		else if (mychar  >='A' && mychar <='F') r+=mychar -'A'+10;
		else return -1;
	}
	return r;
}

inline static int int2reverse_hex( char **c, int *size, int nr )
{
	unsigned short digit;

	if (*size && nr==0) {
		**c = '0';
		(*c)++;
		(*size)--;
		return 1;
	}

	while (*size && nr ) {
		digit = nr & 0xf ;
		**c= digit >= 10 ? digit + 'a' - 10 : digit + '0';
		nr >>= 4;
		(*c)++;
		(*size)--;
	}
	return nr ? -1 /* number not processed; too little space */ : 1;
}

inline static int hexstr2int(char *c, int len, unsigned int *val)
{
	char *pc;
	int r;
	char mychar;

	r=0;
	for (pc=c; pc<c+len; pc++) {
		r <<= 4 ;
		mychar=*pc;
		if ( mychar >='0' && mychar <='9') r+=mychar -'0';
		else if (mychar >='a' && mychar <='f') r+=mychar -'a'+10;
		else if (mychar  >='A' && mychar <='F') r+=mychar -'A'+10;
		else return -1;
	}
	*val = r;
	return 0;
}


/* double output length assumed ; does NOT zero-terminate */
inline static int string2hex( 
	/* input */ unsigned char *str, int len,
	/* output */ char *hex )
{
	int orig_len;

	if (len==0) {
		*hex='0';
		return 1;
	}

	orig_len=len;
	while ( len ) {

		*hex=fourbits2char[((*str) >> 4) & 0x0f];
		hex++;
		*hex=fourbits2char[(*str) & 0x0f];
		hex++;
		len--;
		str++;

	}
	return orig_len-len;
}

/* portable sleep in microseconds (no interrupt handling now) */

inline static void sleep_us( unsigned int nusecs )
{
	struct timeval tval;
	tval.tv_sec=nusecs/1000000;
	tval.tv_usec=nusecs%1000000;
	select(0, NULL, NULL, NULL, &tval );
}


/* portable determination of max_path */
inline static int pathmax(void)
{
#ifdef PATH_MAX
	static int pathmax=PATH_MAX;
#else
	static int pathmax=0;
#endif
	if (pathmax==0) { /* init */
		pathmax=pathconf("/", _PC_PATH_MAX);
		pathmax=(pathmax<=0)?PATH_MAX_GUESS:pathmax+1;
	}
	return pathmax;
}

inline static int hex2int(char hex_digit)
{
	if (hex_digit>='0' && hex_digit<='9')
		return hex_digit-'0';
	if (hex_digit>='a' && hex_digit<='f')
		return hex_digit-'a'+10;
	if (hex_digit>='A' && hex_digit<='F')
		return hex_digit-'A'+10;
	/* no valid hex digit ... */
	LM_ERR("'%c' is no hex char\n", hex_digit );
	return -1;
}

/* Un-escape URI user  -- it takes a pointer to original user
   str, as well as the new, unescaped one, which MUST have
   an allocated buffer linked to the 'str' structure ;
   (the buffer can be allocated with the same length as
   the original string -- the output string is always
   shorter (if escaped characters occur) or same-long
   as the original one).

   only printable characters are permitted

	<0 is returned on an unescaping error, length of the
	unescaped string otherwise
*/
inline static int un_escape(str *user, str *new_user ) 
{
 	int i, j, value;
	int hi, lo;

	if( new_user==0 || new_user->s==0) {
		LM_CRIT("called with invalid param\n");
		return -1;
	}

	new_user->len = 0;
	j = 0;

	for (i = 0; i < user->len; i++) {
		if (user->s[i] == '%') {
			if (i + 2 >= user->len) {
				LM_ERR("escape sequence too short in"
					" '%.*s' @ %d\n",
					user->len, user->s, i );
				goto error;
			}
			hi=hex2int(user->s[i + 1]);
			if (hi<0) {
				LM_ERR(" non-hex high digit in an escape"
					" sequence in '%.*s' @ %d\n",
					user->len, user->s, i+1 );
				goto error;
			}
			lo=hex2int(user->s[i + 2]);
			if (lo<0) {
				LM_ERR("non-hex low digit in an escape sequence in "
					"'%.*s' @ %d\n",
					user->len, user->s, i+2 );
				goto error;
			}
			value=(hi<<4)+lo;
			if (value < 32 || value > 126) {
				LM_ERR("non-ASCII escaped character in '%.*s' @ %d\n",
					user->len, user->s, i );
				goto error;
			}
			new_user->s[j] = value;
			i+=2; /* consume the two hex digits, for cycle will move to the next char */
		} else {
			new_user->s[j] = user->s[i];
		}
        j++; /* good -- we translated another character */
	}
	new_user->len = j;
	return j;

error:
	new_user->len = j;
	return -1;
} 


/*
 * Convert a string to lower case
 */
static inline void strlower(str* _s)
{
	int i;

	for(i = 0; i < _s->len; i++) {
		_s->s[i] = tolower(_s->s[i]);
	}
}


/*
 * Convert a str into integer
 */
static inline int str2int(str* _s, unsigned int* _r)
{
	int i;
	
	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	
	return 0;
}

/*
 * Convert a str into signed integer
 */
static inline int str2sint(str* _s, int* _r)
{
	int i;
	int s;
	
	*_r = 0;
	s = 1;
	i=0;
	if(_s->s[i]=='-') {
		s=-1;
		i++;
	}
	for(; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	*_r *= s;
	return 0;
}


/*
 * Convert a str (base 10 or 16) into integer
 */
static inline int strno2int( str *val, unsigned int *mask )
{
	/* hexa or decimal*/
	if (val->len>2 && val->s[0]=='0' && val->s[1]=='x') {
		return hexstr2int( val->s+2, val->len-2, mask);
	} else {
		return str2int( val, mask);
	}
}


/*
 * Make a copy of a str structure using shm_malloc
 */
static inline int shm_str_dup(str* dst, str* src)
{
	dst->s = shm_malloc(src->len);
	if (!dst->s) {
		LM_ERR("no shared memory left\n");
		return -1;
	}
	
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

/*
 * Make a copy of a str structure using pkg_malloc
 */
static inline int pkg_str_dup(str* dst, str* src)
{
	dst->s = pkg_malloc(src->len);
	if (dst->s==NULL)
	{
		LM_ERR("no private memory left\n");
		return -1;
	}
	
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

/*
 * compare two str's
 */
static inline int str_strcmp(const str *stra, const str *strb)
{
	int i;
	if(stra==NULL || strb==NULL || stra->s ==NULL || strb->s==NULL || stra->len<0 || strb->len<0)
	{
		LM_ERR("bad parameters\n");
		return -2;
	}
	int alen = stra->len;
	int blen = strb->len;
	int minlen = (alen < blen ? alen : blen);

	for (i = 0; i < minlen; i++) {
		const char a = stra->s[i];
		const char b = strb->s[i];
		if (a < b)
			return -1;
		if (a > b)
			return 1;
	}
	if (alen < blen)
		return -1;
	else if (alen > blen)
		return 1;
	else
		return 0;
}

/*
 * case-insensitive compare two str's
 */
static inline int str_strcasecmp(const str *stra, const str *strb)
{
	int i;
	if(stra==NULL || strb==NULL || stra->s ==NULL || strb->s==NULL || stra->len<0 || strb->len<0)
	{
		LM_ERR("bad parameters\n");
		return -2;
	}
	int alen = stra->len;
	int blen = strb->len;
	int minlen = (alen < blen ? alen : blen);

	for (i = 0; i < minlen; i++) {
		const char a = tolower(stra->s[i]);
		const char b = tolower(strb->s[i]);
		if (a < b)
			return -1;
		if (a > b)
			return 1;
	}
	if (alen < blen) 
		return -1;
	else if (alen > blen)
		return 1;
	else
		return 0;
}

int user2uid(int* uid, int* gid, char* user);

int group2gid(int* gid, char* group);

/* utility function to give each children a unique seed */
void seed_child(unsigned int seed);

#endif
