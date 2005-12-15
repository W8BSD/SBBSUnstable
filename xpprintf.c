#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "xpprintf.h"

/* MSVC Sucks - can't tell the required len of a *printf() */
/*
 * ToDo: don't cripple everyone because of this... see
 * conio/ciolib.c cprintf() for details
 */
#define MAX_ARG_LEN		10240

/* Maximum length of a format specifier including the % */
#define MAX_FORMAT_LEN	256

/*
 * Performs the next replacement in format using the variable
 * specified as the only vararg which is currently the type
 * specified in type (defined in xpprintf.h).
 *
 * Does not currently support the $ argument selector.
 *
 * Currently, the type is not overly usefull, but this could be used for
 * automatic type conversions (ie: int to char *).  Right now it just assures
 * that the type passed to sprintf() is the type passed to
 * single_printf_realloc().
 */
char *xp_asprintf_next(char *format, int type, ...)
{
	va_list vars;
	char			*p;
	char			*newbuf;
	int				i,j;
	unsigned int	ui;
	long int		l;
	unsigned long int	ul;
	long long int	ll;
	unsigned long long int	ull;
	double			d;
	long double		ld;
	char*			cp;
	void*			pntr;
	size_t			s;
	unsigned long	offset=0;
	size_t			format_len;
	size_t			this_format_len;
	char			entry_buf[MAX_ARG_LEN];
	char			this_format[MAX_FORMAT_LEN];
	char			*fmt;
	int				modifier;
	char			*fmt_start;

	/*
	 * Find the next non %% format, leaving %% as it is
	 */
	for(p=format; *p; p++) {
		if(*p=='%') {
			if(*(p+1) == '%')
				p+=2;
			else
				break;
		}
	}
	if(!*p)
		return(format);
	offset=p-format;
	format_len=strlen(format);
	this_format[0]=0;
	fmt=this_format;
	fmt_start=p;
	*(fmt++)=*(p++);

	/*
	 * Skip flags (zero or more)
	 */
	while(1) {
		switch(*p) {
			case '#':
				*(fmt++)=*(p++);
				break;
			case '-':
				*(fmt++)=*(p++);
				break;
			case '+':
				*(fmt++)=*(p++);
				break;
			case ' ':
				*(fmt++)=*(p++);
				break;
			case '0':
				*(fmt++)=*(p++);
				break;
			case '\'':
				*(fmt++)=*(p++);
				break;
			default:
				break;
		}
	}

	/*
	 * If width is '*' then the argument is an unsigned int
	 * which specifies the width.
	 */
	if(*p=='*') {		/* The argument is this width */
		va_start(vars, type);
		i=sprintf(entry_buf,"%u", va_arg(vars, int));
		va_end(vars);
		if(i > 1) {
			newbuf=(char *)realloc(format, format_len+i /* -1 for the '*' that's already there, +1 for the terminator */);
			if(newbuf==NULL)
				return(NULL);
			/*
			 * Move trailing end to make space... leaving the * where it
			 * is so it can be overwritten
			 */
			memmove(p+i, p+1, format-p+format_len);
			memcpy(p, entry_buf, i);
		}
		else
			*p=entry_buf[0];
		return(format);
	}
	/* Skip width */
	while(*p >= '0' && *p <= '9')
		*(fmt++)=*(p++);
	/* Check for precision */
	if(*p=='.') {
		p++;
		/*
		 * If the precision is '*' then the argument is an unsigned int which
		 * specifies the precision.
		 */
		if(*p=='*') {
			va_start(vars, type);
			i=sprintf(entry_buf,"%u", va_arg(vars, int));
			va_end(vars);
			if(i > 1) {
				newbuf=(char *)realloc(format, format_len+i /* -1 for the '*' that's already there, +1 for the terminator */);
				if(newbuf==NULL)
					return(NULL);
				/*
				 * Move trailing end to make space... leaving the * where it
				 * is so it can be overwritten
				 */
				memmove(p+i, p+1, format-p+format_len);
				memcpy(p, entry_buf, i);
			}
			else
				*p=entry_buf[0];
			return(format);
		}
		/* Skip precision */
		while(*p >= '0' && *p <= '9')
			*(fmt++)=*(p++);
	}

	/*
	 * Copy the arg to the passed type.
	 */
	va_start(vars, type);
	switch(type) {
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			i=va_arg(vars, int);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			ui=va_arg(vars, unsigned int);
			break;
		case XP_PRINTF_TYPE_LONG:
			l=va_arg(vars, long);
			break;
		case XP_PRINTF_TYPE_ULONG:
			ul=va_arg(vars, unsigned long int);
			break;
		case XP_PRINTF_TYPE_LONGLONG:
			ll=va_arg(vars, long long int);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			ull=va_arg(vars, unsigned long long int);
			break;
		case XP_PRINTF_TYPE_CHARP:
			cp=va_arg(vars, char*);
			break;
		case XP_PRINTF_TYPE_DOUBLE:
			d=va_arg(vars, double);
			break;
		case XP_PRINTF_TYPE_LONGDOUBLE:
			ld=va_arg(vars, long double);
			break;
		case XP_PRINTF_TYPE_VOIDP:
			pntr=va_arg(vars, void*);
			break;
		case XP_PRINTF_TYPE_SIZET:
			s=va_arg(vars, size_t);
			break;
	}
	va_end(vars);

	/* Skip/Translate length modifiers */
	/*
	 * ToDo: This could (should?) convert the standard ll modifier
	 * to the MSVC equivilant (I64 or something?)
	 * if you do this, the calculations using this_format_len will need
	 * rewriting.
	 */
	switch(*p) {
		case 'h':
			modifier='h';
			*(fmt++)=*(p++);
			if(*p=='h') {
				*(fmt++)=*(p++);
				modifier+='h'<<8;
			}
			break;
		case 'l':
			modifier='h';
			*(fmt++)=*(p++);
			if(*p=='l') {
				*(fmt++)=*(p++);
				modifier+='l'<<8;
			}
			break;
		case 'j':
			modifier='j';
			*(fmt++)=*(p++);
			break;
		case 't':
			modifier='t';
			*(fmt++)=*(p++);
			break;
		case 'z':
			modifier='z';
			*(fmt++)=*(p++);
			break;
		case 'L':
			modifier='L';
			*(fmt++)=*(p++);
			break;
	}
	/* The next char is now the type... perform native sprintf() using it */
	*(fmt++)=*p;
	*fmt=0;
	switch(type) {
		case XP_PRINTF_TYPE_INT:	/* Also includes char and short */
			j=sprintf(entry_buf, fmt, i);
			break;
		case XP_PRINTF_TYPE_UINT:	/* Also includes char and short */
			j=sprintf(entry_buf, fmt, ui);
			break;
		case XP_PRINTF_TYPE_LONG:
			j=sprintf(entry_buf, fmt, l);
			break;
		case XP_PRINTF_TYPE_ULONG:
			j=sprintf(entry_buf, fmt, ul);
			break;
		case XP_PRINTF_TYPE_LONGLONG:
			j=sprintf(entry_buf, fmt, ll);
			break;
		case XP_PRINTF_TYPE_ULONGLONG:
			j=sprintf(entry_buf, fmt, ull);
			break;
		case XP_PRINTF_TYPE_CHARP:
			j=sprintf(entry_buf, fmt, cp);
			break;
		case XP_PRINTF_TYPE_DOUBLE:
			j=sprintf(entry_buf, fmt, d);
			break;
		case XP_PRINTF_TYPE_LONGDOUBLE:
			j=sprintf(entry_buf, fmt, ld);
			break;
		case XP_PRINTF_TYPE_VOIDP:
			j=sprintf(entry_buf, fmt, pntr);
			break;
		case XP_PRINTF_TYPE_SIZET:
			j=sprintf(entry_buf, fmt, s);
			break;
	}

	this_format_len=strlen(fmt);
	if(j > this_format_len) {
		newbuf=(char *)realloc(format, format_len-this_format_len+j+1);
		if(newbuf==NULL)
			return(NULL);
		/* Move trailing end to make space */
		memmove(fmt_start+j, fmt_start+this_format_len, format-fmt_start+format_len-this_format_len);
		memcpy(fmt_start, entry_buf, j);
	}
	return(format);
}

char *xp_asprintf_start(char *format)
{
	return(strdup(format));
}

char *xp_asprintf_end(char *format)
{
	char	*p;
	size_t	len;

	len=strlen(format);
	for(p=format; *p; p++, len--) {
		if(*p=='%' && *(p+1)=='%')
			memmove(p, p+1, len--);
	}
	return(format);
}
