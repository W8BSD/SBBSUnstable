/* websrvr.c */

/* Synchronet Web Server */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/*
 * General notes: (ToDo stuff)
 * strtok() is used a LOT in here... notice that there is a strtok_r() for reentrant...
 * this may imply that strtok() is NOT thread-safe... if in fact it isn't this HAS
 * to be fixed before any real production-level quality is achieved with this web server
 * however, strtok_r() may not be a standard function.
 *
 * RE: not sending the headers if an nph scrpit is detected.  (The headers buffer could
 * just be free()ed and NULLed)
 *
 * Currently, all SSJS requests for a session are ran in the same context without clearing the context in
 * any way.  This behaviour should not be relied on as it may disappear in the future... this will require
 * some thought as it COULD be handy in some circumstances and COULD cause weird bugs in others.
 *
 * Dynamic content is always resent on an If-Modified-Since request... this may not be optimal behaviour
 * for GET requests...
 *
 * Should support RFC2617 Digest auth.
 *
 * Fix up all the logging stuff.
 *
 * SSJS stuff could work using three different methods:
 * 1) Temporary file as happens currently
 *		Advantages:
 *			Allows to keep current connection (keep-alive works)
 *			write() doesn't need to be "special"
 *		Disadvantages:
 *			Depends on the temp dir being writable and capable of holding
 *				the full reply
 *			Everything goes throug the disk, so probobly some performance
 *				penalty is involved
 *			No way of sending directly to the remote system
 * 2) nph- style
 *		Advantages:
 *			No file I/O involved
 *			Can do magic tricks (ala my perl web wrapper)
 *		Disadvantages:
 *			Pretty much everything needs to be handled by the script.
 * 3) Return body in http_reply object
 *		All the advantages of 1)
 *		Could use a special write() to make everything just great.
 *		Still doesn't allow page to be sent until fully composed (ie: long
 *			delays)
 * 4) Type three with a callback that sends the header and current body, then
 *		converts write() to send directly to remote.
 *
 * Add in support to pass connections through to a different webserver...
 *      probobly in access.ars... with like a simplified mod_rewrite.
 *      This would allow people to run apache and Synchronet as the same site.
 */

/* Headers for CGI stuff */
#if defined(__unix__)
	#include <sys/wait.h>		/* waitpid() */
	#include <sys/types.h>
	#include <signal.h>			/* kill() */
#endif

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#undef SBBS	/* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"
#include "sockwrap.h"		/* sendfilesocket() */
#include "threadwrap.h"
#include "semwrap.h"
#include "websrvr.h"
#include "base64.h"

static const char*	server_name="Synchronet Web Server";
static const char*	newline="\r\n";
static const char*	http_scheme="http://";
static const size_t	http_scheme_len=7;
static const char*	error_301="301 Moved Permanently";
static const char*	error_302="302 Moved Temporarily";
static const char*	error_404="404 Not Found";
static const char*	error_500="500 Internal Server Error";
static const char*	unknown="<unknown>";

/* Is this not in a header somewhere? */
extern const uchar* nular;

#define TIMEOUT_THREAD_WAIT		60		/* Seconds */
#define MAX_REQUEST_LINE		1024	/* NOT including terminator */
#define MAX_HEADERS_SIZE		16384	/* Maximum total size of all headers 
										   (Including terminator )*/
#define MAX_REDIR_LOOPS			20		/* Max. times to follow internal redirects for a single request */
#define MAX_POST_LEN			1048576	/* Max size of body for POSTS */

enum {
	 CLEANUP_SSJS_TMP_FILE
	,CLEANUP_POST_DATA
	,MAX_CLEANUPS
};

static scfg_t	scfg;
static BOOL		scfg_reloaded=TRUE;
static BOOL		http_logging_thread_running=FALSE;
static ulong	active_clients=0;
static ulong	sockets=0;
static BOOL		terminate_server=FALSE;
static BOOL		terminate_http_logging_thread=FALSE;
static uint		thread_count=0;
static SOCKET	server_socket=INVALID_SOCKET;
static char		revision[16];
static char		root_dir[MAX_PATH+1];
static char		error_dir[MAX_PATH+1];
static char		temp_dir[MAX_PATH+1];
static char		cgi_dir[MAX_PATH+1];
static time_t	uptime=0;
static DWORD	served=0;
static web_startup_t* startup=NULL;
static js_server_props_t js_server_props;
static link_list_t recycle_semfiles;
static link_list_t shutdown_semfiles;

static named_string_t** mime_types;

/* Logging stuff */
sem_t	log_sem;
link_list_t	log_list;
struct log_data {
	char	*hostname;
	char	*ident;
	char	*user;
	char	*request;
	char	*referrer;
	char	*agent;
	char	*vhost;
	int		status;
	unsigned int	size;
	struct tm completed;
};

typedef struct  {
	int			method;
	char		virtual_path[MAX_PATH+1];
	char		physical_path[MAX_PATH+1];
	BOOL		parsed_headers;
	BOOL    	expect_go_ahead;
	time_t		if_modified_since;
	BOOL		keep_alive;
	char		ars[256];
	char    	auth[128];				/* UserID:Password */
	char		host[128];				/* The requested host. (as used for self-referencing URLs) */
	char		vhost[128];				/* The requested host. (virtual host) */
	int			send_location;
	const char*	mime_type;
	link_list_t	headers;
	char		status[MAX_REQUEST_LINE+1];
	char *		post_data;
	size_t		post_len;
	int			dynamic;
	struct log_data	*ld;
	char		request_line[MAX_REQUEST_LINE+1];

	/* CGI parameters */
	char		query_str[MAX_REQUEST_LINE+1];
	char		extra_path_info[MAX_REQUEST_LINE+1];
	link_list_t	cgi_env;
	link_list_t	dynamic_heads;

	/* Dynamically (sever-side JS) generated HTML parameters */
	FILE*	fp;
	char		*cleanup_file[MAX_CLEANUPS];
	BOOL	sent_headers;
	BOOL	prev_write;
} http_request_t;

typedef struct  {
	SOCKET			socket;
	SOCKADDR_IN		addr;
	http_request_t	req;
	char			host_ip[64];
	char			host_name[128];	/* Resolved remote host */
	int				http_ver;       /* HTTP version.  0 = HTTP/0.9, 1=HTTP/1.0, 2=HTTP/1.1 */
	BOOL			finished;		/* Do not accept any more imput from client */
	user_t			user;
	int				last_user_num;
	time_t			logon_time;
	char			username[LEN_NAME+1];
	int				last_js_user_num;

	/* JavaScript parameters */
	JSRuntime*		js_runtime;
	JSContext*		js_cx;
	JSObject*		js_glob;
	JSObject*		js_query;
	JSObject*		js_header;
	JSObject*		js_request;
	js_branch_t		js_branch;
	subscan_t		*subscan;

	/* Client info */
	client_t		client;
} http_session_t;

enum { 
	 HTTP_0_9
	,HTTP_1_0
	,HTTP_1_1
};
static char* http_vers[] = {
	 ""
	,"HTTP/1.0"
	,"HTTP/1.1"
	,NULL	/* terminator */
};

enum { 
	 HTTP_HEAD
	,HTTP_GET
};

static char* methods[] = {
	 "HEAD"
	,"GET"
	,"POST"
	,NULL	/* terminator */
};

enum {
	 IS_STATIC
	,IS_CGI
	,IS_JS
	,IS_SSJS
};

enum { 
	 HEAD_DATE
	,HEAD_HOST
	,HEAD_IFMODIFIED
	,HEAD_LENGTH
	,HEAD_TYPE
	,HEAD_AUTH
	,HEAD_CONNECTION
	,HEAD_WWWAUTH
	,HEAD_STATUS
	,HEAD_ALLOW
	,HEAD_EXPIRES
	,HEAD_LASTMODIFIED
	,HEAD_LOCATION
	,HEAD_PRAGMA
	,HEAD_SERVER
	,HEAD_REFERER
	,HEAD_AGENT
};

static struct {
	int		id;
	char*	text;
} headers[] = {
	{ HEAD_DATE,			"Date"					},
	{ HEAD_HOST,			"Host"					},
	{ HEAD_IFMODIFIED,		"If-Modified-Since"		},
	{ HEAD_LENGTH,			"Content-Length"		},
	{ HEAD_TYPE,			"Content-Type"			},
	{ HEAD_AUTH,			"Authorization"			},
	{ HEAD_CONNECTION,		"Connection"			},
	{ HEAD_WWWAUTH,			"WWW-Authenticate"		},
	{ HEAD_STATUS,			"Status"				},
	{ HEAD_ALLOW,			"Allow"					},
	{ HEAD_EXPIRES,			"Expires"				},
	{ HEAD_LASTMODIFIED,	"Last-Modified"			},
	{ HEAD_LOCATION,		"Location"				},
	{ HEAD_PRAGMA,			"Pragma"				},
	{ HEAD_SERVER,			"Server"				},
	{ HEAD_REFERER,			"Referer"				},
	{ HEAD_AGENT,			"User-Agent"			},
	{ -1,					NULL /* terminator */	},
};

/* Everything MOVED_TEMP and everything after is a magical internal redirect */
enum  {
	 NO_LOCATION
	,MOVED_PERM
	,MOVED_TEMP
	,MOVED_STAT
};

static char	*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static char	*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static void respond(http_session_t * session);
static BOOL js_setup(http_session_t* session);
static char *find_last_slash(char *str);
static BOOL check_extra_path(http_session_t * session);
static BOOL exec_ssjs(http_session_t* session, char *script);
static BOOL ssjs_send_headers(http_session_t* session);

static time_t
sub_mkgmt(struct tm *tm)
{
        int y, nleapdays;
        time_t t;
        /* days before the month */
        static const unsigned short moff[12] = {
                0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
        };

        /*
         * XXX: This code assumes the given time to be normalized.
         * Normalizing here is impossible in case the given time is a leap
         * second but the local time library is ignorant of leap seconds.
         */

        /* minimal sanity checking not to access outside of the array */
        if ((unsigned) tm->tm_mon >= 12)
                return (time_t) -1;
        if (tm->tm_year < 1970 - 1900)
                return (time_t) -1;

        y = tm->tm_year + 1900 - (tm->tm_mon < 2);
        nleapdays = y / 4 - y / 100 + y / 400 -
            ((1970-1) / 4 - (1970-1) / 100 + (1970-1) / 400);
        t = ((((time_t) (tm->tm_year - (1970 - 1900)) * 365 +
                        moff[tm->tm_mon] + tm->tm_mday - 1 + nleapdays) * 24 +
                tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

        return (t < 0 ? (time_t) -1 : t);
}

time_t
time_gm(struct tm *tm)
{
        time_t t, t2;
        struct tm *tm2;
        int sec;

        /* Do the first guess. */
        if ((t = sub_mkgmt(tm)) == (time_t) -1)
                return (time_t) -1;

        /* save value in case *tm is overwritten by gmtime() */
        sec = tm->tm_sec;

        tm2 = gmtime(&t);
        if ((t2 = sub_mkgmt(tm2)) == (time_t) -1)
                return (time_t) -1;

        if (t2 < t || tm2->tm_sec != sec) {
                /*
                 * Adjust for leap seconds.
                 *
                 *     real time_t time
                 *           |
                 *          tm
                 *         /        ... (a) first sub_mkgmt() conversion
                 *       t
                 *       |
                 *      tm2
                 *     /        ... (b) second sub_mkgmt() conversion
                 *   t2
                 *                        --->time
                 */
                /*
                 * Do the second guess, assuming (a) and (b) are almost equal.
                 */
                t += t - t2;
                tm2 = gmtime(&t);

                /*
                 * Either (a) or (b), may include one or two extra
                 * leap seconds.  Try t, t + 2, t - 2, t + 1, and t - 1.
                 */
                if (tm2->tm_sec == sec
                    || (t += 2, tm2 = gmtime(&t), tm2->tm_sec == sec)
                    || (t -= 4, tm2 = gmtime(&t), tm2->tm_sec == sec)
                    || (t += 3, tm2 = gmtime(&t), tm2->tm_sec == sec)
                    || (t -= 2, tm2 = gmtime(&t), tm2->tm_sec == sec))
                        ;        /* found */
                else {
                        /*
                         * Not found.
                         */
                        if (sec >= 60)
                                /*
                                 * The given time is a leap second
                                 * (sec 60 or 61), but the time library
                                 * is ignorant of the leap second.
                                 */
                                ;        /* treat sec 60 as 59,
                                           sec 61 as 0 of the next minute */
                        else
                                /* The given time may not be normalized. */
                                t++;        /* restore t */
                }
        }

        return (t < 0 ? (time_t) -1 : t);
}

static int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->lputs==NULL)
        return(0);

	va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(startup->lputs(startup->cbdata,level,sbuf));
}

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_INFO,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    lprintf(LOG_ERR,"!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(startup->cbdata,str);
}

static void update_clients(void)
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(startup->cbdata,active_clients);
}

static void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,FALSE,sock,NULL,FALSE);
}

static void thread_up(BOOL setuid)
{
	thread_count++;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,TRUE, setuid);
}

static void thread_down(void)
{
	if(thread_count>0)
		thread_count--;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,FALSE, FALSE);
}

/*********************************************************************/
/* Adds an environment variable to the sessions  cgi_env linked list */
/*********************************************************************/
static void add_env(http_session_t *session, const char *name,const char *value)  {
	char	newname[129];
	char	*p;

	if(name==NULL || value==NULL)  {
		lprintf(LOG_WARNING,"%04d Attempt to set NULL env variable", session->socket);
		return;
	}
	SAFECOPY(newname,name);

	for(p=newname;*p;p++)  {
		*p=toupper(*p);
		if(*p=='-')
			*p='_';
	}
	p=(char *)malloc(strlen(name)+strlen(value)+2);
	if(p==NULL) {
		lprintf(LOG_WARNING,"%04d Cannot allocate memory for string", session->socket);
		return;
	}
	sprintf(p,"%s=%s",newname,value);
	listPushNodeString(&session->req.cgi_env,p);
	free(p);
}

/***************************************/
/* Initializes default CGI envirnoment */
/***************************************/
static void init_enviro(http_session_t *session)  {
	char	str[128];

	add_env(session,"SERVER_SOFTWARE",VERSION_NOTICE);
	sprintf(str,"%d",startup->port);
	add_env(session,"SERVER_PORT",str);
	add_env(session,"GATEWAY_INTERFACE","CGI/1.1");
	if(!strcmp(session->host_name,session->host_ip))
		add_env(session,"REMOTE_HOST",session->host_name);
	add_env(session,"REMOTE_ADDR",session->host_ip);
}

/*
 * Sends string str to socket sock... returns number of bytes written, or 0 on an error
 * (Should it be -1 on an error?)
 * Can not close the socket since it can not set it to INVALID_SOCKET
 * ToDo - Decide error behaviour, should a SOCKET * be passed around rather than a socket?
 */
static int sockprint(SOCKET sock, const char *str)
{
	int len;
	int	result;
	int written=0;
	BOOL	wr;

	if(sock==INVALID_SOCKET)
		return(0);
	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG,"%04d TX: %s", sock, str);
	len=strlen(str);

	while(socket_check(sock,NULL,&wr,startup->max_inactivity*1000) && wr && written<len)  {
		result=sendsocket(sock,str+written,len-written);
		if(result==SOCKET_ERROR) {
			if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"%04d Connection reset by peer on send",sock);
			else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"%04d Connection aborted by peer on send",sock);
			else
				lprintf(LOG_WARNING,"%04d !ERROR %d sending on socket",sock,ERROR_VALUE);
			return(0);
		}
		written+=result;
	}
	if(written != len) {
		lprintf(LOG_WARNING,"%04d !ERROR %d sending on socket",sock,ERROR_VALUE);
		return(0);
	}
	return(len);
}

/**********************************************************/
/* Converts a month name/abbr to the 0-based month number */
/* ToDo: This probobly exists somewhere else already	  */
/**********************************************************/
static int getmonth(char *mon)
{
	int	i;
	for(i=0;i<12;i++)
		if(!stricmp(mon,months[i]))
			return(i);

	return 0;
}

/*******************************************************************/
/* Converts a date string in any of the common formats to a time_t */
/*******************************************************************/
static time_t decode_date(char *date)
{
	struct	tm	ti;
	char	*token;
	time_t	t;

	ti.tm_sec=0;		/* seconds (0 - 60) */
	ti.tm_min=0;		/* minutes (0 - 59) */
	ti.tm_hour=0;		/* hours (0 - 23) */
	ti.tm_mday=1;		/* day of month (1 - 31) */
	ti.tm_mon=0;		/* month of year (0 - 11) */
	ti.tm_year=0;		/* year - 1900 */
	ti.tm_isdst=0;		/* is summer time in effect? */

	token=strtok(date,",");
	if(token==NULL)
		return(0);
	/* This probobly only needs to be 9, but the extra one is for luck. */
	if(strlen(date)>15) {
		/* asctime() */
		/* Toss away week day */
		token=strtok(date," ");
		if(token==NULL)
			return(0);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_mday=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_sec=atoi(token);
		token=strtok(NULL,"");
		if(token==NULL)
			return(0);
		ti.tm_year=atoi(token)-1900;
	}
	else  {
		/* RFC 1123 or RFC 850 */
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		ti.tm_mday=atoi(token);
		token=strtok(NULL," -");
		if(token==NULL)
			return(0);
		ti.tm_mon=getmonth(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_year=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_hour=atoi(token);
		token=strtok(NULL,":");
		if(token==NULL)
			return(0);
		ti.tm_min=atoi(token);
		token=strtok(NULL," ");
		if(token==NULL)
			return(0);
		ti.tm_sec=atoi(token);
		if(ti.tm_year>1900)
			ti.tm_year -= 1900;
	}

	t=time_gm(&ti);
	return(t);
}

static SOCKET open_socket(int type)
{
	char	error[256];
	SOCKET	sock;

	sock=socket(AF_INET, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(startup->cbdata,TRUE);
	if(sock!=INVALID_SOCKET) {
		if(set_socket_options(&scfg, sock,error))
			lprintf(LOG_ERR,"%04d !ERROR %s",sock,error);

		sockets++;
	}
	return(sock);
}

static int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET)
		return(-1);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(startup!=NULL && startup->socket_open!=NULL) {
		startup->socket_open(startup->cbdata,FALSE);
	}
	sockets--;
	if(result!=0) {
		if(ERROR_VALUE!=ENOTSOCK)
			lprintf(LOG_WARNING,"%04d !ERROR %d closing socket",sock, ERROR_VALUE);
	}

	return(result);
}

/**************************************************/
/* End of a single request...					  */
/* This is called at the end of EVERY request	  */
/*  Log the request       						  */
/*  Free request-specific data ie: dynamic stuff  */
/*  Close socket unless it's being kept alive     */
/*   If the socket is closed, the session is done */
/**************************************************/
static void close_request(http_session_t * session)
{
	time_t		now;
	int			i;

	if(session->req.ld!=NULL) {
		now=time(NULL);
		localtime_r(&now,&session->req.ld->completed);
		listPushNode(&log_list,session->req.ld);
		sem_post(&log_sem);
		session->req.ld=NULL;
	}

	listFree(&session->req.headers);
	listFree(&session->req.dynamic_heads);
	listFree(&session->req.cgi_env);
	FREE_AND_NULL(session->req.post_data);
	if(!session->req.keep_alive) {
		close_socket(session->socket);
		session->socket=INVALID_SOCKET;
	}
	if(session->socket==INVALID_SOCKET)
		session->finished=TRUE;

	if(session->js_cx!=NULL && (session->req.dynamic==IS_SSJS || session->req.dynamic==IS_JS)) {
		JS_GC(session->js_cx);
	}
	if(session->subscan!=NULL)
		putmsgptrs(&scfg, session->user.number, session->subscan);

	if(session->req.fp!=NULL)
		fclose(session->req.fp);

	for(i=0;i<MAX_CLEANUPS;i++) {
		if(session->req.cleanup_file[i]!=NULL) {
			if(!(startup->options&WEB_OPT_DEBUG_SSJS))
				remove(session->req.cleanup_file[i]);
			free(session->req.cleanup_file[i]);
		}
	}

	memset(&session->req,0,sizeof(session->req));
}

static int get_header_type(char *header)
{
	int i;
	for(i=0; headers[i].text!=NULL; i++) {
		if(!stricmp(header,headers[i].text)) {
			return(headers[i].id);
		}
	}
	return(-1);
}

/* Opposite of get_header_type() */
static char *get_header(int id) 
{
	int i;
	if(headers[id].id==id)
		return(headers[id].text);

	for(i=0;headers[i].text!=NULL;i++) {
		if(headers[i].id==id) {
			return(headers[i].text);
		}
	}
	return(NULL);
}

static const char* unknown_mime_type="application/octet-stream";

static const char* get_mime_type(char *ext)
{
	uint i;

	if(ext==NULL)
		return(unknown_mime_type);

	for(i=0;mime_types[i]!=NULL;i++)
		if(!stricmp(ext+1,mime_types[i]->name))
			return(mime_types[i]->value);

	return(unknown_mime_type);
}

/* This function appends append plus a newline IF the final dst string would have a length less than maxlen */
static void safecat(char *dst, const char *append, size_t maxlen) {
	size_t dstlen,appendlen;
	dstlen=strlen(dst);
	appendlen=strlen(append);
	if(dstlen+appendlen+2 < maxlen) {
		strcat(dst,append);
		strcat(dst,newline);
	}
}

/*************************************************/
/* Sends headers for the reply.					 */
/* HTTP/0.9 doesn't use headers, so just returns */
/*************************************************/
static BOOL send_headers(http_session_t *session, const char *status)
{
	int		ret;
	BOOL	send_file=TRUE;
	time_t	ti;
	const char	*status_line;
	struct stat	stats;
	struct tm	tm;
	char	*headers;
	char	header[MAX_REQUEST_LINE+1];
	list_node_t	*node;

	if(session->socket==INVALID_SOCKET)
		return(FALSE);
	lprintf(LOG_DEBUG,"%04d Request resolved to: %s"
		,session->socket,session->req.physical_path);
	if(session->http_ver <= HTTP_0_9) {
		if(session->req.ld != NULL)
			session->req.ld->status=atoi(status);
		return(TRUE);
	}

	status_line=status;
	ret=stat(session->req.physical_path,&stats);
	if(!ret && session->req.if_modified_since && (stats.st_mtime <= session->req.if_modified_since) && !session->req.dynamic) {
		status_line="304 Not Modified";
		ret=-1;
		send_file=FALSE;
	}
	if(session->req.send_location==MOVED_PERM)  {
		status_line=error_301;
		ret=-1;
		send_file=FALSE;
	}
	if(session->req.send_location==MOVED_TEMP)  {
		status_line=error_302;
		ret=-1;
		send_file=FALSE;
	}

	if(session->req.ld!=NULL)
		session->req.ld->status=atoi(status_line);

	headers=malloc(MAX_HEADERS_SIZE);
	if(headers==NULL)  {
		lprintf(LOG_CRIT,"Could not allocate memory for response headers.");
		return(FALSE);
	}
	*headers=0;
	/* Status-Line */
	safe_snprintf(header,sizeof(header),"%s %s",http_vers[session->http_ver],status_line);

	lprintf(LOG_DEBUG,"%04d Result: %s",session->socket,header);

	safecat(headers,header,MAX_HEADERS_SIZE);

	/* General Headers */
	ti=time(NULL);
	if(gmtime_r(&ti,&tm)==NULL)
		memset(&tm,0,sizeof(tm));
	safe_snprintf(header,sizeof(header),"%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
		,get_header(HEAD_DATE)
		,days[tm.tm_wday],tm.tm_mday,months[tm.tm_mon]
		,tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
	safecat(headers,header,MAX_HEADERS_SIZE);
	if(session->req.keep_alive) {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_CONNECTION),"Keep-Alive");
		safecat(headers,header,MAX_HEADERS_SIZE);
	}
	else {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_CONNECTION),"Close");
		safecat(headers,header,MAX_HEADERS_SIZE);
	}

	/* Response Headers */
	safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_SERVER),VERSION_NOTICE);
	safecat(headers,header,MAX_HEADERS_SIZE);
	
	/* Entity Headers */
	if(session->req.dynamic) {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_ALLOW),"GET, HEAD, POST");
		safecat(headers,header,MAX_HEADERS_SIZE);
	}
	else {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_ALLOW),"GET, HEAD");
		safecat(headers,header,MAX_HEADERS_SIZE);
	}

	if(session->req.send_location) {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_LOCATION),(session->req.virtual_path));
		safecat(headers,header,MAX_HEADERS_SIZE);
	}
	if(session->req.keep_alive) {
		if(ret)  {
			safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_LENGTH),"0");
			safecat(headers,header,MAX_HEADERS_SIZE);
		}
		else  {
			safe_snprintf(header,sizeof(header),"%s: %d",get_header(HEAD_LENGTH),(int)stats.st_size);
			safecat(headers,header,MAX_HEADERS_SIZE);
		}
	}

	if(!ret && !session->req.dynamic)  {
		safe_snprintf(header,sizeof(header),"%s: %s",get_header(HEAD_TYPE),session->req.mime_type);
		safecat(headers,header,MAX_HEADERS_SIZE);
		gmtime_r(&stats.st_mtime,&tm);
		safe_snprintf(header,sizeof(header),"%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
			,get_header(HEAD_LASTMODIFIED)
			,days[tm.tm_wday],tm.tm_mday,months[tm.tm_mon]
			,tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
		safecat(headers,header,MAX_HEADERS_SIZE);
	} 

	if(session->req.dynamic)  {
		/* Dynamic headers */
		/* Set up environment */
		for(node=listFirstNode(&session->req.dynamic_heads);node!=NULL;node=listNextNode(node))
			safecat(headers,listNodeData(node),MAX_HEADERS_SIZE);
	}

	safecat(headers,"",MAX_HEADERS_SIZE);
	send_file = (sockprint(session->socket,headers) && send_file);
	FREE_AND_NULL(headers);
	return(send_file);
}

static int sock_sendfile(SOCKET socket,char *path)
{
	int		file;
	long	offset=0;
	int		ret=0;

	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG,"%04d Sending %s",socket,path);
	if((file=open(path,O_RDONLY|O_BINARY))==-1)
		lprintf(LOG_WARNING,"%04d !ERROR %d opening %s",socket,errno,path);
	else {
		if((ret=sendfilesocket(socket, file, &offset, 0)) < 0) {
			lprintf(LOG_DEBUG,"%04d !ERROR %d sending %s"
				, socket, errno, path);
			ret=0;
		}
		close(file);
	}
	return(ret);
}

/********************************************************/
/* Sends a specified error message, closes the request, */
/* and marks the session to be closed 					*/
/********************************************************/
static void send_error(http_session_t * session, const char* message)
{
	char	error_code[4];
	struct stat	sb;
	char	sbuf[1024];
	BOOL	sent_ssjs=FALSE;

	if(session->socket==INVALID_SOCKET)
		return;
	session->req.if_modified_since=0;
	lprintf(LOG_INFO,"%04d !ERROR: %s",session->socket,message);
	session->req.keep_alive=FALSE;
	session->req.send_location=NO_LOCATION;
	SAFECOPY(error_code,message);
	SAFECOPY(session->req.status,message);
	if(atoi(error_code)<500) {
		/*
		 * Attempt to run SSJS error pages
		 * If this fails, do the standard error page instead,
		 * ie: Don't "upgrade" to a 500 error
		 */

		sprintf(sbuf,"%s%s.ssjs",error_dir,error_code);
		if(!stat(sbuf,&sb)) {
			lprintf(LOG_INFO,"%04d Using SSJS error page",session->socket);
			session->req.dynamic=IS_SSJS;
			if(js_setup(session)) {
				sent_ssjs=exec_ssjs(session,sbuf);
				if(sent_ssjs) {
					int	snt=0;

					lprintf(LOG_INFO,"%04d Sending generated error page",session->socket);
					snt=sock_sendfile(session->socket,session->req.physical_path);
					if(snt<0)
						snt=0;
					if(session->req.ld!=NULL)
						session->req.ld->size=snt;
				}
				else
					 session->req.dynamic=IS_STATIC;
			}
			else
				session->req.dynamic=IS_STATIC;
		}
	}
	if(!sent_ssjs) {
		sprintf(session->req.physical_path,"%s%s.html",error_dir,error_code);
		session->req.mime_type=get_mime_type(strrchr(session->req.physical_path,'.'));
		send_headers(session,message);
		if(!stat(session->req.physical_path,&sb)) {
			int	snt=0;
			snt=sock_sendfile(session->socket,session->req.physical_path);
			if(snt<0)
				snt=0;
			if(session->req.ld!=NULL)
				session->req.ld->size=snt;
		}
		else {
			lprintf(LOG_NOTICE,"%04d Error message file %s doesn't exist"
				,session->socket,session->req.physical_path);
			safe_snprintf(sbuf,sizeof(sbuf)
				,"<HTML><HEAD><TITLE>%s Error</TITLE></HEAD>"
				"<BODY><H1>%s Error</H1><BR><H3>In addition, "
				"I can't seem to find the %s error file</H3><br>"
				"please notify <a href=\"mailto:sysop@%s\">"
				"%s</a></BODY></HTML>"
				,error_code,error_code,error_code,scfg.sys_inetaddr,scfg.sys_op);
			sockprint(session->socket,sbuf);
			if(session->req.ld!=NULL)
				session->req.ld->size=strlen(sbuf);
		}
	}
	close_request(session);
}

void http_logon(http_session_t * session, user_t *usr)
{
	char	str[128];

	if(usr==NULL)
		getuserdat(&scfg, &session->user);
	else
		session->user=*usr;

	if(session->user.number==session->last_user_num)
		return;

	lprintf(LOG_DEBUG,"%04d HTTP Logon (user #%d)",session->socket,session->user.number);

	if(session->subscan!=NULL)
		getmsgptrs(&scfg,session->user.number,session->subscan);

	session->logon_time=time(NULL);
	if(session->user.number==0)
		SAFECOPY(session->username,unknown);
	else {
		SAFECOPY(session->username,session->user.alias);
		/* Adjust Connect and host */
		putuserrec(&scfg,session->user.number,U_MODEM,LEN_MODEM,"HTTP");
		putuserrec(&scfg,session->user.number,U_COMP,LEN_COMP,session->host_name);
		putuserrec(&scfg,session->user.number,U_NOTE,LEN_NOTE,session->host_ip);
		putuserrec(&scfg,session->user.number,U_LOGONTIME,0,ultoa(session->logon_time,str,16));
	}
	session->client.user=session->username;
	client_on(session->socket, &session->client, /* update existing client record? */TRUE);

	session->last_user_num=session->user.number;
}

void http_logoff(http_session_t* session, SOCKET socket, int line)
{
	if(session->last_user_num<=0)
		return;

	lprintf(LOG_DEBUG,"%04d HTTP Logoff (user #%d) from line %d"
		,socket,session->user.number, line);

	SAFECOPY(session->username,unknown);
	logoutuserdat(&scfg, &session->user, time(NULL), session->logon_time);
	memset(&session->user,0,sizeof(session->user));
	session->last_user_num=session->user.number;
}

BOOL http_checkuser(http_session_t * session)
{
	if(session->req.dynamic==IS_SSJS || session->req.dynamic==IS_JS) {
		if(session->last_js_user_num==session->user.number)
			return(TRUE);
		lprintf(LOG_INFO,"%04d JavaScript: Initializing User Objects",session->socket);
		if(session->user.number>0) {
			if(!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user
				,NULL /* ftp index file */, session->subscan /* subscan */)) {
				lprintf(LOG_ERR,"%04d !JavaScript ERROR creating user objects",session->socket);
				send_error(session,"500 Error initializing JavaScript User Objects");
				return(FALSE);
			}
		}
		else {
			if(!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, NULL
				,NULL /* ftp index file */, session->subscan /* subscan */)) {
				lprintf(LOG_ERR,"%04d !ERROR initializing JavaScript User Objects",session->socket);
				send_error(session,"500 Error initializing JavaScript User Objects");
				return(FALSE);
			}
		}
		session->last_js_user_num=session->user.number;
	}
	return(TRUE);
}

static BOOL check_ars(http_session_t * session)
{
	char	*username;
	char	*password;
	uchar	*ar;
	BOOL	authorized;
	char	auth_req[MAX_REQUEST_LINE+1];
	int		i;
	user_t	thisuser;

	if(session->req.auth[0]==0) {
		/* No authentication information... */
		if(session->last_user_num!=0) {
			if(session->last_user_num>0)
				http_logoff(session,session->socket,__LINE__);
			session->user.number=0;
			http_logon(session,NULL);
		}
		if(!http_checkuser(session))
			return(FALSE);
		if(session->req.ars[0]) {
			/* There *IS* an ARS string  ie: Auth is required */
			if(startup->options&WEB_OPT_DEBUG_RX)
				lprintf(LOG_NOTICE,"%04d !No authentication information",session->socket);
			return(FALSE);
		}
		/* No auth required, allow */
		return(TRUE);
	}
	SAFECOPY(auth_req,session->req.auth);

	username=strtok(auth_req,":");
	if(username==NULL)
		username="";
	password=strtok(NULL,":");
	/* Require a password */
	if(password==NULL)
		password="";
	i=matchuser(&scfg, username, FALSE);
	if(i==0) {
		if(session->last_user_num!=0) {
			if(session->last_user_num>0)
				http_logoff(session,session->socket,__LINE__);
			session->user.number=0;
			http_logon(session,NULL);
		}
		if(!http_checkuser(session))
			return(FALSE);
		if(scfg.sys_misc&SM_ECHO_PW)
			lprintf(LOG_NOTICE,"%04d !UNKNOWN USER: %s, Password: %s"
				,session->socket,username,password);
		else
			lprintf(LOG_NOTICE,"%04d !UNKNOWN USER: %s"
				,session->socket,username);
		return(FALSE);
	}
	thisuser.number=i;
	getuserdat(&scfg, &thisuser);
	if(thisuser.pass[0] && stricmp(thisuser.pass,password)) {
		if(session->last_user_num!=0) {
			if(session->last_user_num>0)
				http_logoff(session,session->socket,__LINE__);
			session->user.number=0;
			http_logon(session,NULL);
		}
		if(!http_checkuser(session))
			return(FALSE);
		/* Should go to the hack log? */
		if(scfg.sys_misc&SM_ECHO_PW)
			lprintf(LOG_WARNING,"%04d !PASSWORD FAILURE for user %s: '%s' expected '%s'"
				,session->socket,username,password,session->user.pass);
		else
			lprintf(LOG_WARNING,"%04d !PASSWORD FAILURE for user %s"
				,session->socket,username);
#ifdef _WIN32
		if(startup->hack_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
			PlaySound(startup->hack_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif
		return(FALSE);
	}

	if(i != session->last_user_num) {
		http_logoff(session,session->socket,__LINE__);
		session->user.number=i;
		http_logon(session,&thisuser);
	}
	if(!http_checkuser(session))
		return(FALSE);

	if(session->req.ld!=NULL)
		session->req.ld->user=strdup(username);

	ar = arstr(NULL,session->req.ars,&scfg);
	authorized=chk_ar(&scfg,ar,&session->user);
	if(ar!=NULL && ar!=nular)
		FREE_AND_NULL(ar);

	if(authorized)  {
		if(session->req.dynamic==IS_CGI)  {
			add_env(session,"AUTH_TYPE","Basic");
			/* Should use real name if set to do so somewhere ToDo */
			add_env(session,"REMOTE_USER",session->user.alias);
		}

		return(TRUE);
	}

	/* Should go to the hack log? */
	lprintf(LOG_WARNING,"%04d !AUTHORIZATION FAILURE for user %s, ARS: %s"
		,session->socket,username,session->req.ars);

#ifdef _WIN32
	if(startup->hack_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
		PlaySound(startup->hack_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	return(FALSE);
}

static BOOL read_mime_types(char* fname)
{
	int		mime_count;
	FILE*	fp;

	mime_types=iniFreeNamedStringList(mime_types);

	lprintf(LOG_DEBUG,"Reading %s",fname);
	if((fp=iniOpenFile(fname))==NULL) {
		lprintf(LOG_WARNING,"Error %d opening %s",errno,fname);
		return(FALSE);
	}
	mime_types=iniReadNamedStringList(fp,NULL /* root section */);
	iniCloseFile(fp);

	COUNT_LIST_ITEMS(mime_types,mime_count);
	lprintf(LOG_DEBUG,"Loaded %d mime types", mime_count);
	return(mime_count>0);
}

static int sockreadline(http_session_t * session, char *buf, size_t length)
{
	char	ch;
	DWORD	i;
	BOOL	rd;
	DWORD	chucked=0;

	for(i=0;TRUE;) {
		if(!socket_check(session->socket,&rd,NULL,startup->max_inactivity*1000) 
			|| !rd || recv(session->socket, &ch, 1, 0)!=1)  {
			session->req.keep_alive=FALSE;
			close_request(session);
			return(-1);        /* time-out */
		}

		if(ch=='\n')
			break;

		if(i<length)
			buf[i++]=ch;
		else
			chucked++;
	}

	/* Terminate at length if longer */
	if(i>length)
		i=length;
		
	if(i>0 && buf[i-1]=='\r')
		buf[--i]=0;
	else
		buf[i]=0;

	if(startup->options&WEB_OPT_DEBUG_RX) {
		lprintf(LOG_DEBUG,"%04d RX: %s",session->socket,buf);
		if(chucked)
			lprintf(LOG_DEBUG,"%04d Long header, chucked %d bytes",session->socket,chucked);
	}
	return(i);
}

#if defined(_WIN32)
static int pipereadline(HANDLE pipe, char *buf, size_t length)
#else
static int pipereadline(int pipe, char *buf, size_t length)
#endif
{
	char	ch;
	DWORD	i;
	time_t	start;
	int		ret=0;

	start=time(NULL);
	for(i=0;TRUE;) {
		if(time(NULL)-start>startup->max_cgi_inactivity)
			return(-1);

#if defined(_WIN32)
		ret=0;
		ReadFile(pipe, &ch, 1, &ret, NULL);
#else
		ret=read(pipe, &ch, 1);
#endif
		if(ret==1)  {
			start=time(NULL);

			if(ch=='\n')
				break;

			if(i<length)
				buf[i++]=ch;
		}
		else
			return(-1);
	}

	/* Terminate at length if longer */
	if(i>length)
		i=length;

	if(i>0 && buf[i-1]=='\r')
		buf[--i]=0;
	else
		buf[i]=0;

	return(i);
}

int recvbufsocket(int sock, char *buf, long count)
{
	int		rd=0;
	int		i;
	time_t	start;

	if(count<1) {
		errno=ERANGE;
		return(0);
	}

	while(rd<count && socket_check(sock,NULL,NULL,startup->max_inactivity*1000))  {
		i=recv(sock,buf+rd,count-rd,0);
		if(i<=0)  {
			*buf=0;
			return(0);
		}

		rd+=i;
		start=time(NULL);
	}

	if(rd==count)  {
		return(rd);
	}

	*buf=0;
	return(0);
}

/* Wasn't this done up as a JS thing too?  ToDo */
static void unescape(char *p)
{
	char *	dst;
	char	code[3];
	
	dst=p;
	for(;*p;p++) {
		if(*p=='%' && isxdigit(*(p+1)) && isxdigit(*(p+2))) {
			sprintf(code,"%.2s",p+1);
			*(dst++)=(char)strtol(code,NULL,16);
			p+=2;
		}
		else  {
			if(*p=='+')  {
				*(dst++)=' ';
			}
			else  {
				*(dst++)=*p;
			}
		}
	}
	*(dst)=0;
}

static void js_add_queryval(http_session_t * session, char *key, char *value)
{
	JSObject*	keyarray;
	jsval		val;
	jsuint		len;
	int			alen;

	/* Return existing object if it's already been created */
	if(JS_GetProperty(session->js_cx,session->js_query,key,&val) && val!=JSVAL_VOID)  {
		keyarray = JSVAL_TO_OBJECT(val);
		alen=-1;
	}
	else {
		keyarray = JS_NewArrayObject(session->js_cx, 0, NULL);
		if(!JS_DefineProperty(session->js_cx, session->js_query, key, OBJECT_TO_JSVAL(keyarray)
			, NULL, NULL, JSPROP_ENUMERATE))
			return;
		alen=0;
	}

	if(alen==-1) {
		if(JS_GetArrayLength(session->js_cx, keyarray, &len)==JS_FALSE)
			return;
		alen=len;
	}

	lprintf(LOG_DEBUG,"%04d Adding query value %s=%s at pos %d",session->socket,key,value,alen);
	val=STRING_TO_JSVAL(JS_NewStringCopyZ(session->js_cx,value));
	JS_SetElement(session->js_cx, keyarray, alen, &val);
}

static void js_add_request_prop(http_session_t * session, char *key, char *value)  
{
	JSString*	js_str;

	if(session->js_cx==NULL || session->js_request==NULL)
		return;
	if(key==NULL || value==NULL)
		return;
	if((js_str=JS_NewStringCopyZ(session->js_cx, value))==NULL)
		return;
	JS_DefineProperty(session->js_cx, session->js_request, key, STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
}

static void js_add_header(http_session_t * session, char *key, char *value)  
{
	JSString*	js_str;
	char		*lckey;

	if((lckey=(char *)malloc(strlen(key)+1))==NULL)
		return;
	strcpy(lckey,key);
	strlwr(lckey);
	if((js_str=JS_NewStringCopyZ(session->js_cx, value))==NULL) {
		free(lckey);
		return;
	}
	JS_DefineProperty(session->js_cx, session->js_header, lckey, STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	free(lckey);
}

static void js_parse_query(http_session_t * session, char *p)  {
	size_t		key_len;
	size_t		value_len;
	char		*lp;
	char		*key;
	char		*value;

	if(p == NULL)
		return;

	lp=p;

	while((key_len=strcspn(lp,"="))!=0)  {
		key=lp;
		lp+=key_len;
		if(*lp) {
			*lp=0;
			lp++;
		}
		value_len=strcspn(lp,"&");
		value=lp;
		lp+=value_len;
		if(*lp) {
			*lp=0;
			lp++;
		}
		unescape(value);
		unescape(key);
		js_add_queryval(session, key, value);
	}
}

static BOOL parse_headers(http_session_t * session)
{
	char	*head_line;
	char	*value;
	char	*p;
	int		i;
	size_t	content_len=0;
	char	env_name[128];
	list_node_t	*node;

	for(node=listFirstNode(&session->req.headers);node!=NULL;node=listNextNode(node)) {
		head_line=listNodeData(node);
		if((strtok(head_line,":"))!=NULL && (value=strtok(NULL,""))!=NULL) {
			i=get_header_type(head_line);
			while(*value && *value<=' ') value++;
			if(session->req.dynamic==IS_SSJS || session->req.dynamic==IS_JS)
				js_add_header(session,head_line,value);
			switch(i) {
				case HEAD_AUTH:
					strtok(value," ");
					p=strtok(NULL," ");
					if(p==NULL)
						break;
					while(*p && *p<' ') p++;
					b64_decode(session->req.auth,sizeof(session->req.auth),p,strlen(p));
					break;
				case HEAD_LENGTH:
					if(session->req.dynamic==IS_CGI)
						add_env(session,"CONTENT_LENGTH",value);
					content_len=atoi(value);
					break;
				case HEAD_TYPE:
					if(session->req.dynamic==IS_CGI)
						add_env(session,"CONTENT_TYPE",value);
					break;
				case HEAD_IFMODIFIED:
					session->req.if_modified_since=decode_date(value);
					break;
				case HEAD_CONNECTION:
					if(!stricmp(value,"Keep-Alive")) {
						session->req.keep_alive=TRUE;
					}
					if(!stricmp(value,"Close")) {
						session->req.keep_alive=FALSE;
					}
					break;
				case HEAD_REFERER:
					if(session->req.ld!=NULL)
						session->req.ld->referrer=strdup(value);
					break;
				case HEAD_AGENT:
					if(session->req.ld!=NULL)
						session->req.ld->agent=strdup(value);
					break;
				default:
					break;
			}
			if(session->req.dynamic==IS_CGI)  {
				sprintf(env_name,"HTTP_%s",head_line);
				add_env(session,env_name,value);
			}
		}
	}
	if(content_len && session->req.dynamic != IS_CGI)  {
		if(content_len < (MAX_POST_LEN+1) && (session->req.post_data=malloc(content_len+1)) != NULL)  {
			session->req.post_len=recvbufsocket(session->socket,session->req.post_data,content_len);
			if(session->req.post_len != content_len)
				lprintf(LOG_DEBUG,"%04d !ERROR Browser said they sent %d bytes, but I got %d",session->socket,content_len,session->req.post_len);
			if(session->req.post_len > content_len)
				session->req.post_len = content_len;
			session->req.post_data[session->req.post_len]=0;
			if(session->req.dynamic==IS_SSJS || session->req.dynamic==IS_JS)  {
				js_add_request_prop(session,"post_data",session->req.post_data);
				js_parse_query(session,session->req.post_data);
			}
		}
		else  {
			lprintf(LOG_CRIT,"%04d !ERROR Allocating %d bytes of memory",session->socket,content_len);
			send_error(session,"413 Request entity too large");
			return(FALSE);
		}
	}
	if(session->req.dynamic==IS_CGI)
		add_env(session,"SERVER_NAME",session->req.host[0] ? session->req.host : startup->host_name );
	return TRUE;
}

static int get_version(char *p)
{
	int		i;
	if(p==NULL)
		return(0);
	while(*p && *p<' ') p++;
	if(*p==0)
		return(0);
	for(i=1;http_vers[i]!=NULL;i++) {
		if(!stricmp(p,http_vers[i])) {
			return(i);
		}
	}
	return(i-1);
}

static int is_dynamic_req(http_session_t* session)
{
	int		i=0;
	char	drive[4];
	char	cgidrive[4];
	char	dir[MAX_PATH+1];
	char	cgidir[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ext[MAX_PATH+1];

	check_extra_path(session);
	_splitpath(session->req.physical_path, drive, dir, fname, ext);

	if(stricmp(ext,startup->ssjs_ext)==0)
		i=IS_SSJS;
	else if(stricmp(ext,startup->js_ext)==0)
		i=IS_JS;
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT) && i)  {
		lprintf(LOG_INFO,"%04d Setting up JavaScript support", session->socket);
		if(!js_setup(session)) {
			lprintf(LOG_ERR,"%04d !ERROR setting up JavaScript support", session->socket);
			send_error(session,error_500);
			return(IS_STATIC);
		}

		return(i);
	}

	if(!(startup->options&WEB_OPT_NO_CGI)) {
		for(i=0; startup->cgi_ext!=NULL && startup->cgi_ext[i]!=NULL; i++)  {
			if(stricmp(ext,startup->cgi_ext[i])==0)  {
				init_enviro(session);
				return(IS_CGI);
			}
		}
		_splitpath(cgi_dir, cgidrive, cgidir, fname, ext);
		if(stricmp(dir,cgidir)==0 && stricmp(drive,cgidrive)==0)  {
			init_enviro(session);
			return(IS_CGI);
		}
	}

	return(IS_STATIC);
}

static char *get_request(http_session_t * session, char *req_line)
{
	char*	p;
	char*	query;
	char*	retval;
	int		offset;

	SKIP_WHITESPACE(req_line);
	SAFECOPY(session->req.virtual_path,req_line);
	strtok(session->req.virtual_path," \t");
	SAFECOPY(session->req.request_line,session->req.virtual_path);
	retval=strtok(NULL," \t");
	strtok(session->req.virtual_path,"?");
	query=strtok(NULL,"");

	/* Must initialize physical_path before calling is_dynamic_req() */
	SAFECOPY(session->req.physical_path,session->req.virtual_path);
	unescape(session->req.physical_path);
	if(!strnicmp(session->req.physical_path,http_scheme,http_scheme_len)) {
		/* Set HOST value... ignore HOST header */
		SAFECOPY(session->req.host,session->req.physical_path+http_scheme_len);
		SAFECOPY(session->req.vhost,session->req.host);
		/* Remove port specification */
		strtok(session->req.vhost,":");
		strtok(session->req.physical_path,"/");
		p=strtok(NULL,"/");
		if(p==NULL) {
			/* Do not allow host values larger than 128 bytes */
			session->req.host[0]=0;
			p=session->req.physical_path+http_scheme_len;
		}
		offset=p-session->req.physical_path;
		memmove(session->req.physical_path
			,session->req.physical_path+offset
			,strlen(session->req.physical_path+offset)+1	/* move '\0' terminator too */
			);
	}
	if(query!=NULL)
		SAFECOPY(session->req.query_str,query);

	return(retval);
}

static char *get_method(http_session_t * session, char *req_line)
{
	int i;

	for(i=0;methods[i]!=NULL;i++) {
		if(!strnicmp(req_line,methods[i],strlen(methods[i]))) {
			session->req.method=i;
			if(strlen(req_line)<strlen(methods[i])+2) {
				send_error(session,"400 Bad Request");
				return(NULL);
			}
			return(req_line+strlen(methods[i])+1);
		}
	}
	if(req_line!=NULL && *req_line>=' ')
		send_error(session,"501 Not Implemented");
	return(NULL);
}

static BOOL get_request_headers(http_session_t * session)
{
	char	head_line[MAX_REQUEST_LINE+1];
	char	next_char;
	char	*value;
	int		i;

	while(sockreadline(session,head_line,sizeof(head_line)-1)>0) {
		/* Multi-line headers */
		while((recvfrom(session->socket,&next_char,1,MSG_PEEK,NULL,0)>0)
			&& (next_char=='\t' || next_char==' ')) {
			i=strlen(head_line);
			if(i>sizeof(head_line)-1) {
				lprintf(LOG_ERR,"%04d !ERROR long multi-line header. The web server is broken!", session->socket);
				i=sizeof(head_line)/2;
				break;
			}
			sockreadline(session,head_line+i,sizeof(head_line)-i-1);
		}
		listPushNodeString(&session->req.headers,head_line);

		if((strtok(head_line,":"))!=NULL && (value=strtok(NULL,""))!=NULL) {
			i=get_header_type(head_line);
			while(*value && *value<=' ') value++;
			switch(i) {
				case HEAD_HOST:
					if(session->req.host[0]==0) {
						SAFECOPY(session->req.host,value);
						SAFECOPY(session->req.vhost,value);
						/* Remove port part of host (Win32 doesn't allow : in dir names) */
						/* Either an existing : will be replaced with a null, or nothing */
						/* Will happen... the return value is not relevent here */
						strtok(session->req.vhost,":");
					}
					break;
				default:
					break;
			}
		}
	}
	return TRUE;
}

static BOOL get_fullpath(http_session_t * session)
{
	char	str[MAX_PATH+1];

	if(!(startup->options&WEB_OPT_VIRTUAL_HOSTS))
		session->req.vhost[0]=0;
	if(session->req.vhost[0]) {
		safe_snprintf(str,sizeof(str),"%s/%s",root_dir,session->req.vhost);
		if(isdir(str))
			safe_snprintf(str,sizeof(str),"%s/%s%s",root_dir,session->req.vhost,session->req.physical_path);
		else
			safe_snprintf(str,sizeof(str),"%s%s",root_dir,session->req.physical_path);
	} else
		safe_snprintf(str,sizeof(str),"%s%s",root_dir,session->req.physical_path);

	if(FULLPATH(session->req.physical_path,str,sizeof(session->req.physical_path))==NULL) {
		send_error(session,error_500);
		return(FALSE);
	}

	return(TRUE);
}

static BOOL get_req(http_session_t * session, char *request_line)
{
	char	req_line[MAX_REQUEST_LINE+1];
	char *	p;
	int		is_redir=0;
	int		len;

	req_line[0]=0;
	if(request_line == NULL) {
		/* Eat leaing blank lines... as apache does...
		 * "This is a legacy issue. The CERN webserver required POST data to have an extra
		 * CRLF following it. Thus many clients send an extra CRLF that is not included in the
		 * Content-Length of the request. Apache works around this problem by eating any empty
		 * lines which appear before a request."
		 * http://httpd.apache.org/docs/misc/known_client_problems.html
		 */
		while((len=sockreadline(session,req_line,sizeof(req_line)-1))==0);
		if(len<0)
			return(FALSE);
		if(req_line[0])
			lprintf(LOG_DEBUG,"%04d Request: %s",session->socket,req_line);
	}
	else {
		lprintf(LOG_DEBUG,"%04d Handling Internal Redirect to: %s",session->socket,request_line);
		SAFECOPY(req_line,request_line);
		is_redir=1;
	}
	if(session->req.ld!=NULL)
		session->req.ld->request=strdup(req_line);
	if(req_line[0]) {
		p=NULL;
		p=get_method(session,req_line);
		if(p!=NULL) {
			p=get_request(session,p);
			session->http_ver=get_version(p);
			if(session->http_ver>=HTTP_1_1)
				session->req.keep_alive=TRUE;
			if(!is_redir)
				get_request_headers(session);
			if(!get_fullpath(session)) {
				send_error(session,error_500);
				return(FALSE);
			}
			if(session->req.ld!=NULL)
				session->req.ld->vhost=strdup(session->req.vhost);
			session->req.dynamic=is_dynamic_req(session);
			if(session->req.query_str[0])  {
				switch(session->req.dynamic) {
					case IS_CGI:
						add_env(session,"QUERY_STRING",session->req.query_str);
						break;
					case IS_JS:
					case IS_SSJS:
						js_add_request_prop(session,"query_string",session->req.query_str);
						js_parse_query(session,session->req.query_str);
						break;
				}
			}

			if(session->req.dynamic==IS_CGI)  {
				add_env(session,"REQUEST_METHOD",methods[session->req.method]);
				add_env(session,"SERVER_PROTOCOL",session->http_ver ? 
					http_vers[session->http_ver] : "HTTP/0.9");
			}
			return(TRUE);
		}
	}
	session->req.keep_alive=FALSE;
	send_error(session,"400 Bad Request");
	return FALSE;
}

/* This may exist somewhere else - ToDo */
static char *find_last_slash(char *str)
{
#ifdef _WIN32
	char * LastFSlash;
	char * LastBSlash;

	LastFSlash=strrchr(str,'/');
	LastBSlash=strrchr(str,'\\');
	if(LastFSlash==NULL)
		return(LastBSlash);
	if(LastBSlash==NULL)
		return(LastFSlash);
	if(LastBSlash < LastFSlash)
		return(LastFSlash);
	return(LastBSlash);
#else
	return(strrchr(str,'/'));
#endif
}

/* This may exist somewhere else - ToDo */
static char *find_first_slash(char *str)
{
#ifdef _WIN32
	char * FirstFSlash;
	char * FirstBSlash;

	FirstFSlash=strchr(str,'/');
	FirstBSlash=strchr(str,'\\');
	if(FirstFSlash==NULL)
		return(FirstBSlash);
	if(FirstBSlash==NULL)
		return(FirstFSlash);
	if(FirstBSlash > FirstFSlash)
		return(FirstFSlash);
	return(FirstBSlash);
#else
	return(strchr(str,'/'));
#endif
}

static BOOL check_extra_path(http_session_t * session)
{
	char	*p;
	char	rpath[MAX_PATH+1];
	char	vpath[MAX_PATH+1];
	char	epath[MAX_PATH+1];
	char	str[MAX_PATH+1];
	struct	stat sb;

	epath[0]=0;
	if(IS_PATH_DELIM(*lastchar(session->req.physical_path)) || stat(session->req.physical_path,&sb)==-1 /* && errno==ENOTDIR */)
	{
		SAFECOPY(vpath,session->req.virtual_path);
		SAFECOPY(rpath,session->req.physical_path);
		while((p=find_last_slash(vpath))!=NULL)
		{
			*p=0;
			if(p==vpath)
				return(FALSE);
			if((p=find_last_slash(rpath))==NULL)
				return(FALSE);
			*p=0;
			SAFECOPY(str,epath);
			sprintf(epath,"/%s%s",(p+1),str);
			if(stat(rpath,&sb)!=-1 && (!(sb.st_mode&S_IFDIR)))
			{
				SAFECOPY(session->req.extra_path_info,epath);
				SAFECOPY(session->req.virtual_path,vpath);
				SAFECOPY(session->req.physical_path,rpath);
				session->req.dynamic=IS_CGI;
				return(TRUE);
			}
		}
	}
	return(FALSE);
}

static BOOL check_request(http_session_t * session)
{
	char	path[MAX_PATH+1];
	char	str[MAX_PATH+1];
	char	last_ch;
	char*	last_slash;
	char*	p;
	FILE*	file;
	int		i;
	struct stat sb;
	int		send404=0;

	SAFECOPY(path,session->req.physical_path);
	if(startup->options&WEB_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG,"%04d Path is: %s",session->socket,path);

	if(isdir(path)) {
		last_ch=*lastchar(path);
		if(!IS_PATH_DELIM(last_ch))  {
			session->req.send_location=MOVED_PERM;
			strcat(path,"/");
		}
		last_ch=*lastchar(session->req.virtual_path);
		if(!IS_PATH_DELIM(last_ch))  {
			session->req.send_location=MOVED_PERM;
			strcat(session->req.virtual_path,"/");
		}
		last_slash=find_last_slash(path);
		if(last_slash==NULL) {
			send_error(session,error_500);
			return(FALSE);
		}
		last_slash++;
		for(i=0; startup->index_file_name!=NULL && startup->index_file_name[i]!=NULL ;i++)  {
			*last_slash=0;
			strcat(path,startup->index_file_name[i]);
			if(startup->options&WEB_OPT_DEBUG_TX)
				lprintf(LOG_DEBUG,"%04d Checking for %s",session->socket,path);
			if(!stat(path,&sb))
				break;
		}

		/* Don't send 404 unless authourized... prevent info leak */
		if(startup->index_file_name[i] == NULL)
			send404=1;
		else {
			strcat(session->req.virtual_path,startup->index_file_name[i]);
			if(session->req.send_location != MOVED_PERM)
				session->req.send_location=MOVED_STAT;
		}
	}
	if(strnicmp(path,root_dir,strlen(root_dir))) {
		session->req.keep_alive=FALSE;
		send_error(session,"400 Bad Request");
		lprintf(LOG_NOTICE,"%04d !ERROR Request for %s is outside of web root %s"
			,session->socket,path,root_dir);
		return(FALSE);
	}

	/* Set default ARS to a 0-length string */
	session->req.ars[0]=0;
	/* Walk up from root_dir checking for access.ars */
	SAFECOPY(str,path);
	last_slash=str+strlen(root_dir)-1;
	/* Loop while there's more /s in path*/
	p=last_slash;

	while((last_slash=find_first_slash(p+1))!=NULL) {
		p=last_slash;
		/* Terminate the path after the slash */
		*(last_slash+1)=0;
		strcat(str,"access.ars");
		if(!stat(str,&sb)) {
			if(!strcmp(path,str)) {
				send_error(session,"403 Forbidden");
				return(FALSE);
			}
			/* Read access.ars file */
			if((file=fopen(str,"r"))!=NULL) {
				fgets(session->req.ars,sizeof(session->req.ars),file);
				fclose(file);
			}
			else  {
				/* If cannot open access.ars, only allow sysop access */
				SAFECOPY(session->req.ars,"LEVEL 90");
				break;
			}
			/* Truncate at \r or \n - can use last_slash since I'm done with it.*/
			truncsp(session->req.ars);
		}
		SAFECOPY(str,path);
	}

	if(!check_ars(session)) {
		/* No authentication provided */
		sprintf(str,"401 Unauthorized%s%s: Basic realm=\"%s\""
			,newline,get_header(HEAD_WWWAUTH),scfg.sys_name);
		send_error(session,str);
		return(FALSE);
	}

	if(stat(path,&sb) || IS_PATH_DELIM(*(lastchar(path))) || send404) {
		if(startup->options&WEB_OPT_DEBUG_TX)
			lprintf(LOG_DEBUG,"%04d 404 - %s does not exist",session->socket,path);
		send_error(session,error_404);
		return(FALSE);
	}
	SAFECOPY(session->req.physical_path,path);
	if(session->req.dynamic==IS_CGI)  {
		add_env(session,"SCRIPT_NAME",session->req.virtual_path);
	}
	SAFECOPY(str,session->req.virtual_path);
	last_slash=find_last_slash(str);
	if(last_slash!=NULL)
		*(last_slash+1)=0;
	if(session->req.dynamic==IS_CGI && *(session->req.extra_path_info))
	{
		sprintf(str,"%s%s",startup->root_dir,session->req.extra_path_info);
		add_env(session,"PATH_TRANSLATED",str);
		add_env(session,"PATH_INFO",session->req.extra_path_info);
	}

	return(TRUE);
}

static BOOL exec_cgi(http_session_t *session)
{
#ifdef __unix__
	char	cmdline[MAX_PATH+256];
	/* ToDo: Damn, that's WAY too many variables */
	int		i=0;
	int		status=0;
	pid_t	child=0;
	int		out_pipe[2];
	int		err_pipe[2];
	struct timeval tv={0,0};
	fd_set	read_set;
	fd_set	write_set;
	int		high_fd=0;
	char	buf[1024];
	BOOL	done_parsing_headers=FALSE;
	BOOL	done_reading=FALSE;
	char	cgi_status[MAX_REQUEST_LINE+1];
	char	header[MAX_REQUEST_LINE+1];
	char	*directive=NULL;
	char	*value=NULL;
	BOOL	done_wait=FALSE;
	BOOL	got_valid_headers=FALSE;
	time_t	start;
	char	cgipath[MAX_PATH+1];
	char	*p;
	char	ch;
	list_node_t	*node;
	BOOL	orig_keep=FALSE;

	SAFECOPY(cmdline,session->req.physical_path);

	lprintf(LOG_INFO,"%04d Executing CGI: %s",session->socket,cmdline);

	orig_keep=session->req.keep_alive;
	session->req.keep_alive=FALSE;

	/* Set up I/O pipes */

	if(pipe(out_pipe)!=0) {
		lprintf(LOG_ERR,"%04d Can't create out_pipe",session->socket,buf);
		return(FALSE);
	}

	if(pipe(err_pipe)!=0) {
		lprintf(LOG_ERR,"%04d Can't create err_pipe",session->socket,buf);
		return(FALSE);
	}

	if((child=fork())==0)  {
		/* Do a full suid thing. */
		if(startup->setuid!=NULL)
			startup->setuid(TRUE);

		/* Set up environment */
		for(node=listFirstNode(&session->req.cgi_env);node!=NULL;node=listNextNode(node))
			putenv(listNodeData(node));

		/* Set up STDIO */
		dup2(session->socket,0);		/* redirect stdin */
		close(out_pipe[0]);		/* close read-end of pipe */
		dup2(out_pipe[1],1);	/* stdout */
		close(out_pipe[1]);		/* close excess file descriptor */
		close(err_pipe[0]);		/* close read-end of pipe */
		dup2(err_pipe[1],2);	/* stderr */
		close(err_pipe[1]);		/* close excess file descriptor */

		SAFECOPY(cgipath,cmdline);
		if((p=strrchr(cgipath,'/'))!=NULL)
		{
			*p=0;
			chdir(cgipath);
		}

		/* Execute command */
		execl(cmdline,cmdline,NULL);
		lprintf(LOG_ERR,"%04d !FAILED! execl()",session->socket);
		exit(EXIT_FAILURE); /* Should never happen */
	}

	if(child==-1)  {
		lprintf(LOG_ERR,"%04d !FAILED! fork() errno=%d",session->socket,errno);
		close(out_pipe[0]);		/* close read-end of pipe */
		close(err_pipe[0]);		/* close read-end of pipe */
	}

	close(out_pipe[1]);		/* close excess file descriptor */
	close(err_pipe[1]);		/* close excess file descriptor */

	if(child==-1)
		return(FALSE);

	start=time(NULL);

	high_fd=out_pipe[0];
	if(err_pipe[0]>high_fd)
		high_fd=err_pipe[0];

	/* ToDo: Magically set done_parsing_headers for nph-* scripts */
	cgi_status[0]=0;
	while(!done_reading)  {
		tv.tv_sec=startup->max_cgi_inactivity;
		tv.tv_usec=0;

		FD_ZERO(&read_set);
		FD_SET(out_pipe[0],&read_set);
		FD_SET(err_pipe[0],&read_set);
		FD_ZERO(&write_set);

		if(select(high_fd+1,&read_set,&write_set,NULL,&tv)>0)  {
			if(FD_ISSET(out_pipe[0],&read_set))  {
				if(done_parsing_headers && got_valid_headers)  {
					i=read(out_pipe[0],buf,sizeof(buf));
					if(i>0)  {
						int snt=0;
						start=time(NULL);
						if(session->req.method!=HTTP_HEAD) {
							snt=write(session->socket,buf,i);
							if(session->req.ld!=NULL && snt>0) {
								session->req.ld->size+=snt;
							}
						}
					}
					else
						done_reading=TRUE;
				}
				else  {
					/* This is the tricky part */
					i=pipereadline(out_pipe[0],buf,sizeof(buf));
					if(i<0)  {
						done_reading=TRUE;
						got_valid_headers=FALSE;
					}
					else
						start=time(NULL);

					if(!done_parsing_headers && *buf)  {
						SAFECOPY(header,buf);
						directive=strtok(header,":");
						if(directive != NULL)  {
							value=strtok(NULL,"");
							i=get_header_type(directive);
							switch (i)  {
								case HEAD_LOCATION:
									got_valid_headers=TRUE;
									if(*value=='/')  {
										unescape(value);
										SAFECOPY(session->req.virtual_path,value);
										session->req.send_location=MOVED_STAT;
										if(cgi_status[0]==0)
											SAFECOPY(cgi_status,error_302);
									} else  {
										SAFECOPY(session->req.virtual_path,value);
										session->req.send_location=MOVED_STAT;
										if(cgi_status[0]==0)
											SAFECOPY(cgi_status,error_302);
									}
									break;
								case HEAD_STATUS:
									SAFECOPY(cgi_status,value);
									break;
								case HEAD_LENGTH:
									session->req.keep_alive=orig_keep;
									listPushNodeString(&session->req.dynamic_heads,buf);
									break;
								case HEAD_TYPE:
									got_valid_headers=TRUE;
									listPushNodeString(&session->req.dynamic_heads,buf);
									break;
								default:
									listPushNodeString(&session->req.dynamic_heads,buf);
							}
						}
					}
					else  {
						if(got_valid_headers)  {
							session->req.dynamic=IS_CGI;
							if(cgi_status[0]==0)
								SAFECOPY(cgi_status,session->req.status);
							send_headers(session,cgi_status);
						}
						done_parsing_headers=TRUE;
					}
				}
			}
			if(FD_ISSET(err_pipe[0],&read_set))  {
				i=read(err_pipe[0],buf,sizeof(buf));
				buf[i]=0;
				if(i>0)
					start=time(NULL);
			}
			if(!done_wait)
				done_wait = (waitpid(child,&status,WNOHANG)==child);
			if(!FD_ISSET(err_pipe[0],&read_set) && !FD_ISSET(out_pipe[0],&read_set) && done_wait)
				done_reading=TRUE;
		}
		else  {
			if((time(NULL)-start) >= startup->max_cgi_inactivity)  {
				lprintf(LOG_ERR,"%04d CGI Process %s Timed out",session->socket,cmdline);
				done_reading=TRUE;
				start=0;
			}
		}
	}

	/* Drain STDERR */	
	tv.tv_sec=1;
	tv.tv_usec=0;
	FD_ZERO(&read_set);
	FD_SET(err_pipe[0],&read_set);
	if(select(high_fd+1,&read_set,&write_set,NULL,&tv)>0)
	if(FD_ISSET(err_pipe[0],&read_set)) {
		while(pipereadline(err_pipe[0],buf,sizeof(buf))!=-1)
			lprintf(LOG_ERR,"%s",buf);
	}

	if(!done_wait)
		done_wait = (waitpid(child,&status,WNOHANG)==child);
	if(!done_wait)  {
		if(start)
			lprintf(LOG_NOTICE,"%04d CGI Process %s still alive on client exit"
				,session->socket,cmdline);
		kill(child,SIGTERM);
		mswait(1000);
		done_wait = (waitpid(child,&status,WNOHANG)==child);
		if(!done_wait)  {
			kill(child,SIGKILL);
			done_wait = (waitpid(child,&status,0)==child);
		}
	}

	close(out_pipe[0]);		/* close read-end of pipe */
	close(err_pipe[0]);		/* close read-end of pipe */
	if(!got_valid_headers) {
		lprintf(LOG_ERR,"%04d CGI Process %s did not generate valid headers",session->socket,cmdline);
		return(FALSE);
	}

	if(!done_parsing_headers) {
		lprintf(LOG_ERR,"%04d CGI Process %s did not send data header termination",session->socket,cmdline);
		return(FALSE);
	}

	return(TRUE);
#else
	/* Win32 exec_cgi() */

	/* These are (more or less) copied from the Unix version */
	char*	p;
	char	cmdline[MAX_PATH+256];
	char	buf[4096];
	int		i;
	BOOL	orig_keep;
	BOOL	done_parsing_headers=FALSE;
	BOOL	got_valid_headers=FALSE;
	char	cgi_status[MAX_REQUEST_LINE+1];
	char	header[MAX_REQUEST_LINE+1];
	char	*directive=NULL;
	char	*value=NULL;
	time_t	start;

	/* Win32-specific */
	char	startup_dir[MAX_PATH+1];
	int		wr;
	HANDLE	rdpipe=INVALID_HANDLE_VALUE;
	HANDLE	wrpipe=INVALID_HANDLE_VALUE;
	HANDLE	rdoutpipe;
	HANDLE	wrinpipe;
	DWORD	waiting;
	DWORD	msglen;
	DWORD	retval;
    PROCESS_INFORMATION process_info;
	SECURITY_ATTRIBUTES sa;
    STARTUPINFO startup_info={0};

    startup_info.cb=sizeof(startup_info);
	startup_info.dwFlags|=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    startup_info.wShowWindow=SW_HIDE;

	SAFECOPY(cmdline,session->req.physical_path);

	lprintf(LOG_INFO,"%04d Executing CGI: %s",session->socket,cmdline);

	orig_keep=session->req.keep_alive;
	session->req.keep_alive=FALSE;

	memset(&sa,0,sizeof(sa));
	sa.nLength= sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	// Create the child output pipe (override default 4K buffer size)
	if(!CreatePipe(&rdoutpipe,&startup_info.hStdOutput,&sa,sizeof(buf))) {
		lprintf(LOG_ERR,"%04d !ERROR %d creating stdout pipe",session->socket,GetLastError());
		return(FALSE);
	}
	startup_info.hStdError=startup_info.hStdOutput;

	// Create the child input pipe.
	if(!CreatePipe(&startup_info.hStdInput,&wrinpipe,&sa,0 /* default buffer size */)) {
		lprintf(LOG_ERR,"%04d !ERROR %d creating stdin pipe",session->socket,GetLastError());
		return(FALSE);
	}

	DuplicateHandle(
		GetCurrentProcess(), rdoutpipe,
		GetCurrentProcess(), &rdpipe, 0, FALSE, DUPLICATE_SAME_ACCESS);

	DuplicateHandle(
		GetCurrentProcess(), wrinpipe,
		GetCurrentProcess(), &wrpipe, 0, FALSE, DUPLICATE_SAME_ACCESS);

	CloseHandle(rdoutpipe);
	CloseHandle(wrinpipe);

	SAFECOPY(startup_dir,cmdline);
	if((p=strrchr(startup_dir,'/'))!=NULL || (p=strrchr(startup_dir,'\\'))!=NULL)
		*p=0;
	else
		SAFECOPY(startup_dir,cgi_dir);

	lprintf(LOG_DEBUG,"%04d CGI startup dir: %s", session->socket, startup_dir);

    if(!CreateProcess(
		NULL,			// pointer to name of executable module
		cmdline,  		// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		TRUE,	 		// handle inheritance flag
		CREATE_NEW_CONSOLE, // creation flags
        NULL,  			// pointer to new environment block
		startup_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		)) {
		lprintf(LOG_ERR,"%04d !ERROR %d running %s",session->socket,GetLastError(),cmdline);
		return(FALSE);
    }

	start=time(NULL);

	cgi_status[0]=0;
	while(server_socket!=INVALID_SOCKET) {

		if((time(NULL)-start) >= startup->max_cgi_inactivity)  {
			lprintf(LOG_WARNING,"%04d CGI Process %s Timed out",session->socket,cmdline);
			break;
		}

		waiting = 0;
		PeekNamedPipe(
			rdpipe,             // handle to pipe to copy from
			NULL,               // pointer to data buffer
			0,					// size, in bytes, of data buffer
			NULL,				// pointer to number of bytes read
			&waiting,			// pointer to total number of bytes available
			NULL				// pointer to unread bytes in this message
			);
		if(waiting) {
			if(done_parsing_headers && got_valid_headers)  {
				msglen=0;
				if(ReadFile(rdpipe,buf,sizeof(buf),&msglen,NULL)==FALSE) {
					lprintf(LOG_ERR,"%04d !ERROR %d reading from pipe"
						,session->socket,GetLastError());
					break;
				}
				if(msglen) {
					lprintf(LOG_DEBUG,"%04d Sending %d bytes"
						,session->socket,msglen);
					wr=sendsocket(session->socket,buf,msglen);
					/* log actual bytes sent */
					if(session->req.ld!=NULL && wr>0)
						session->req.ld->size+=wr;	
					/* reset inactivity timer */
					start=time(NULL);	
					continue;
				}
			} else  {
				/* This is the tricky part */
				i=pipereadline(rdpipe,buf,sizeof(buf));
				if(i<0)  {
					got_valid_headers=FALSE;
					break;
				}
				start=time(NULL);

				if(!done_parsing_headers && *buf)  {
					SAFECOPY(header,buf);
					directive=strtok(header,":");
					if(directive != NULL)  {
						value=strtok(NULL,"");
						i=get_header_type(directive);
						switch (i)  {
							case HEAD_LOCATION:
								got_valid_headers=TRUE;
								if(*value=='/')  {
									unescape(value);
									SAFECOPY(session->req.virtual_path,value);
									session->req.send_location=MOVED_STAT;
									if(cgi_status[0]==0)
										SAFECOPY(cgi_status,error_302);
								} else  {
									SAFECOPY(session->req.virtual_path,value);
									session->req.send_location=MOVED_STAT;
									if(cgi_status[0]==0)
										SAFECOPY(cgi_status,error_302);
								}
								break;
							case HEAD_STATUS:
								SAFECOPY(cgi_status,value);
								break;
							case HEAD_LENGTH:
								session->req.keep_alive=orig_keep;
								listPushNodeString(&session->req.dynamic_heads,buf);
								break;
							case HEAD_TYPE:
								got_valid_headers=TRUE;
								listPushNodeString(&session->req.dynamic_heads,buf);
								break;
							default:
								listPushNodeString(&session->req.dynamic_heads,buf);
						}
					}
				}
				else  {
					if(got_valid_headers)  {
						session->req.dynamic=IS_CGI;
						if(cgi_status[0]==0)
							SAFECOPY(cgi_status,session->req.status);
						send_headers(session,cgi_status);
					}
					done_parsing_headers=TRUE;
				}
				continue;
			}
		}
		Sleep(1);

		if(WaitForSingleObject(process_info.hProcess,0)==WAIT_OBJECT_0)
			break;
	}

    if(GetExitCodeProcess(process_info.hProcess, &retval)==FALSE)
	    lprintf(LOG_ERR,"%04d !ERROR GetExitCodeProcess(%s) returned %d"
			,session->socket,cmdline,GetLastError());

	if(retval==STILL_ACTIVE) {
		lprintf(LOG_WARNING,"%04d Terminating CGI process: %s",session->socket,cmdline);
		TerminateProcess(process_info.hProcess, GetLastError());
	}	

	if(rdpipe!=INVALID_HANDLE_VALUE)
		CloseHandle(rdpipe);
	if(wrpipe!=INVALID_HANDLE_VALUE)
		CloseHandle(wrpipe);
	CloseHandle(process_info.hProcess);

	if(!got_valid_headers) {
		lprintf(LOG_ERR,"%04d CGI Process %s did not generate valid headers",session->socket,cmdline);
		return(FALSE);
	}

	if(!done_parsing_headers) {
		lprintf(LOG_ERR,"%04d CGI Process %s did not send data header termination",session->socket,cmdline);
		return(FALSE);
	}

	return(TRUE);
#endif
}

/********************/
/* JavaScript stuff */
/********************/

JSObject* DLLCALL js_CreateHttpReplyObject(JSContext* cx
										   ,JSObject* parent, http_session_t *session)
{
	JSObject*	reply;
	JSObject*	headers;
	jsval		val;
	JSString*	js_str;
	
	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"http_reply",&val) && val!=JSVAL_VOID)  {
		reply = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,reply);
	}
	else
		reply = JS_DefineObject(cx, parent, "http_reply", NULL
									, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, session->req.status))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, reply, "status", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,reply,"header",&val) && val!=JSVAL_VOID)  {
		headers = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,headers);
	}
	else
		headers = JS_DefineObject(cx, reply, "header", NULL
									, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, "text/html"))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, headers, "Content-Type", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE);

	return(reply);
}

JSObject* DLLCALL js_CreateHttpRequestObject(JSContext* cx
											 ,JSObject* parent, http_session_t *session)
{
/*	JSObject*	cookie; */
	jsval		val;

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"http_request",&val) && val!=JSVAL_VOID)  {
		session->js_request=JSVAL_TO_OBJECT(val);
	}
	else
		session->js_request = JS_DefineObject(cx, parent, "http_request", NULL
									, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	js_add_request_prop(session,"path_info",session->req.extra_path_info);
	js_add_request_prop(session,"method",methods[session->req.method]);
	js_add_request_prop(session,"virtual_path",session->req.virtual_path);

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,session->js_request,"query",&val) && val!=JSVAL_VOID)  {
		session->js_query = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,session->js_query);
	}
	else
		session->js_query = JS_DefineObject(cx, session->js_request, "query", NULL
									, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	
	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,session->js_request,"header",&val) && val!=JSVAL_VOID)  {
		session->js_header = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx,session->js_header);
	}
	else
		session->js_header = JS_DefineObject(cx, session->js_request, "header", NULL
									, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	return(session->js_request);
}

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	char*	warning;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return;
	
	if(report==NULL) {
		lprintf(LOG_ERR,"%04d !JavaScript: %s", session->socket, message);
		if(session->req.fp!=NULL)
			fprintf(session->req.fp,"!JavaScript: %s", message);
		return;
    }

	if(report->filename)
		sprintf(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		sprintf(line," line %u",report->lineno);
	else
		line[0]=0;

	if(JSREPORT_IS_WARNING(report->flags)) {
		if(JSREPORT_IS_STRICT(report->flags))
			warning="strict warning";
		else
			warning="warning";
	} else
		warning="";

	lprintf(LOG_ERR,"%04d !JavaScript %s%s%s: %s",session->socket,warning,file,line,message);
	if(session->req.fp!=NULL)
		fprintf(session->req.fp,"!JavaScript %s%s%s: %s",warning,file,line,message);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString*	str=NULL;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(session->req.fp==NULL)
		return(JS_FALSE);

	if((!session->req.prev_write) && (!session->req.sent_headers)) {
		/* "Fast Mode" requested? */
		jsval		val;
		JSObject*	reply;

		JS_GetProperty(cx, session->js_glob, "http_reply", &val);
		reply=JSVAL_TO_OBJECT(val);
		JS_GetProperty(cx, reply, "fast", &val);
		if(JSVAL_IS_BOOLEAN(val) && JSVAL_TO_BOOLEAN(val)) {
			session->req.keep_alive=FALSE;
			if(!ssjs_send_headers(session))
				return(JS_FALSE);
		}
	}

	session->req.prev_write=TRUE;

    for(i=0; i<argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
			continue;
		if(session->req.sent_headers) {
			if(session->req.method!=HTTP_HEAD)
				sendsocket(session->socket, JS_GetStringBytes(str), JS_GetStringLength(str));
		}
		else
			fwrite(JS_GetStringBytes(str),1,JS_GetStringLength(str),session->req.fp);
	}

	if(str==NULL)
		*rval = JSVAL_VOID;
	else
		*rval = STRING_TO_JSVAL(str);

	return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	js_write(cx, obj, argc, argv, rval);

	/* Should this do the whole \r\n thing for Win32 *shudder* */
	if(session->req.sent_headers) {
		if(session->req.method!=HTTP_HEAD)
			sendsocket(session->socket, "\n", 1);
	}
	else
		fprintf(session->req.fp,"\n");

	return(JS_TRUE);
}

static JSBool
js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		str[512];
    uintN		i=0;
	int32		level=LOG_INFO;
    JSString*	js_str;
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    if(startup==NULL || startup->lputs==NULL)
        return(JS_FALSE);

	if(JSVAL_IS_NUMBER(argv[i]))
		JS_ValueToInt32(cx,argv[i++],&level);

	str[0]=0;
    for(;i<argc && strlen(str)<(sizeof(str)/2);i++) {
		if((js_str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		strncat(str,JS_GetStringBytes(js_str),sizeof(str)/2);
		strcat(str," ");
	}

	lprintf(level,"%04d %s",session->socket,str);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));

    return(JS_TRUE);
}

static JSBool
js_login(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSBool		inc_logons=JS_FALSE;
	user_t		user;
	JSString*	js_str;
	http_session_t*	session;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	/* User name */
	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((p=JS_GetStringBytes(js_str))==NULL) 
		return(JS_FALSE);

	memset(&user,0,sizeof(user));

	if(isdigit(*p))
		user.number=atoi(p);
	else if(*p)
		user.number=matchuser(&scfg,p,FALSE);

	if(getuserdat(&scfg,&user)!=0) {
		lprintf(LOG_NOTICE,"%04d !USER NOT FOUND: '%s'"
			,session->socket,p);
		return(JS_TRUE);
	}

	if(user.misc&(DELETED|INACTIVE)) {
		lprintf(LOG_WARNING,"%04d !DELETED OR INACTIVE USER #%d: %s"
			,session->socket,user.number,p);
		return(JS_TRUE);
	}

	/* Password */
	if(user.pass[0]) {
		if((js_str=JS_ValueToString(cx, argv[1]))==NULL) 
			return(JS_FALSE);

		if((p=JS_GetStringBytes(js_str))==NULL) 
			return(JS_FALSE);

		if(stricmp(user.pass,p)) { /* Wrong password */
			lprintf(LOG_WARNING,"%04d !INVALID PASSWORD ATTEMPT FOR USER: %s"
				,session->socket,user.alias);
			return(JS_TRUE);
		}
	}

	if(argc>2)
		JS_ValueToBoolean(cx,argv[2],&inc_logons);

	if(inc_logons) {
		user.logons++;
		user.ltoday++;
	}

	http_logon(session, &user);

	/* user-specific objects */
	if(!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user
		,NULL /* ftp index file */, session->subscan /* subscan */)) {
		lprintf(LOG_ERR,"%04d !JavaScript ERROR creating user objects",session->socket);
		send_error(session,"500 Error initializing JavaScript User Objects");
		return(FALSE);
	}

	*rval=BOOLEAN_TO_JSVAL(JS_TRUE);

	return(JS_TRUE);
}

static JSFunctionSpec js_global_functions[] = {
	{"write",           js_write,           1},		/* write to HTML file */
	{"writeln",         js_writeln,         1},		/* write line to HTML file */
	{"print",			js_writeln,			1},		/* write line to HTML file (alias) */
	{"log",				js_log,				0},		/* Log a string */
	{"login",           js_login,           2},		/* log in as a different user */
	{0}
};

static JSBool
js_BranchCallback(JSContext *cx, JSScript *script)
{
	http_session_t* session;

	if((session=(http_session_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    return(js_CommonBranchCallback(cx,&session->js_branch));
}

static JSContext* 
js_initcx(http_session_t *session)
{
	JSContext*	js_cx;

	lprintf(LOG_INFO,"%04d JavaScript: Initializing context (stack: %lu bytes)"
		,session->socket,startup->js_cx_stack);

    if((js_cx = JS_NewContext(session->js_runtime, startup->js_cx_stack))==NULL)
		return(NULL);

	lprintf(LOG_INFO,"%04d JavaScript: Context created",session->socket);

    JS_SetErrorReporter(js_cx, js_ErrorReporter);

	JS_SetBranchCallback(js_cx, js_BranchCallback);

	lprintf(LOG_INFO,"%04d JavaScript: Creating Global Objects and Classes",session->socket);
	if((session->js_glob=js_CreateCommonObjects(js_cx, &scfg, NULL
									,NULL						/* global */
									,uptime						/* system */
									,startup->host_name			/* system */
									,SOCKLIB_DESC				/* system */
									,&session->js_branch		/* js */
									,&session->client			/* client */
									,session->socket			/* client */
									,&js_server_props			/* server */
		))==NULL
		|| !JS_DefineFunctions(js_cx, session->js_glob, js_global_functions)) {
		JS_DestroyContext(js_cx);
		return(NULL);
	}

	return(js_cx);
}

static BOOL js_setup(http_session_t* session)
{
	JSObject*	argv;

	if(session->js_runtime == NULL) {
		lprintf(LOG_INFO,"%04d JavaScript: Creating runtime: %lu bytes"
			,session->socket,startup->js_max_bytes);

		if((session->js_runtime=JS_NewRuntime(startup->js_max_bytes))==NULL) {
			lprintf(LOG_ERR,"%04d !ERROR creating JavaScript runtime",session->socket);
			return(FALSE);
		}
	}

	if(session->js_cx==NULL) {	/* Context not yet created, create it now */
		if(((session->js_cx=js_initcx(session))==NULL)) {
			lprintf(LOG_ERR,"%04d !ERROR initializing JavaScript context",session->socket);
			return(FALSE);
		}
		argv=JS_NewArrayObject(session->js_cx, 0, NULL);

		JS_DefineProperty(session->js_cx, session->js_glob, "argv", OBJECT_TO_JSVAL(argv)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
		JS_DefineProperty(session->js_cx, session->js_glob, "argc", INT_TO_JSVAL(0)
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

		JS_DefineProperty(session->js_cx, session->js_glob, "web_root_dir",
			STRING_TO_JSVAL(JS_NewStringCopyZ(session->js_cx, root_dir))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
		JS_DefineProperty(session->js_cx, session->js_glob, "web_error_dir",
			STRING_TO_JSVAL(JS_NewStringCopyZ(session->js_cx, error_dir))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	}

	lprintf(LOG_INFO,"%04d JavaScript: Initializing HttpRequest object",session->socket);
	if(js_CreateHttpRequestObject(session->js_cx, session->js_glob, session)==NULL) {
		lprintf(LOG_ERR,"%04d !ERROR initializing JavaScript HttpRequest object",session->socket);
		return(FALSE);
	}

	lprintf(LOG_INFO,"%04d JavaScript: Initializing HttpReply object",session->socket);
	if(js_CreateHttpReplyObject(session->js_cx, session->js_glob, session)==NULL) {
		lprintf(LOG_ERR,"%04d !ERROR initializing JavaScript HttpReply object",session->socket);
		return(FALSE);
	}

	JS_SetContextPrivate(session->js_cx, session);

	return(TRUE);
}

static BOOL ssjs_send_headers(http_session_t* session)
{
	jsval		val;
	JSObject*	reply;
	JSIdArray*	heads;
	JSObject*	headers;
	int			i;
	JSString*	js_str;
	char		str[MAX_REQUEST_LINE+1];

	JS_GetProperty(session->js_cx,session->js_glob,"http_reply",&val);
	reply = JSVAL_TO_OBJECT(val);
	JS_GetProperty(session->js_cx,reply,"status",&val);
	SAFECOPY(session->req.status,JS_GetStringBytes(JSVAL_TO_STRING(val)));
	JS_GetProperty(session->js_cx,reply,"header",&val);
	headers = JSVAL_TO_OBJECT(val);
	heads=JS_Enumerate(session->js_cx,headers);
	for(i=0;i<heads->length;i++)  {
		JS_IdToValue(session->js_cx,heads->vector[i],&val);
		js_str=JSVAL_TO_STRING(val);
		JS_GetProperty(session->js_cx,headers,JS_GetStringBytes(js_str),&val);
		safe_snprintf(str,sizeof(str),"%s: %s"
			,JS_GetStringBytes(js_str),JS_GetStringBytes(JSVAL_TO_STRING(val)));
		listPushNodeString(&session->req.dynamic_heads,str);
	}
	JS_DestroyIdArray(session->js_cx, heads);
	session->req.sent_headers=TRUE;
	return(send_headers(session,session->req.status));
}

static BOOL exec_ssjs(http_session_t* session, char *script)  {
	JSScript*	js_script;
	jsval		rval;
	char		path[MAX_PATH+1];
	BOOL		retval=TRUE;

	sprintf(path,"%sSBBS_SSJS.%u.%u.html",temp_dir,getpid(),session->socket);
	if((session->req.fp=fopen(path,"wb"))==NULL) {
		lprintf(LOG_ERR,"%04d !ERROR %d opening/creating %s", session->socket, errno, path);
		return(FALSE);
	}
	session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE]=strdup(path);

	js_add_request_prop(session,"real_path",session->req.physical_path);
	js_add_request_prop(session,"virtual_path",session->req.virtual_path);
	js_add_request_prop(session,"ars",session->req.ars);
	js_add_request_prop(session,"request_string",session->req.request_line);
	js_add_request_prop(session,"host",session->req.host);
	js_add_request_prop(session,"http_ver",http_vers[session->http_ver]);
	js_add_request_prop(session,"remote_ip",session->host_ip);
	js_add_request_prop(session,"remote_host",session->host_name);

	do {
		/* RUN SCRIPT */
		JS_ClearPendingException(session->js_cx);

		session->js_branch.counter=0;

		if((js_script=JS_CompileFile(session->js_cx, session->js_glob
			,script))==NULL) {
			lprintf(LOG_ERR,"%04d !JavaScript FAILED to compile script (%s)"
				,session->socket,script);
			return(FALSE);
		}

		JS_ExecuteScript(session->js_cx, session->js_glob, js_script, &rval);
	} while(0);

	SAFECOPY(session->req.physical_path, path);
	if(session->req.fp!=NULL) {
		fclose(session->req.fp);
		session->req.fp=NULL;
	}


	/* Read http_reply object */
	if(!session->req.sent_headers) {
		retval=ssjs_send_headers(session);
	}

	/* Free up temporary resources here */

	if(js_script!=NULL) 
		JS_DestroyScript(session->js_cx, js_script);
	session->req.dynamic=IS_SSJS;
	
	return(retval);
}

static void respond(http_session_t * session)
{
	BOOL		send_file=TRUE;

	if(session->req.dynamic==IS_CGI)  {
		if(!exec_cgi(session))  {
			send_error(session,error_500);
			return;
		}
		close_request(session);
		return;
	}

	if(session->req.dynamic==IS_SSJS) {	/* Server-Side JavaScript */
		if(!exec_ssjs(session,session->req.physical_path))  {
			send_error(session,error_500);
			return;
		}
		sprintf(session->req.physical_path
			,"%sSBBS_SSJS.%u.%u.html",temp_dir,getpid(),session->socket);
	}
	else {
		session->req.mime_type=get_mime_type(strrchr(session->req.physical_path,'.'));
		send_file=send_headers(session,session->req.status);
	}
	if(session->req.method==HTTP_HEAD)
		send_file=FALSE;
	if(send_file)  {
		int snt=0;
		lprintf(LOG_INFO,"%04d Sending file: %s (%u bytes)"
			,session->socket, session->req.physical_path, flength(session->req.physical_path));
		snt=sock_sendfile(session->socket,session->req.physical_path);
		if(session->req.ld!=NULL) {
			if(snt<0)
				snt=0;
			session->req.ld->size=snt;
		}
		if(snt>0)
			lprintf(LOG_INFO,"%04d Sent file: %s (%d bytes)"
				,session->socket, session->req.physical_path, snt);
	}
	close_request(session);
}

void http_session_thread(void* arg)
{
	int				i;
	char*			host_name;
	HOSTENT*		host;
	SOCKET			socket;
	char			redir_req[MAX_REQUEST_LINE+1];
	char			*redirp;
	http_session_t	session=*(http_session_t*)arg;	/* copies arg BEFORE it's freed */
	int				loop_count;

	FREE_AND_NULL(arg);

	socket=session.socket;
	lprintf(LOG_DEBUG,"%04d Session thread started", session.socket);

#ifdef _WIN32
	if(startup->answer_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
		PlaySound(startup->answer_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	thread_up(TRUE /* setuid */);
	session.finished=FALSE;

	sbbs_srand();	/* Seed random number generator */

	if(startup->options&BBS_OPT_NO_HOST_LOOKUP)
		host=NULL;
	else
		host=gethostbyaddr ((char *)&session.addr.sin_addr
			,sizeof(session.addr.sin_addr),AF_INET);

	if(host!=NULL && host->h_name!=NULL)
		host_name=host->h_name;
	else
		host_name=session.host_ip;

	SAFECOPY(session.host_name,host_name);

	if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP))  {
		lprintf(LOG_INFO,"%04d Hostname: %s", session.socket, host_name);
		for(i=0;host!=NULL && host->h_aliases!=NULL 
			&& host->h_aliases[i]!=NULL;i++)
			lprintf(LOG_INFO,"%04d HostAlias: %s", session.socket, host->h_aliases[i]);
		if(trashcan(&scfg,host_name,"host")) {
			close_socket(session.socket);
			lprintf(LOG_NOTICE,"%04d !CLIENT BLOCKED in host.can: %s", session.socket, host_name);
			thread_down();
			return;
		}
	}

	/* host_ip wasn't defined in http_session_thread */
	if(trashcan(&scfg,session.host_ip,"ip")) {
		close_socket(session.socket);
		lprintf(LOG_NOTICE,"%04d !CLIENT BLOCKED in ip.can: %s", session.socket, session.host_ip);
		thread_down();
		return;
	}

	active_clients++;
	update_clients();
	SAFECOPY(session.username,unknown);

	SAFECOPY(session.client.addr,session.host_ip);
	SAFECOPY(session.client.host,session.host_name);
	session.client.port=ntohs(session.addr.sin_port);
	session.client.time=time(NULL);
	session.client.protocol="HTTP";
	session.client.user=session.username;
	session.client.size=sizeof(session.client);
	client_on(session.socket, &session.client, /* update existing client record? */FALSE);

	session.last_user_num=-1;
	session.last_js_user_num=-1;
	session.logon_time=0;

	session.subscan=(subscan_t*)malloc(sizeof(subscan_t)*scfg.total_subs);

	while(!session.finished && server_socket!=INVALID_SOCKET) {
	    memset(&(session.req), 0, sizeof(session.req));
		redirp=NULL;
		loop_count=0;
		while((redirp==NULL || session.req.send_location >= MOVED_TEMP)
				 && !session.finished && server_socket!=INVALID_SOCKET) {
			SAFECOPY(session.req.status,"200 OK");
			session.req.send_location=NO_LOCATION;
			session.req.ld=NULL;
			if(startup->options&WEB_OPT_HTTP_LOGGING) {
				if((session.req.ld=(struct log_data*)malloc(sizeof(struct log_data)))==NULL)
					lprintf(LOG_ERR,"%04d Cannot allocate memory for log data!",session.socket);
			}
			if(session.req.ld!=NULL) {
				memset(session.req.ld,0,sizeof(struct log_data));
				session.req.ld->hostname=strdup(session.host_name);
			}
			listInit(&session.req.headers,0);
			listInit(&session.req.cgi_env,0);
			listInit(&session.req.dynamic_heads,0);
			if(get_req(&session,redirp)) {
				/* At this point, if redirp is non-NULL then the headers have already been parsed */
				if((session.http_ver<HTTP_1_0)||redirp!=NULL||parse_headers(&session)) {
					if(check_request(&session)) {
						if(session.req.send_location < MOVED_TEMP || session.req.virtual_path[0]!='/' || loop_count++ >= MAX_REDIR_LOOPS) {
							respond(&session);
						}
						else {
							safe_snprintf(redir_req,sizeof(redir_req),"%s %s%s%s",methods[session.req.method]
								,session.req.virtual_path,session.http_ver<HTTP_1_0?"":" ",http_vers[session.http_ver]);
							lprintf(LOG_DEBUG,"%04d Internal Redirect to: %s",socket,redir_req);
							redirp=redir_req;
						}
					}
				}
			}
		}
	}

	http_logoff(&session,socket,__LINE__);

	if(session.js_cx!=NULL) {
		lprintf(LOG_INFO,"%04d JavaScript: Destroying context",socket);
		JS_DestroyContext(session.js_cx);	/* Free Context */
		session.js_cx=NULL;
	}

	if(session.js_runtime!=NULL) {
		lprintf(LOG_INFO,"%04d JavaScript: Destroying runtime",socket);
		JS_DestroyRuntime(session.js_runtime);
		session.js_runtime=NULL;
	}

	FREE_AND_NULL(session.subscan);

#ifdef _WIN32
	if(startup->hangup_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
		PlaySound(startup->hangup_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	close_socket(session.socket);

	active_clients--;
	update_clients();
	client_off(socket);

	thread_down();
	lprintf(LOG_INFO,"%04d Session thread terminated (%u clients, %u threads remain, %lu served)"
		,socket, active_clients, thread_count, served);

}

void DLLCALL web_terminate(void)
{
   	lprintf(LOG_INFO,"%04d Web Server terminate",server_socket);
	terminate_server=TRUE;
}

static void cleanup(int code)
{
	free_cfg(&scfg);

	listFree(&log_list);

	mime_types=iniFreeNamedStringList(mime_types);

	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	if(server_socket!=INVALID_SOCKET) {
		close_socket(server_socket);
		server_socket=INVALID_SOCKET;
	}

	update_clients();

#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf(LOG_ERR,"0000 !WSACleanup ERROR %d",ERROR_VALUE);
#endif

	thread_down();
	status("Down");
	if(terminate_server || code)
		lprintf(LOG_INFO,"#### Web Server thread terminated (%u threads remain, %lu clients served)"
			,thread_count, served);
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(startup->cbdata,code);
}

const char* DLLCALL web_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sscanf("$Revision$", "%*s %s", revision);

	sprintf(ver,"%s %s%s  "
		"Compiled %s %s with %s"
		,server_name
		,revision
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,__DATE__, __TIME__, compiler);

	return(ver);
}

void http_logging_thread(void* arg)
{
	char	base[MAX_PATH+1];
	char	filename[MAX_PATH+1];
	char	newfilename[MAX_PATH+1];
	FILE*	logfile=NULL;

	http_logging_thread_running=TRUE;
	terminate_http_logging_thread=FALSE;

	SAFECOPY(base,arg);
	if(!base[0])
		SAFEPRINTF(base,"%slogs/http-",scfg.logs_dir);

	filename[0]=0;
	newfilename[0]=0;

	thread_up(TRUE /* setuid */);

	lprintf(LOG_DEBUG,"%04d http logging thread started", server_socket);

	for(;!terminate_http_logging_thread;) {
		struct log_data *ld;
		char	timestr[128];
		char	sizestr[100];

		sem_wait(&log_sem);
		if(terminate_http_logging_thread)
			break;

		ld=listShiftNode(&log_list);
		if(ld==NULL) {
			lprintf(LOG_ERR,"%04d http logging thread received NULL linked list log entry"
				,server_socket);
			continue;
		}
		SAFECOPY(newfilename,base);
		if(startup->options&WEB_OPT_VIRTUAL_HOSTS && ld->vhost!=NULL) {
			strcat(newfilename,ld->vhost);
			if(ld->vhost[0])
				strcat(newfilename,"-");
		}
		strftime(strchr(newfilename,0),15,"%Y-%m-%d.log",&ld->completed);
		if(strcmp(newfilename,filename)) {
			if(logfile!=NULL)
				fclose(logfile);
			SAFECOPY(filename,newfilename);
			logfile=fopen(filename,"ab");
			lprintf(LOG_INFO,"%04d http logfile is now: %s",server_socket,filename);
		}
		if(logfile!=NULL) {
			if(ld->status) {
				sprintf(sizestr,"%d",ld->size);
				strftime(timestr,sizeof(timestr),"%d/%b/%Y:%H:%M:%S %z",&ld->completed);
				while(lock(fileno(logfile),0,1) && !terminate_http_logging_thread) {
					SLEEP(10);
				}
				fprintf(logfile,"%s %s %s [%s] \"%s\" %d %s \"%s\" \"%s\"\n"
						,ld->hostname?(ld->hostname[0]?ld->hostname:"-"):"-"
						,ld->ident?(ld->ident[0]?ld->ident:"-"):"-"
						,ld->user?(ld->user[0]?ld->user:"-"):"-"
						,timestr
						,ld->request?(ld->request[0]?ld->request:"-"):"-"
						,ld->status
						,ld->size?sizestr:"-"
						,ld->referrer?(ld->referrer[0]?ld->referrer:"-"):"-"
						,ld->agent?(ld->agent[0]?ld->agent:"-"):"-");
				fflush(logfile);
				unlock(fileno(logfile),0,1);
			}
		}
		else {
			logfile=fopen(filename,"ab");
			lprintf(LOG_ERR,"%04d http logfile %s was not open!",server_socket,filename);
		}
		FREE_AND_NULL(ld->hostname);
		FREE_AND_NULL(ld->ident);
		FREE_AND_NULL(ld->user);
		FREE_AND_NULL(ld->request);
		FREE_AND_NULL(ld->referrer);
		FREE_AND_NULL(ld->agent);
		FREE_AND_NULL(ld);
	}
	if(logfile!=NULL) {
		fclose(logfile);
		logfile=NULL;
	}
	thread_down();
	lprintf(LOG_DEBUG,"%04d http logging thread terminated",server_socket);

	http_logging_thread_running=FALSE;
}

void DLLCALL web_server(void* arg)
{
	int				i;
	int				result;
	time_t			start;
	WORD			host_port;
	char			host_ip[32];
	char			path[MAX_PATH+1];
	char			logstr[256];
	SOCKADDR_IN		server_addr={0};
	SOCKADDR_IN		client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket;
	SOCKET			high_socket_set;
	fd_set			socket_set;
	time_t			t;
	time_t			initialized=0;
	char*			p;
	char			compiler[32];
	http_session_t *	session;
	struct timeval tv;
	startup=(web_startup_t*)arg;

	web_ver();	/* get CVS revision */

    if(startup==NULL) {
    	sbbs_beep(100,500);
    	fprintf(stderr, "No startup structure passed!\n");
    	return;
    }

	if(startup->size!=sizeof(web_startup_t)) {	/* verify size */
		sbbs_beep(100,500);
		sbbs_beep(300,500);
		sbbs_beep(100,500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}

#ifdef _THREAD_SUID_BROKEN
	startup->seteuid(TRUE);
#endif

	/* Setup intelligent defaults */
	if(startup->port==0)					startup->port=IPPORT_HTTP;
	if(startup->root_dir[0]==0)				SAFECOPY(startup->root_dir,WEB_DEFAULT_ROOT_DIR);
	if(startup->error_dir[0]==0)			SAFECOPY(startup->error_dir,WEB_DEFAULT_ERROR_DIR);
	if(startup->cgi_dir[0]==0)				SAFECOPY(startup->cgi_dir,WEB_DEFAULT_CGI_DIR);
	if(startup->max_inactivity==0) 			startup->max_inactivity=120; /* seconds */
	if(startup->max_cgi_inactivity==0) 		startup->max_cgi_inactivity=120; /* seconds */
	if(startup->sem_chk_freq==0)			startup->sem_chk_freq=2; /* seconds */
	if(startup->js_max_bytes==0)			startup->js_max_bytes=JAVASCRIPT_MAX_BYTES;
	if(startup->js_cx_stack==0)				startup->js_cx_stack=JAVASCRIPT_CONTEXT_STACK;
	if(startup->ssjs_ext[0]==0)				SAFECOPY(startup->ssjs_ext,".ssjs");
	if(startup->js_ext[0]==0)				SAFECOPY(startup->js_ext,".bbs");

	sprintf(js_server_props.version,"%s %s",server_name,revision);
	js_server_props.version_detail=web_ver();
	js_server_props.clients=&active_clients;
	js_server_props.options=&startup->options;
	js_server_props.interface_addr=&startup->interface_addr;

	uptime=0;
	served=0;
	startup->recycle_now=FALSE;
	startup->shutdown_now=FALSE;
	terminate_server=FALSE;

	do {

		thread_up(FALSE /* setuid */);

		status("Initializing");

		/* Copy html directories */
		SAFECOPY(root_dir,startup->root_dir);
		SAFECOPY(error_dir,startup->error_dir);
		SAFECOPY(cgi_dir,startup->cgi_dir);
		if(startup->temp_dir[0])
			SAFECOPY(temp_dir,startup->temp_dir);
		else
			SAFECOPY(temp_dir,"../temp");

		/* Change to absolute path */
		prep_dir(startup->ctrl_dir, root_dir, sizeof(root_dir));
		prep_dir(startup->ctrl_dir, temp_dir, sizeof(temp_dir));
		prep_dir(root_dir, error_dir, sizeof(error_dir));
		prep_dir(root_dir, cgi_dir, sizeof(cgi_dir));

		/* Trim off trailing slash/backslash */
		if(IS_PATH_DELIM(*(p=lastchar(root_dir))))	*p=0;

		memset(&scfg, 0, sizeof(scfg));

		lprintf(LOG_INFO,"%s Revision %s%s"
			,server_name
			,revision
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			);

		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO,"Compiled %s %s with %s", __DATE__, __TIME__, compiler);

		if(!winsock_startup()) {
			cleanup(1);
			return;
		}

		t=time(NULL);
		lprintf(LOG_INFO,"Initializing on %.24s with options: %lx"
			,CTIME_R(&t,logstr),startup->options);

		if(chdir(startup->ctrl_dir)!=0)
			lprintf(LOG_ERR,"!ERROR %d changing directory to: %s", errno, startup->ctrl_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
		lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, NULL, TRUE, logstr)) {
			lprintf(LOG_ERR,"!ERROR %s",logstr);
			lprintf(LOG_ERR,"!FAILED to load configuration files");
			cleanup(1);
			return;
		}
		scfg_reloaded=TRUE;

		lprintf(LOG_DEBUG,"Temporary file directory: %s", temp_dir);
		MKDIR(temp_dir);
		if(!isdir(temp_dir)) {
			lprintf(LOG_ERR,"!Invalid temp directory: %s", temp_dir);
			cleanup(1);
			return;
		}
		lprintf(LOG_DEBUG,"Root directory: %s", root_dir);
		lprintf(LOG_DEBUG,"Error directory: %s", error_dir);
		lprintf(LOG_DEBUG,"CGI directory: %s", cgi_dir);

		iniFileName(path,sizeof(path),scfg.ctrl_dir,"mime_types.ini");
		if(!read_mime_types(path)) {
			cleanup(1);
			return;
		}

		if(startup->host_name[0]==0)
			SAFECOPY(startup->host_name,scfg.sys_inetaddr);

		if(!(scfg.sys_misc&SM_LOCAL_TZ) && !(startup->options&BBS_OPT_LOCAL_TIMEZONE)) {
			if(putenv("TZ=UTC0"))
				lprintf(LOG_WARNING,"!putenv() FAILED");
			tzset();
		}

		if(uptime==0)
			uptime=time(NULL);	/* this must be done *after* setting the timezone */

		active_clients=0;
		update_clients();

		/* open a socket and wait for a client */

		server_socket = open_socket(SOCK_STREAM);

		if(server_socket == INVALID_SOCKET) {
			lprintf(LOG_ERR,"!ERROR %d creating HTTP socket", ERROR_VALUE);
			cleanup(1);
			return;
		}
		
/*
 *		i=1;
 *		if(setsockopt(server_socket, IPPROTO_TCP, TCP_NOPUSH, &i, sizeof(i)))
 *			lprintf("Cannot set TCP_NOPUSH socket option");
 */

		lprintf(LOG_INFO,"%04d Web Server socket opened",server_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->interface_addr);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->port);

		if(startup->seteuid!=NULL)
			startup->seteuid(FALSE);
		result = retry_bind(server_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)
			,startup->bind_retry_count,startup->bind_retry_delay,"Web Server",lprintf);
		if(startup->seteuid!=NULL)
			startup->seteuid(TRUE);
		if(result != 0) {
			lprintf(LOG_NOTICE,"%s",BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		result = listen(server_socket, 64);

		if(result != 0) {
			lprintf(LOG_ERR,"%04d !ERROR %d (%d) listening on socket"
				,server_socket, result, ERROR_VALUE);
			cleanup(1);
			return;
		}
		lprintf(LOG_INFO,"%04d Web Server listening on port %d"
			,server_socket, startup->port);
		status("Listening");

		lprintf(LOG_INFO,"%04d Web Server thread started", server_socket);

		listInit(&log_list,/* flags */ 0);
		if(startup->options&WEB_OPT_HTTP_LOGGING) {
			/********************/
			/* Start log thread */
			/********************/
			sem_init(&log_sem,0,0);
			_beginthread(http_logging_thread, 0, startup->logfile_base);
		}

		/* Setup recycle/shutdown semaphore file lists */
		semfile_list_init(&shutdown_semfiles,scfg.ctrl_dir,"shutdown","web");
		semfile_list_init(&recycle_semfiles,scfg.ctrl_dir,"recycle","web");
		SAFEPRINTF(path,"%swebsrvr.rec",scfg.ctrl_dir);	/* legacy */
		semfile_list_add(&recycle_semfiles,path);
		if(!initialized) {
			initialized=time(NULL);
			semfile_list_check(&initialized,&recycle_semfiles);
			semfile_list_check(&initialized,&shutdown_semfiles);
		}

		/* signal caller that we've started up successfully */
		if(startup->started!=NULL)
    		startup->started(startup->cbdata);

		while(server_socket!=INVALID_SOCKET && !terminate_server) {

			/* check for re-cycle/shutdown semaphores */
			if(active_clients==0) {
				if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
					if((p=semfile_list_check(&initialized,&recycle_semfiles))!=NULL) {
						lprintf(LOG_INFO,"%04d Recycle semaphore file (%s) detected"
							,server_socket,p);
						break;
					}
#if 0	/* unused */
					if(startup->recycle_sem!=NULL && sem_trywait(&startup->recycle_sem)==0)
						startup->recycle_now=TRUE;
#endif
					if(startup->recycle_now==TRUE) {
						lprintf(LOG_INFO,"%04d Recycle semaphore signaled",server_socket);
						startup->recycle_now=FALSE;
						break;
					}
				}
				if(((p=semfile_list_check(&initialized,&shutdown_semfiles))!=NULL
						&& lprintf(LOG_INFO,"%04d Shutdown semaphore file (%s) detected"
							,server_socket,p))
					|| (startup->shutdown_now==TRUE
						&& lprintf(LOG_INFO,"%04d Shutdown semaphore signaled"
							,server_socket))) {
					startup->shutdown_now=FALSE;
					terminate_server=TRUE;
					break;
				}
			}	

			/* now wait for connection */

			FD_ZERO(&socket_set);
			FD_SET(server_socket,&socket_set);
			high_socket_set=server_socket+1;

			tv.tv_sec=startup->sem_chk_freq;
			tv.tv_usec=0;

			if((i=select(high_socket_set,&socket_set,NULL,NULL,&tv))<1) {
				if(i==0)
					continue;
				if(ERROR_VALUE==EINTR)
					lprintf(LOG_INFO,"Web Server listening interrupted");
				else if(ERROR_VALUE == ENOTSOCK)
            		lprintf(LOG_INFO,"Web Server socket closed");
				else
					lprintf(LOG_WARNING,"!ERROR %d selecting socket",ERROR_VALUE);
				continue;
			}

			if(server_socket==INVALID_SOCKET)	/* terminated */
				break;

			client_addr_len = sizeof(client_addr);

			if(server_socket!=INVALID_SOCKET
				&& FD_ISSET(server_socket,&socket_set)) {
				client_socket = accept(server_socket, (struct sockaddr *)&client_addr
	        		,&client_addr_len);
			}
			else {
				lprintf(LOG_NOTICE,"!NO SOCKETS set by select");
				continue;
			}

			if(client_socket == INVALID_SOCKET)	{
				lprintf(LOG_WARNING,"!ERROR %d accepting connection", ERROR_VALUE);
#ifdef _WIN32
				if(WSAGetLastError()==WSAENOBUFS)	/* recycle (re-init WinSock) on this error */
					break;
#endif
				continue;
			}

			if(startup->socket_open!=NULL)
				startup->socket_open(startup->cbdata,TRUE);

			SAFECOPY(host_ip,inet_ntoa(client_addr.sin_addr));

			if(trashcan(&scfg,host_ip,"ip-silent")) {
				close_socket(client_socket);
				continue;
			}

			if(startup->max_clients && active_clients>=startup->max_clients) {
				lprintf(LOG_WARNING,"%04d !MAXMIMUM CLIENTS (%d) reached, access denied"
					,client_socket, startup->max_clients);
				mswait(3000);
				close_socket(client_socket);
				continue;
			}

			host_port=ntohs(client_addr.sin_port);

			lprintf(LOG_INFO,"%04d HTTP connection accepted from: %s port %u"
				,client_socket
				,host_ip, host_port);

			if((session=malloc(sizeof(http_session_t)))==NULL) {
				lprintf(LOG_CRIT,"%04d !ERROR allocating %u bytes of memory for http_session_t"
					,client_socket, sizeof(http_session_t));
				mswait(3000);
				close_socket(client_socket);
				continue;
			}

			memset(session, 0, sizeof(http_session_t));
			SAFECOPY(session->host_ip,host_ip);
			session->addr=client_addr;
   			session->socket=client_socket;
			session->js_branch.auto_terminate=TRUE;
			session->js_branch.terminated=&terminate_server;
			session->js_branch.limit=startup->js_branch_limit;
			session->js_branch.gc_interval=startup->js_gc_interval;
			session->js_branch.yield_interval=startup->js_yield_interval;

			_beginthread(http_session_thread, 0, session);
			served++;
		}

		/* Wait for active clients to terminate */
		if(active_clients) {
			lprintf(LOG_DEBUG,"%04d Waiting for %d active clients to disconnect..."
				,server_socket, active_clients);
			start=time(NULL);
			while(active_clients) {
				if(time(NULL)-start>startup->max_inactivity) {
					lprintf(LOG_WARNING,"%04d !TIMEOUT waiting for %d active clients"
						,server_socket, active_clients);
					break;
				}
				mswait(100);
			}
		}

		if(http_logging_thread_running) {
			terminate_http_logging_thread=TRUE;
			sem_post(&log_sem);
			mswait(100);
		}
		if(http_logging_thread_running) {
			lprintf(LOG_DEBUG,"%04d Waiting for HTTP logging thread to terminate..."
				,server_socket);
			start=time(NULL);
			while(http_logging_thread_running) {
				if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_WARNING,"%04d !TIMEOUT waiting for HTTP logging thread to "
            			"terminate", server_socket);
					break;
				}
				mswait(100);
			}
		}

		cleanup(0);

		if(!terminate_server) {
			lprintf(LOG_INFO,"Recycling server...");
			mswait(2000);
			if(startup->recycle!=NULL)
				startup->recycle(startup->cbdata);
		}

	} while(!terminate_server);
}
