/* xtrn.cpp */

/* Synchronet external program support routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2014 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "sbbs.h"
#include "cmdshell.h"
#include "telnet.h"

#include <signal.h>			// kill()

#ifdef __unix__
	#include <sys/wait.h>	// WEXITSTATUS

	#define TTYDEFCHARS		// needed for ttydefchars definition
	#include <sys/ttydefaults.h>	// Linux - it's motherfucked.
#if defined(__FreeBSD__)
	#include <libutil.h>	// forkpty()
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DARWIN__)
	#include <util.h>
#elif defined(__linux__)
	#include <pty.h>
#elif defined(__QNX__)
#if 0
	#include <unix.h>
#else
	#define NEEDS_FORKPTY
#endif
#endif

	#ifdef NEEDS_FORKPTY
	#include <grp.h>
	#endif

	#include <termios.h>

/*
 * Control Character Defaults
 */
#ifndef CTRL
	#define CTRL(x)	(x&037)
#endif
#ifndef CEOF
	#define	CEOF		CTRL('d')
#endif
#ifndef CEOL
	#define	CEOL		0xff		/* XXX avoid _POSIX_VDISABLE */
#endif
#ifndef CERASE
	#define	CERASE		0177
#endif
#ifndef CERASE2
	#define	CERASE2		CTRL('h')
#endif
#ifndef CINTR
	#define	CINTR		CTRL('c')
#endif
#ifndef CSTATUS
	#define	CSTATUS		CTRL('t')
#endif
#ifndef CKILL
	#define	CKILL		CTRL('u')
#endif
#ifndef CMIN
	#define	CMIN		1
#endif
#ifndef CQUIT
	#define	CQUIT		034		/* FS, ^\ */
#endif
#ifndef CSUSP
	#define	CSUSP		CTRL('z')
#endif
#ifndef CTIME
	#define	CTIME		0
#endif
#ifndef CDSUSP
	#define	CDSUSP		CTRL('y')
#endif
#ifndef CSTART
	#define	CSTART		CTRL('q')
#endif
#ifndef CSTOP
	#define	CSTOP		CTRL('s')
#endif
#ifndef CLNEXT
	#define	CLNEXT		CTRL('v')
#endif
#ifndef CDISCARD
	#define	CDISCARD 	CTRL('o')
#endif
#ifndef CWERASE
	#define	CWERASE 	CTRL('w')
#endif
#ifndef CREPRINT
	#define	CREPRINT 	CTRL('r')
#endif
#ifndef CEOT
	#define	CEOT		CEOF
#endif
/* compat */
#ifndef CBRK
	#define	CBRK		CEOL
#endif
#ifndef CRPRNT
	#define CRPRNT		CREPRINT
#endif
#ifndef CFLUSH
	#define	CFLUSH		CDISCARD
#endif

#ifndef TTYDEF_IFLAG
	#define TTYDEF_IFLAG    (BRKINT | ICRNL | IMAXBEL | IXON | IXANY)
#endif
#ifndef TTYDEF_OFLAG
	#define TTYDEF_OFLAG    (OPOST | ONLCR)
#endif
#ifndef TTYDEF_LFLAG
	#define TTYDEF_LFLAG    (ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#endif
#ifndef TTYDEF_CFLAG
	#define TTYDEF_CFLAG    (CREAD | CS8 | HUPCL)
#endif
#if defined(__QNX__) || defined(__solaris__) || defined(__NetBSD__)
	static cc_t     ttydefchars[NCCS] = {
        CEOF,   CEOL,   CEOL,   CERASE, CWERASE, CKILL, CREPRINT,
        CERASE2, CINTR, CQUIT,  CSUSP,  CDSUSP, CSTART, CSTOP,  CLNEXT,
        CDISCARD, CMIN, CTIME,  CSTATUS
#ifndef __solaris__
	, _POSIX_VDISABLE
#endif
	};
#endif

#endif	/* __unix__ */

#define XTRN_IO_BUF_LEN 10000	/* 50% of IO_THREAD_BUF_SIZE */

/*****************************************************************************/
/* Interrupt routine to expand WWIV Ctrl-C# codes into ANSI escape sequences */
/*****************************************************************************/
BYTE* wwiv_expand(BYTE* buf, ulong buflen, BYTE* outbuf, ulong& newlen
	,ulong user_misc, bool& ctrl_c)
{
    char	ansi_seq[32];
	ulong 	i,j,k;

    for(i=j=0;i<buflen;i++) {
        if(buf[i]==CTRL_C) {	/* WWIV color escape char */
            ctrl_c=true;
            continue;
        }
        if(!ctrl_c) {
            outbuf[j++]=buf[i];
            continue;
        }
        ctrl_c=false;
        if(user_misc&ANSI) {
            switch(buf[i]) {
                default:
                    strcpy(ansi_seq,"\x1b[0m");          /* low grey */
                    break;
                case '1':
                    strcpy(ansi_seq,"\x1b[0;1;36m");     /* high cyan */
                    break;
                case '2':
                    strcpy(ansi_seq,"\x1b[0;1;33m");     /* high yellow */
                    break;
                case '3':
                    strcpy(ansi_seq,"\x1b[0;35m");       /* low magenta */
                    break;
                case '4':
                    strcpy(ansi_seq,"\x1b[0;1;44m");     /* white on blue */
                    break;
                case '5':
                    strcpy(ansi_seq,"\x1b[0;32m");       /* low green */
                    break;
                case '6':
                    strcpy(ansi_seq,"\x1b[0;1;5;31m");   /* high blinking red */
                    break;
                case '7':
                    strcpy(ansi_seq,"\x1b[0;1;34m");     /* high blue */
                    break;
                case '8':
                    strcpy(ansi_seq,"\x1b[0;34m");       /* low blue */
                    break;
                case '9':
                    strcpy(ansi_seq,"\x1b[0;36m");       /* low cyan */
                    break;
            }
            for(k=0;ansi_seq[k];k++)
                outbuf[j++]=ansi_seq[k];
        }
    }
    newlen=j;
    return(outbuf);
}

/*****************************************************************************/
// Escapes Telnet IAC (255) by doubling the IAC char
/*****************************************************************************/
BYTE* telnet_expand(BYTE* inbuf, ulong inlen, BYTE* outbuf, ulong& newlen)
{
	BYTE*   first_iac;
	ulong	i,outlen;

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if(first_iac==NULL) {	/* Nothing to expand */
		newlen=inlen;
		return(inbuf);
	}

	outlen=first_iac-inbuf;
	memcpy(outbuf, inbuf, outlen);

    for(i=outlen;i<inlen;i++) {
		if(inbuf[i]==TELNET_IAC)
			outbuf[outlen++]=TELNET_IAC;
		outbuf[outlen++]=inbuf[i];
	}
    newlen=outlen;
    return(outbuf);
}

static bool native_executable(scfg_t* cfg, const char* cmdline, long mode)
{
	char*	p;
	char	str[MAX_PATH+1];
	char	name[64];
	char	base[64];
	unsigned i;

	if(mode&EX_NATIVE)
		return(TRUE);

    SAFECOPY(str,cmdline);				/* Set str to program name only */
	truncstr(str," ");
    SAFECOPY(name,getfname(str));
	SAFECOPY(base,name);
	if((p=getfext(base))!=NULL)
		*p=0;

    for(i=0;i<cfg->total_natvpgms;i++)
        if(stricmp(name,cfg->natvpgm[i]->name)==0
		|| stricmp(base,cfg->natvpgm[i]->name)==0)
            break;
    return(i<cfg->total_natvpgms);
}

#define XTRN_LOADABLE_MODULE(cmdline,startup_dir)			\
	if(cmdline[0]=='*')		/* Baja module or JavaScript */	\
		return(exec_bin(cmdline+1,&main_csi,startup_dir))				
#ifdef JAVASCRIPT
	#define XTRN_LOADABLE_JS_MODULE(cmdline,startup_dir)	\
	if(cmdline[0]=='?') 	/* JavaScript */				\
		return(js_execfile(cmdline+1,startup_dir))						
#else
	#define XTRN_LOADABLE_JS_MODULE
#endif

#ifdef _WIN32

#include "execvxd.h"	/* Win9X FOSSIL VxD API */

extern SOCKET node_socket[];

// -------------------------------------------------------------------------
// GetAddressOfOpenVxDHandle
//
// This function returns the address of OpenVxDHandle. OpenVxDHandle is a 
// KERNEL32 function that returns a ring 0 event handle that corresponds to a
// given ring 3 event handle. The ring 0 handle can be used by VxDs to
// synchronize with the Win32 app.
//
typedef HANDLE (WINAPI *OPENVXDHANDLE)(HANDLE);

OPENVXDHANDLE GetAddressOfOpenVxDHandle(void)
{
	return((OPENVXDHANDLE)GetProcAddress(hK32, "OpenVxDHandle"));
}

/*****************************************************************************/
// Expands Single CR to CRLF
/*****************************************************************************/
BYTE* cr_expand(BYTE* inbuf, ulong inlen, BYTE* outbuf, ulong& newlen)
{
	ulong	i,j;

	for(i=j=0;i<inlen;i++) {
		outbuf[j++]=inbuf[i];
		if(inbuf[i]=='\r')
			outbuf[j++]='\n';
	}
	newlen=j;
    return(outbuf);
}

static void add_env_var(str_list_t* list, const char* var, const char* val)
{
	char	str[MAX_PATH*2];
	SetEnvironmentVariable(var,NULL);	/* Delete in current process env */
	SAFEPRINTF2(str,"%s=%s",var,val);
	strListPush(list,str);
}

/* Clean-up resources while preserving current LastError value */
#define XTRN_CLEANUP												\
	last_error=GetLastError();										\
    if(vxd!=INVALID_HANDLE_VALUE)		CloseHandle(vxd);			\
	if(rdslot!=INVALID_HANDLE_VALUE)	CloseHandle(rdslot);		\
	if(wrslot!=INVALID_HANDLE_VALUE)	CloseHandle(wrslot);		\
	if(start_event!=NULL)				CloseHandle(start_event);	\
	if(hungup_event!=NULL)				CloseHandle(hungup_event);	\
	if(hangup_event!=NULL)				CloseHandle(hangup_event);	\
	ReleaseMutex(exec_mutex);										\
	SetLastError(last_error)

/****************************************************************************/
/* Runs an external program 												*/
/****************************************************************************/
int sbbs_t::external(const char* cmdline, long mode, const char* startup_dir)
{
	char	str[MAX_PATH+1];
	char*	env_block=NULL;
	char*	env_strings;
	const char* p_startup_dir;
	char	path[MAX_PATH+1];
    char	fullcmdline[MAX_PATH+1];
	char	realcmdline[MAX_PATH+1];
	char	comspec_str[MAX_PATH+1];
	char	title[MAX_PATH+1];
	BYTE	buf[XTRN_IO_BUF_LEN],*bp;
    BYTE 	telnet_buf[XTRN_IO_BUF_LEN*2];
    BYTE 	output_buf[XTRN_IO_BUF_LEN*2];
    BYTE 	wwiv_buf[XTRN_IO_BUF_LEN*2];
    bool	wwiv_flag=false;
    bool	native=false;			// DOS program by default
	bool	nt=false;				// WinNT/2K? 
    bool	was_online=true;
	bool	rio_abortable_save=rio_abortable;
	bool	use_pipes=false;	// NT-compatible console redirection
	BOOL	success;
	BOOL	processTerminated=false;
	uint	i;
    time_t	hungup=0;
	HANDLE	vxd=INVALID_HANDLE_VALUE;
	HANDLE	rdslot=INVALID_HANDLE_VALUE;
	HANDLE	wrslot=INVALID_HANDLE_VALUE;
	HANDLE  start_event=NULL;
	HANDLE	hungup_event=NULL;
	HANDLE	hangup_event=NULL;
	HANDLE	rdoutpipe;
	HANDLE	wrinpipe;
    PROCESS_INFORMATION process_info;
	DWORD	hVM;
	unsigned long	rd;
    unsigned long	wr;
    unsigned long	len;
    DWORD	avail;
	unsigned long	dummy;
	unsigned long	msglen;
	unsigned long	retval;
	DWORD	last_error;
	DWORD	loop_since_io=0;
	struct	tm tm;
	str_list_t	env_list;
	sbbsexec_start_t start;
	OPENVXDHANDLE OpenVxDHandle;

	if(online!=ON_REMOTE || cfg.node_num==0)
		eprintf(LOG_DEBUG,"Executing external: %s",cmdline);
	else
		lprintf(LOG_DEBUG,"Node %d Executing external: %s",cfg.node_num,cmdline);

	if(startup_dir!=NULL && startup_dir[0] && !isdir(startup_dir)) {
		errormsg(WHERE, ERR_CHK, startup_dir, 0);
		return -1;
	}

	XTRN_LOADABLE_MODULE(cmdline,startup_dir);
	XTRN_LOADABLE_JS_MODULE(cmdline,startup_dir);

	attr(cfg.color[clr_external]);		/* setup default attributes */

	native = native_executable(&cfg, cmdline, mode);

	if(!native && (startup->options&BBS_OPT_NO_DOS)) {
		bprintf("Sorry, DOS programs are not supported on this node.\r\n");
		return -1;
	}

	if(mode&EX_SH || strcspn(cmdline,"<>|")!=strlen(cmdline)) 
		sprintf(comspec_str,"%s /C ", comspec);
	else
		comspec_str[0]=0;

    if(startup_dir && cmdline[1]!=':' && cmdline[0]!='/'
    	&& cmdline[0]!='\\' && cmdline[0]!='.')
       	sprintf(fullcmdline, "%s%s%s", comspec_str, startup_dir, cmdline);
    else
    	sprintf(fullcmdline, "%s%s", comspec_str, cmdline);

	SAFECOPY(realcmdline, fullcmdline);	// for errormsg if failed to execute

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		memset(&tm,0,sizeof(tm));

	OpenVxDHandle=GetAddressOfOpenVxDHandle();

	if(OpenVxDHandle==NULL) 
		nt=true;	// Windows NT/2000

	if(!nt && !native && !(cfg.xtrn_misc&XTRN_NO_MUTEX)
		&& (retval=WaitForSingleObject(exec_mutex,5000))!=WAIT_OBJECT_0) {
		errormsg(WHERE, ERR_TIMEOUT, "exec_mutex", retval);
		return(GetLastError());
	}

	if(native && mode&EX_STDOUT && !(mode&EX_OFFLINE))
		use_pipes=true;

 	if(native) { // Native (32-bit) external

		if((env_list=strListInit())==NULL) {
			XTRN_CLEANUP;
        	errormsg(WHERE, ERR_CREATE, "env_list", 0);
            return(errno);
		}

		// Current environment passed to child process
		sprintf(str,"%sprotocol.log",cfg.node_dir);			
		add_env_var(&env_list,"DSZLOG",str);
		add_env_var(&env_list,"SBBSNODE",cfg.node_dir);
		add_env_var(&env_list,"SBBSCTRL",cfg.ctrl_dir);
		add_env_var(&env_list,"SBBSDATA",cfg.data_dir);
		add_env_var(&env_list,"SBBSEXEC",cfg.exec_dir);
		sprintf(str,"%d",cfg.node_num);
		add_env_var(&env_list,"SBBSNNUM",str);
		/* date/time env vars */
		sprintf(str,"%02u",tm.tm_mday);	
		add_env_var(&env_list,"DAY",str);
		add_env_var(&env_list,"WEEKDAY",wday[tm.tm_wday]);
		add_env_var(&env_list,"MONTHNAME",mon[tm.tm_mon]);
		sprintf(str,"%02u",tm.tm_mon+1);
		add_env_var(&env_list,"MONTH",str);
		sprintf(str,"%u",1900+tm.tm_year);
		add_env_var(&env_list,"YEAR",str);

		env_strings=GetEnvironmentStrings();
		env_block=strListCopyBlock(env_strings);
		if(env_strings!=NULL)
			FreeEnvironmentStrings(env_strings);
		env_block=strListAppendBlock(env_block,env_list);
		strListFree(&env_list);
		if(env_block==NULL) {
			XTRN_CLEANUP;
        	errormsg(WHERE, ERR_CREATE, "env_block", 0);
            return(errno);
		}

    } else { // DOS external

		// DOS-compatible (short) paths
		char node_dir[MAX_PATH+1];
		char ctrl_dir[MAX_PATH+1];
		char data_dir[MAX_PATH+1];
		char exec_dir[MAX_PATH+1];

		// in case GetShortPathName fails
		SAFECOPY(node_dir,cfg.node_dir);
		SAFECOPY(ctrl_dir,cfg.ctrl_dir);
		SAFECOPY(data_dir,cfg.data_dir);
		SAFECOPY(exec_dir,cfg.exec_dir);

		GetShortPathName(cfg.node_dir,node_dir,sizeof(node_dir));
		GetShortPathName(cfg.ctrl_dir,ctrl_dir,sizeof(node_dir));
		GetShortPathName(cfg.data_dir,data_dir,sizeof(data_dir));
		GetShortPathName(cfg.exec_dir,exec_dir,sizeof(exec_dir));

		sprintf(path,"%sDOSXTRN.RET", cfg.node_dir);
		remove(path);

    	// Create temporary environment file
    	sprintf(path,"%sDOSXTRN.ENV", node_dir);
        FILE* fp=fopen(path,"w");
        if(fp==NULL) {
			XTRN_CLEANUP;
        	errormsg(WHERE, ERR_CREATE, path, 0);
            return(errno);
        }
        fprintf(fp, "%s\n", fullcmdline);
		fprintf(fp, "DSZLOG=%sPROTOCOL.LOG\n", node_dir);
        fprintf(fp, "SBBSNODE=%s\n", node_dir);
        fprintf(fp, "SBBSCTRL=%s\n", ctrl_dir);
		fprintf(fp, "SBBSDATA=%s\n", data_dir);
		fprintf(fp, "SBBSEXEC=%s\n", exec_dir);
        fprintf(fp, "SBBSNNUM=%d\n", cfg.node_num);
		/* date/time env vars */
		fprintf(fp, "DAY=%02u\n", tm.tm_mday);
		fprintf(fp, "WEEKDAY=%s\n",wday[tm.tm_wday]);
		fprintf(fp, "MONTHNAME=%s\n",mon[tm.tm_mon]);
		fprintf(fp, "MONTH=%02u\n",tm.tm_mon+1);
		fprintf(fp, "YEAR=%u\n",1900+tm.tm_year);
        fclose(fp);

        sprintf(fullcmdline, "%sDOSXTRN.EXE %s", cfg.exec_dir, path);

		if(!(mode&EX_OFFLINE) && nt) {	// Windows NT/2000
			i=SBBSEXEC_MODE_FOSSIL;
			if(mode&EX_STDIN)
           		i|=SBBSEXEC_MODE_DOS_IN;
			if(mode&EX_STDOUT)
        		i|=SBBSEXEC_MODE_DOS_OUT;
			sprintf(str," NT %u %u"
				,cfg.node_num,i);
			strcat(fullcmdline,str);

			sprintf(str,"sbbsexec_hungup%d",cfg.node_num);
			if((hungup_event=CreateEvent(
				 NULL	// pointer to security attributes
				,TRUE	// flag for manual-reset event
				,FALSE  // flag for initial state
				,str	// pointer to event-object name
				))==NULL) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return(GetLastError());
			}

			sprintf(str,"sbbsexec_hangup%d",cfg.node_num);
			if((hangup_event=CreateEvent(
				 NULL	// pointer to security attributes
				,TRUE	// flag for manual-reset event
				,FALSE  // flag for initial state
				,str	// pointer to event-object name
				))==NULL) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return(GetLastError());
			}

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\rd%d"
				,cfg.node_num);
			rdslot=CreateMailslot(str
				,sizeof(buf)/2			// Maximum message size (0=unlimited)
				,0						// Read time-out
				,NULL);                 // Security
			if(rdslot==INVALID_HANDLE_VALUE) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return(GetLastError());
			}
		}
		else if(!(mode&EX_OFFLINE)) {

   			// Load vxd to intercept interrupts

			sprintf(str,"\\\\.\\%s%s",cfg.exec_dir, SBBSEXEC_VXD);
			if((vxd=CreateFile(str,0,0,0
				,CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE,0))
				 ==INVALID_HANDLE_VALUE) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_OPEN, str, 0);
				return(GetLastError());
			}

			if((start_event=CreateEvent(
				 NULL	// pointer to security attributes
				,TRUE	// flag for manual-reset event
				,FALSE  // flag for initial state
				,NULL	// pointer to event-object name
				))==NULL) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, "exec start event", 0);
				return(GetLastError());
			}

			if(OpenVxDHandle!=NULL)
				start.event=OpenVxDHandle(start_event);
			else
				start.event=start_event;

			start.mode=SBBSEXEC_MODE_FOSSIL;
			if(mode&EX_STDIN)
           		start.mode|=SBBSEXEC_MODE_DOS_IN;
			if(mode&EX_STDOUT)
        		start.mode|=SBBSEXEC_MODE_DOS_OUT;

			sprintf(str," 95 %u %u"
				,cfg.node_num,start.mode);
			strcat(fullcmdline,str);

			if(!DeviceIoControl(
				vxd,					// handle to device of interest
				SBBSEXEC_IOCTL_START,	// control code of operation to perform
				&start,					// pointer to buffer to supply input data
				sizeof(start),			// size of input buffer
				NULL,					// pointer to buffer to receive output data
				0,						// size of output buffer
				&rd,					// pointer to variable to receive output byte count
				NULL 					// Overlapped I/O
				)) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_START);
				return(GetLastError());
			}
		}
    }

	if(startup_dir!=NULL && startup_dir[0])
		p_startup_dir=startup_dir;
	else
		p_startup_dir=NULL;
    STARTUPINFO startup_info={0};
    startup_info.cb=sizeof(startup_info);
	if(mode&EX_OFFLINE)
		startup_info.lpTitle=NULL;
	else {
		sprintf(title,"%s running %s on node %d"
			,useron.number ? useron.alias : "Event"
			,realcmdline
			,cfg.node_num);
		startup_info.lpTitle=title;
	}
    if(startup->options&BBS_OPT_XTRN_MINIMIZED) {
    	startup_info.wShowWindow=SW_SHOWMINNOACTIVE;
        startup_info.dwFlags|=STARTF_USESHOWWINDOW;
    }
	if(use_pipes) {
		// Set up the security attributes struct.
		SECURITY_ATTRIBUTES sa;
		memset(&sa,0,sizeof(sa));
		sa.nLength= sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		// Create the child output pipe (override default 4K buffer size)
		if(!CreatePipe(&rdoutpipe,&startup_info.hStdOutput,&sa,sizeof(buf))) {
			errormsg(WHERE,ERR_CREATE,"stdout pipe",0);
			return(GetLastError());
		}
		startup_info.hStdError=startup_info.hStdOutput;

		// Create the child input pipe.
		if(!CreatePipe(&startup_info.hStdInput,&wrinpipe,&sa,sizeof(buf))) {
			errormsg(WHERE,ERR_CREATE,"stdin pipe",0);
			return(GetLastError());
		}

		DuplicateHandle(
			GetCurrentProcess(), rdoutpipe,
			GetCurrentProcess(), &rdslot, 0, FALSE, DUPLICATE_SAME_ACCESS);

		DuplicateHandle(
			GetCurrentProcess(), wrinpipe,
			GetCurrentProcess(), &wrslot, 0, FALSE, DUPLICATE_SAME_ACCESS);

		CloseHandle(rdoutpipe);
		CloseHandle(wrinpipe);

		startup_info.dwFlags|=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    	startup_info.wShowWindow=SW_HIDE;
	}
	if(native && !(mode&EX_OFFLINE)) {

		if(!(mode&EX_STDIN) && input_thread_running) {
			pthread_mutex_lock(&input_thread_mutex);
			input_thread_mutex_locked=true;
		}

		if(!(mode&EX_STDOUT)) {	 /* Native Socket I/O program */
			/* Enable the Nagle algorithm */
			BOOL nodelay=FALSE;
			setsockopt(client_socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
		}
	}

    success=CreateProcess(
		NULL,			// pointer to name of executable module
		fullcmdline,  	// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		native && !(mode&EX_OFFLINE),	 			// handle inheritance flag
		CREATE_NEW_CONSOLE/*|CREATE_SEPARATE_WOW_VDM*/, // creation flags
        env_block, 		// pointer to new environment block
		p_startup_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		);

	strListFreeBlock(env_block);

	if(!success) {
		XTRN_CLEANUP;
		if(input_thread_mutex_locked && input_thread_running) {
			pthread_mutex_unlock(&input_thread_mutex);
			input_thread_mutex_locked=false;
		}
		SetLastError(last_error);	/* Restore LastError */
        errormsg(WHERE, ERR_EXEC, realcmdline, mode);
        return(GetLastError());
    }

#if 0
	char dbgstr[256];
	sprintf(dbgstr,"Node %d created: hProcess %X hThread %X processID %X threadID %X\n"
		,cfg.node_num
		,process_info.hProcess 
		,process_info.hThread 
		,process_info.dwProcessId 
		,process_info.dwThreadId); 
	OutputDebugString(dbgstr);
#endif

	CloseHandle(process_info.hThread);

	if(!native) {

		if(!(mode&EX_OFFLINE) && !nt) {
    		// Wait for notification from VXD that new VM has started
			if((retval=WaitForSingleObject(start_event, 5000))!=WAIT_OBJECT_0) {
				XTRN_CLEANUP;
                TerminateProcess(process_info.hProcess, __LINE__);
				CloseHandle(process_info.hProcess);
				errormsg(WHERE, ERR_TIMEOUT, "start_event", retval);
				return(GetLastError());
			}

			CloseHandle(start_event);
			start_event=NULL;	/* Mark as closed */

			if(!DeviceIoControl(
				vxd,					// handle to device of interest
				SBBSEXEC_IOCTL_COMPLETE,	// control code of operation to perform
				NULL,					// pointer to buffer to supply input data
				0,						// size of input buffer
				&hVM,					// pointer to buffer to receive output data
				sizeof(hVM),			// size of output buffer
				&rd,					// pointer to variable to receive output byte count
				NULL					// Overlapped I/O
				)) {
				XTRN_CLEANUP;
                TerminateProcess(process_info.hProcess, __LINE__);
				CloseHandle(process_info.hProcess);
				errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_COMPLETE);
				return(GetLastError());
			}
		}
	}
    ReleaseMutex(exec_mutex);

	/* Disable Ctrl-C checking */
	if(!(mode&EX_OFFLINE))
		rio_abortable=false;

    // Executing app in foreground?, monitor
    retval=STILL_ACTIVE;
    while(!(mode&EX_BG)) {
		if(mode&EX_CHKTIME)
			gettimeleft();
        if(!online && !(mode&EX_OFFLINE)) { // Tell VXD/VDD and external that user hung-up
        	if(was_online) {
				sprintf(str,"%s hung-up in external program",useron.alias);
				logline(LOG_NOTICE,"X!",str);
            	hungup=time(NULL);
				if(!native) {
					if(nt)
						SetEvent(hungup_event);
					else if(!DeviceIoControl(
						vxd,		// handle to device of interest
						SBBSEXEC_IOCTL_DISCONNECT,	// operation to perform
						&hVM,		// pointer to buffer to supply input data
						sizeof(hVM),// size of input buffer
						NULL,		// pointer to buffer to receive output data
						0,			// size of output buffer
						&rd,		// pointer to variable to receive output byte count
						NULL		// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_DISCONNECT);
						break;
					}
				}
	            was_online=false;
            }
            if(hungup && time(NULL)-hungup>5 && !processTerminated) {
				lprintf(LOG_INFO,"Node %d Terminating process from line %d",cfg.node_num,__LINE__);
				processTerminated=TerminateProcess(process_info.hProcess, 2112);
			}
        }
		if((native && !use_pipes) || mode&EX_OFFLINE) {	
			/* Monitor for process termination only */
			if(WaitForSingleObject(process_info.hProcess,1000)==WAIT_OBJECT_0)
				break;
		} else {

			if(nt || use_pipes) {	// Windows NT/2000

				/* Write to VDD */

				wr=RingBufPeek(&inbuf,buf,sizeof(buf));
				if(wr) {
					if(!use_pipes && wrslot==INVALID_HANDLE_VALUE) {
						sprintf(str,"\\\\.\\mailslot\\sbbsexec\\wr%d"
							,cfg.node_num);
						wrslot=CreateFile(str
							,GENERIC_WRITE
							,FILE_SHARE_READ
							,NULL
							,OPEN_EXISTING
							,FILE_ATTRIBUTE_NORMAL
							,(HANDLE) NULL);
						if(wrslot==INVALID_HANDLE_VALUE)
							lprintf(LOG_DEBUG,"Node %d !ERROR %u opening %s", cfg.node_num, GetLastError(), str);
						else
							lprintf(LOG_DEBUG,"Node %d CreateFile(%s)=0x%x", cfg.node_num, str, wrslot);
					}
					
					/* CR expansion */
					if(use_pipes) 
						bp=cr_expand(buf,wr,output_buf,wr);
					else
						bp=buf;

					len=0;
					if(wrslot==INVALID_HANDLE_VALUE)
						lprintf(LOG_WARNING,"Node %d VDD Open failed (not loaded yet?)",cfg.node_num);
					else if(!WriteFile(wrslot,bp,wr,&len,NULL)) {
						lprintf(LOG_ERR,"Node %d !VDD WriteFile(0x%x, %u) FAILURE (Error=%u)", cfg.node_num, wrslot, wr, GetLastError());
						if(GetMailslotInfo(wrslot,&wr,NULL,NULL,NULL))
							lprintf(LOG_DEBUG,"Node %d !VDD MailSlot max_msg_size=%u", cfg.node_num, wr);
						else
							lprintf(LOG_DEBUG,"Node %d !GetMailslotInfo(0x%x)=%u", cfg.node_num, wrslot, GetLastError());
					} else {
						if(len!=wr)
							lprintf(LOG_WARNING,"Node %d VDD short write (%u instead of %u)",cfg.node_num,len,wr);
						RingBufRead(&inbuf, NULL, len);
						if(use_pipes && !(mode&EX_NOECHO)) {
							/* echo */
							RingBufWrite(&outbuf, bp, len);
						}
					}
					wr=len;
				}

				/* Read from VDD */

				rd=0;
				len=sizeof(buf);
				avail=RingBufFree(&outbuf)/2;	// leave room for wwiv/telnet expansion
#if 0
				if(avail==0)
					lprintf("Node %d !output buffer full (%u bytes)"
						,cfg.node_num,RingBufFull(&outbuf));
#endif
				if(len>avail)
            		len=avail;

				while(rd<len) {
					unsigned long waiting=0;

					if(use_pipes)
						PeekNamedPipe(
							rdslot,             // handle to pipe to copy from
							NULL,               // pointer to data buffer
							0,					// size, in bytes, of data buffer
							NULL,				// pointer to number of bytes read
							&waiting,			// pointer to total number of bytes available
							NULL				// pointer to unread bytes in this message
							);
					else
						GetMailslotInfo(
							rdslot,				// mailslot handle 
 							NULL,				// address of maximum message size 
							NULL,				// address of size of next message 
							&waiting,			// address of number of messages 
 							NULL				// address of read time-out 
							);
					if(!waiting)
						break;
					if(ReadFile(rdslot,buf+rd,len-rd,&msglen,NULL)==FALSE || msglen<1)
						break;
					rd+=msglen;
				}

				if(rd) {
					if(mode&EX_WWIV) {
                		bp=wwiv_expand(buf, rd, wwiv_buf, rd, useron.misc, wwiv_flag);
						if(rd>sizeof(wwiv_buf))
							lprintf(LOG_ERR,"WWIV_BUF OVERRUN");
					} else if(telnet_mode&TELNET_MODE_OFF) {
						bp=buf;
					} else {
                		bp=telnet_expand(buf, rd, telnet_buf, rd);
						if(rd>sizeof(telnet_buf))
							lprintf(LOG_ERR,"TELNET_BUF OVERRUN");
					}
					if(rd>RingBufFree(&outbuf)) {
						lprintf(LOG_ERR,"output buffer overflow");
						rd=RingBufFree(&outbuf);
					}
					RingBufWrite(&outbuf, bp, rd);
				}
			} else {	// Windows 9x

				/* Write to VXD */

				wr=RingBufPeek(&inbuf, buf+sizeof(hVM),sizeof(buf)-sizeof(hVM));
				if(wr) {
					*(DWORD*)buf=hVM;
					wr+=sizeof(hVM);
					if(!DeviceIoControl(
						vxd,					// handle to device of interest
						SBBSEXEC_IOCTL_WRITE,	// control code of operation to perform
						buf,					// pointer to buffer to supply input data
						wr,						// size of input buffer
						&rd,					// pointer to buffer to receive output data
						sizeof(rd),				// size of output buffer
						&dummy,	 				// pointer to variable to receive output byte count
						NULL					// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_READ);
						break;
					}
					RingBufRead(&inbuf, NULL, rd);
					wr=rd;
				}
        		/* Read from VXD */
				rd=0;
				len=sizeof(buf);
				avail=RingBufFree(&outbuf)/2;	// leave room for wwiv/telnet expansion
#if 0
				if(avail==0) 
					lprintf("Node %d !output buffer full (%u bytes)"
						,cfg.node_num,RingBufFull(&outbuf));
#endif

				if(len>avail)
            		len=avail;
				if(len) {
					if(!DeviceIoControl(
						vxd,					// handle to device of interest
						SBBSEXEC_IOCTL_READ,	// control code of operation to perform
						&hVM,					// pointer to buffer to supply input data
						sizeof(hVM),			// size of input buffer
						buf,					// pointer to buffer to receive output data
						len,					// size of output buffer
						&rd,					// pointer to variable to receive output byte count
						NULL					// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_READ);
						break;
					}
					if(mode&EX_WWIV) {
                		bp=wwiv_expand(buf, rd, wwiv_buf, rd, useron.misc, wwiv_flag);
						if(rd>sizeof(wwiv_buf))
							lprintf(LOG_ERR,"WWIV_BUF OVERRUN");
					} else if(telnet_mode&TELNET_MODE_OFF) {
						bp=buf;
					} else {
                		bp=telnet_expand(buf, rd, telnet_buf, rd);
						if(rd>sizeof(telnet_buf))
							lprintf(LOG_ERR,"TELNET_BUF OVERRUN");
					}
					if(rd>RingBufFree(&outbuf)) {
						lprintf(LOG_ERR,"output buffer overflow");
						rd=RingBufFree(&outbuf);
					}
					RingBufWrite(&outbuf, bp, rd);
				}
			}
#if defined(_DEBUG) && 0
			if(rd>1) {
				sprintf(str,"Node %d read %5d bytes from xtrn", cfg.node_num, rd);
				OutputDebugString(str);
			}
#endif
            if((!rd && !wr) || hungup) {

				loop_since_io++;	/* number of loop iterations with no I/O */

				/* only check process termination after 300 milliseconds of no I/O */
				/* to allow for last minute reception of output from DOS programs */
				if(loop_since_io>=3) {

					if(online && hangup_event!=NULL
						&& WaitForSingleObject(hangup_event,0)==WAIT_OBJECT_0) {
						lprintf(LOG_NOTICE,"Node %d External program requested hangup (dropped DTR)"
							,cfg.node_num);
						hangup();
					}

					if(WaitForSingleObject(process_info.hProcess,0)==WAIT_OBJECT_0)
						break;	/* Process Terminated */
				}

				/* only check node for interrupt flag every 3 seconds of no I/O */
				if((loop_since_io%30)==0) {	
					// Check if the node has been interrupted
					getnodedat(cfg.node_num,&thisnode,0);
					if(thisnode.misc&NODE_INTR)
						break;
				}

				/* only send telnet GA every 30 seconds of no I/O */
				if((loop_since_io%300)==0) {
#if defined(_DEBUG)
					sprintf(str,"Node %d xtrn idle\n",cfg.node_num);
					OutputDebugString(str);
#endif
					// Let's make sure the socket is up
					// Sending will trigger a socket d/c detection
					if(!(startup->options&BBS_OPT_NO_TELNET_GA))
						send_telnet_cmd(TELNET_GA,0);
				}
				sem_trywait_block(&inbuf.sem,100);
            } else
				loop_since_io=0;
        }
	}

	if(!native && !(mode&EX_OFFLINE) && !nt) {
		if(!DeviceIoControl(
			vxd,					// handle to device of interest
			SBBSEXEC_IOCTL_STOP,	// control code of operation to perform
			&hVM,					// pointer to buffer to supply input data
			sizeof(hVM),			// size of input buffer
			NULL,					// pointer to buffer to receive output data
			0,						// size of output buffer
			&rd,					// pointer to variable to receive output byte count
			NULL					// Overlapped I/O
			)) {
			errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_STOP);
		}
	}

    if(!(mode&EX_BG)) {			/* !background execution */

        if(GetExitCodeProcess(process_info.hProcess, &retval)==FALSE)
            errormsg(WHERE, ERR_CHK, "ExitCodeProcess",(DWORD)process_info.hProcess);

		if(retval==STILL_ACTIVE) {
			lprintf(LOG_INFO,"Node %d Terminating process from line %d",cfg.node_num,__LINE__);
			TerminateProcess(process_info.hProcess, GetLastError());
		}	

	 	// Get return value
		if(!native) {
    		sprintf(str,"%sDOSXTRN.RET", cfg.node_dir);
			FILE* fp=fopen(str,"r");
			if(fp!=NULL) {
				fscanf(fp,"%d",&retval);
				fclose(fp);
			}
		}
	}

	XTRN_CLEANUP;
	CloseHandle(process_info.hProcess);

	if(!(mode&EX_OFFLINE)) {	/* !off-line execution */

		if(native) {
			
			/* Re-enable blocking (incase disabled by xtrn program) */
			ulong l=0;
			ioctlsocket(client_socket, FIONBIO, &l);

			/* Re-set socket options */
			if(set_socket_options(&cfg, client_socket, client.protocol, str, sizeof(str)))
				lprintf(LOG_ERR,"%04d !ERROR %s",client_socket, str);

			if(input_thread_mutex_locked && input_thread_running) {
				pthread_mutex_unlock(&input_thread_mutex);
				input_thread_mutex_locked=false;
			}
		}

		curatr=~0;			// Can't guarantee current attributes
		attr(LIGHTGRAY);	// Force to "normal"

		rio_abortable=rio_abortable_save;	// Restore abortable state

		/* Got back to Text/NVT mode */
		request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX);
	}

//	lprintf("%s returned %d",realcmdline, retval);

	errorlevel = retval; // Baja or JS retrievable error value

	return(retval);
}

#else	/* !WIN32 */

/*****************************************************************************/
// Expands Unix LF to CRLF
/*****************************************************************************/
BYTE* lf_expand(BYTE* inbuf, ulong inlen, BYTE* outbuf, ulong& newlen)
{
	ulong	i,j;

	for(i=j=0;i<inlen;i++) {
		if(inbuf[i]=='\n' && (!i || inbuf[i-1]!='\r'))
			outbuf[j++]='\r';
		outbuf[j++]=inbuf[i];
	}
	newlen=j;
    return(outbuf);
}

#define MAX_ARGS 1000

#ifdef NEEDS_SETENV
static int setenv(const char *name, const char *value, int overwrite)
{
	char *envstr;
	char *oldenv;
	if(overwrite || getenv(name)==NULL) {
		envstr=(char *)malloc(strlen(name)+strlen(value)+2);
		if(envstr==NULL) {
			errno=ENOMEM;
			return(-1);
		}
		/* Note, on some platforms, this can be free()d... */
		sprintf(envstr,"%s=%s",name,value);
		putenv(envstr);
	}
	return(0);
}
#endif

#ifdef NEEDS_CFMAKERAW
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
}
#endif

#ifdef NEEDS_FORKPTY
static int login_tty(int fd)
{
	(void) setsid();
	if (!isatty(fd))
		return (-1);
	(void) dup2(fd, 0);
	(void) dup2(fd, 1);
	(void) dup2(fd, 2);
	if (fd > 2)
		(void) close(fd);
	return (0);
}

#ifdef NEEDS_DAEMON
/****************************************************************************/
/* Daemonizes the process                                                   */
/****************************************************************************/
int
daemon(int nochdir, int noclose)
{
    int fd;

    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(0);
    }

    if (setsid() == -1)
        return (-1);

    if (!nochdir)
        (void)chdir("/");

    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > 2)
            (void)close(fd);
    }
    return (0);
}
#endif

static int openpty(int *amaster, int *aslave, char *name, struct termios *termp, winsize *winp)
{
	char line[] = "/dev/ptyXX";
	const char *cp1, *cp2;
	int master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrsPQRS"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdefghijklmnopqrstuv"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					break; /* try the next pty group */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				/* Hrm... SunOS doesn't seem to have revoke
				(void) revoke(line); */
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
							TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
							(char *)winp);
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

static int forkpty(int *amaster, char *name, termios *termp, winsize *winp)
{
	int master, slave, pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return (-1);
	switch (pid = FORK()) {
	case -1:
		return (-1);
	case 0:
		/*
		 * child
		 */
		(void) close(master);
		login_tty(slave);
		return (0);
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void) close(slave);
	return (pid);
}
#endif /* NEED_FORKPTY */

int sbbs_t::external(const char* cmdline, long mode, const char* startup_dir)
{
	char	str[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	fullpath[MAX_PATH+1];
	char	fullcmdline[MAX_PATH+1];
	char*	argv[MAX_ARGS];
	char*	p;
	BYTE*	bp;
	BYTE	buf[XTRN_IO_BUF_LEN];
    BYTE 	output_buf[XTRN_IO_BUF_LEN*2];
	ulong	avail;
    ulong	output_len;
	bool	native=false;			// DOS program by default
	bool	rio_abortable_save=rio_abortable;
	int		i;
	bool	data_waiting;
	int		rd;
	int		wr;
	int		argc;
	pid_t	pid;
	int		in_pipe[2];
	int		out_pipe[2];
#ifdef XTERN_LOG_STDERR
	int		err_pipe[2];
#endif
	fd_set ibits;
	int	high_fd;
	struct timeval timeout;

	if(online!=ON_REMOTE || cfg.node_num==0)
		eprintf(LOG_DEBUG,"Executing external: %s",cmdline);

	if(startup_dir!=NULL && startup_dir[0] && !isdir(startup_dir)) {
		errormsg(WHERE, ERR_CHK, startup_dir, 0);
		return -1;
	}

	if(startup_dir==NULL)
		startup_dir=nulstr;

	XTRN_LOADABLE_MODULE(cmdline,startup_dir);
	XTRN_LOADABLE_JS_MODULE(cmdline,startup_dir);

	attr(cfg.color[clr_external]);  /* setup default attributes */

	native = native_executable(&cfg, cmdline, mode);

    SAFECOPY(str,cmdline);			/* Set fname to program name only */
	truncstr(str," ");
    SAFECOPY(fname,getfname(str));

	sprintf(fullpath,"%s%s",startup_dir,fname);
	if(startup_dir!=NULL && cmdline[0]!='/' && cmdline[0]!='.' && fexist(fullpath))
		sprintf(fullcmdline,"%s%s",startup_dir,cmdline);
	else
		SAFECOPY(fullcmdline,cmdline);

 	if(native) { // Native (32-bit) external

		// Current environment passed to child process
		sprintf(dszlog,"%sPROTOCOL.LOG",cfg.node_dir);
		setenv("DSZLOG",dszlog,1); 		/* Makes the DSZ LOG active */
		setenv("SBBSNODE",cfg.node_dir,1);
		setenv("SBBSCTRL",cfg.ctrl_dir,1);
		setenv("SBBSDATA",cfg.data_dir,1);
		setenv("SBBSEXEC",cfg.exec_dir,1);
		sprintf(str,"%u",cfg.node_num);
		if(setenv("SBBSNNUM",str,1))
        	errormsg(WHERE,ERR_WRITE,"environment",0);

	} else {
		if(startup->options&BBS_OPT_NO_DOS) {
			bprintf("Sorry, DOS programs are not supported on this node.\r\n");
			return -1;
		}
#if defined(__FreeBSD__)
		/* ToDo: This seems to work for every door except Iron Ox
		   ToDo: Iron Ox is unique in that it runs perfectly from
		   ToDo: tcsh but not at all from anywhere else, complaining
		   ToDo: about corrupt files.  I've ruled out the possibilty
		   ToDo: of it being a terminal mode issue... no other ideas
		   ToDo: come to mind. */

		FILE * doscmdrc;

		sprintf(str,"%s.doscmdrc",cfg.node_dir);
		if((doscmdrc=fopen(str,"w+"))==NULL)  {
			errormsg(WHERE,ERR_CREATE,str,0);
			return(-1);
		}
		if(startup_dir!=NULL && startup_dir[0])
			fprintf(doscmdrc,"assign C: %s\n",startup_dir);
		else
			fprintf(doscmdrc,"assign C: .\n");

		fprintf(doscmdrc,"assign D: %s\n",cfg.node_dir);
		SAFECOPY(str,cfg.exec_dir);
		if((p=strrchr(str,'/'))!=NULL)
			*p=0;
		if((p=strrchr(str,'/'))!=NULL)
			*p=0;
		fprintf(doscmdrc,"assign E: %s\n",str);
		
		/* setup doscmd env here */
		/* ToDo Note, this assumes that the BBS uses standard dir names */
		fprintf(doscmdrc,"DSZLOG=E:\\node%d\\PROTOCOL.LOG\n",cfg.node_num);
		fprintf(doscmdrc,"SBBSNODE=D:\\\n");
		fprintf(doscmdrc,"SBBSCTRL=E:\\ctrl\\\n");
		fprintf(doscmdrc,"SBBSDATA=E:\\data\\\n");
		fprintf(doscmdrc,"SBBSEXEC=E:\\exec\\\n");
		fprintf(doscmdrc,"SBBSNNUM=%d\n",cfg.node_num);

		fclose(doscmdrc);
		SAFECOPY(str,fullcmdline);
		sprintf(fullcmdline,"%s -F %s",startup->dosemu_path,str);

#elif defined(__linux__) && defined(USE_DOSEMU)

		/* dosemu integration  --  Ryan Underwood, <nemesis @ icequake.net> */

		FILE *dosemubat;
		int setup_override;
		char tok[MAX_PATH+1];
 
		char dosemuconf[MAX_PATH+1];
		char dosemubinloc[MAX_PATH+1];
		char virtualconf[75];
		char dosterm[15];
		char log_external[MAX_PATH+1];

		/*  on the Unix side. xtrndir is the parent of the door's startup dir. */
		char xtrndir[MAX_PATH+1];

		/*  on the DOS side.  */
		char xtrndir_dos[MAX_PATH+1];
		char ctrldir_dos[MAX_PATH+1];
		char datadir_dos[MAX_PATH+1];
		char execdir_dos[MAX_PATH+1];

		/* Default locations that can be overridden by 
		 * the sysop in emusetup.bat */

		const char nodedrive[] = "D:";
		const char xtrndrive[] = "E:";
		const char ctrldrive[] = "F:";
		const char datadrive[] = "G:";
		const char execdrive[] = "H:";

		SAFECOPY(str,startup_dir);
		if(*(p=lastchar(str))=='/')		/* kill trailing slash */
			*p=0;
		if((p=strrchr(str,'/'))!=NULL)  /* kill the last element of the path */
			*p=0;

		SAFECOPY(xtrndir,str);

		/* construct DOS equivalents for the unix directories */

		SAFECOPY(ctrldir_dos,cfg.ctrl_dir);
		REPLACE_CHARS(ctrldir_dos,'/','\\',p);

		p=lastchar(ctrldir_dos);
		if (*p=='\\') *p=0;

		SAFECOPY(datadir_dos,cfg.data_dir);
		REPLACE_CHARS(datadir_dos,'/','\\',p);

		p=lastchar(datadir_dos);
		if (*p=='\\') *p=0;

		SAFECOPY(execdir_dos,cfg.exec_dir);
		REPLACE_CHARS(execdir_dos,'/','\\',p);

		p=lastchar(execdir_dos);
		if (*p=='\\') *p=0;

		SAFECOPY(xtrndir_dos,xtrndir);
		REPLACE_CHARS(xtrndir_dos,'/','\\',p);

		/* check for existence of a dosemu.conf in the door directory.
		 * It is a good idea to be able to use separate configs for each
		 * door. */

		sprintf(str,"%sdosemu.conf",startup_dir);
		if (!fexist(str)) {

		/* If we can't find it in the door dir, look for a global one
		 * in the ctrl dir. */

			sprintf(str,"%sdosemu.conf",cfg.ctrl_dir);
			if (!fexist(str)) {

		/* If we couldn't find either, try for the system one, then
		 * error out. */
				SAFECOPY(str,"/etc/dosemu/dosemu.conf");
				if (!fexist(str)) {
				
					SAFECOPY(str,"/etc/dosemu.conf");
					if (!fexist(str)) {
						errormsg(WHERE,ERR_READ,str,0);
						return(-1);
					}
					else SAFECOPY(dosemuconf,str);  /* using system conf */
				}
				else SAFECOPY(dosemuconf,str);  /* using system conf */
			}
			else SAFECOPY(dosemuconf,str);   /* using global conf */
		}
		else SAFECOPY(dosemuconf,str);  /* using door-specific conf */

		/* same deal for emusetup.bat. */

		sprintf(str,"%semusetup.bat",startup_dir);
		fprintf(stderr, str);
		if (!fexist(str)) {

		/* If we can't find it in the door dir, look for a global one
		 * in the ctrl dir. */

			sprintf(str,"%semusetup.bat",cfg.ctrl_dir);
			if (!fexist(str)) {

		/* If we couldn't find either, set an error condition. */
				setup_override = -1;
			}
			else setup_override = 0;  /* using global bat */
		}
		else setup_override = 1;  /* using door-specific bat */

		/* Create the external bat here to be placed in the node dir. */

		sprintf(str,"%sexternal.bat",cfg.node_dir);
		if(!(dosemubat=fopen(str,"w+"))) {
			errormsg(WHERE,ERR_CREATE,str,0);
			return(-1);
		}

		fprintf(dosemubat,"@echo off\r\n");
		fprintf(dosemubat,"set DSZLOG=%s\\PROTOCOL.LOG\r\n",nodedrive);
		fprintf(dosemubat,"set SBBSNODE=%s\r\n",nodedrive);
		fprintf(dosemubat,"set SBBSNNUM=%d\r\n",cfg.node_num);
		fprintf(dosemubat,"set SBBSCTRL=%s\r\n",ctrldrive);
		fprintf(dosemubat,"set SBBSDATA=%s\r\n",datadrive);
		fprintf(dosemubat,"set SBBSEXEC=%s\r\n",execdrive);

		/* clear existing redirections on dos side */
		fprintf(dosemubat,"lredir del %s\r\nlredir del %s\r\nlredir del %s\r\nlredir del %s\r\n",xtrndrive,ctrldrive,datadrive,execdrive);

		/* redirect necessary drive letters to unix paths */
		fprintf(dosemubat,"lredir %s linux\\fs%s\r\n",xtrndrive,xtrndir_dos);
		fprintf(dosemubat,"lredir %s linux\\fs%s\r\n",ctrldrive,ctrldir_dos);
		fprintf(dosemubat,"lredir %s linux\\fs%s\r\n",datadrive,datadir_dos);
		fprintf(dosemubat,"lredir %s linux\\fs%s\r\n",execdrive,execdir_dos);

		/* change to the drive where the parent of the startup_dir is mounted */
		fprintf(dosemubat,"%s\r\n",xtrndrive);

		if(startup_dir!=NULL && startup_dir[0]) {

			SAFECOPY(str,startup_dir);

		/* if theres a trailing slash, dump it */

			p=lastchar(str);
			if (*p=='/') *p=0;

			if ((p=strrchr(str, '/'))!=NULL)
				SAFECOPY(str,p+1);  /* str = game's starting dir */

			else str[0] = '\0';
		}

		else str[0] = '\0';

		fprintf(dosemubat,"cd %s\r\n",str);  /* startup_dir  */

		if (setup_override == 1)
			fprintf(dosemubat,"call %s\\%s\\emusetup.bat %s\r\n",xtrndrive,str,cmdline);
		else if (setup_override == 0)
			fprintf(dosemubat,"call %s\\emusetup.bat\r\n",ctrldrive);
		/* if (setup_override == -1) do_nothing */

		/*  Check if it's a bat file, to prepend "call" to the command  */

		SAFECOPY(tok,cmdline);
		truncstr(tok," ");

		p = strstr(tok, ".bat");  /*  check if it's a bat file  */
		if (p)
			fprintf(dosemubat,"call ");  /* if so, "call" it */

		fprintf(dosemubat,"%s\r\n",cmdline);
		fprintf(dosemubat,"exitemu\r\n");

		/* Check the "Stdio Interception" flag from scfg for this door.  If it's
		 * enabled, we enable doorway mode.  Else, it's vmodem for us, unless
		 * it's a timed event.
		 */

		if (!(mode&(EX_STDIO)) && online!=ON_LOCAL)
			SAFECOPY(virtualconf,"-I\"serial { virtual com 1 }\"");
		else
			virtualconf[0] = '\0';

		/* Set the interception bits, since we are always going to want Synchronet
		 * to intercept dos programs under Unix.
		 */

		mode |= EX_STDIO;

		/* See if we have the dosemu link in the door's dir.  If so, use the dosemu
		 * that it points to as our command to execute.  If not, use DOSemuPath.
		 */
 
		sprintf(str,"%sdosemu.bin",startup_dir);
		if (!fexist(str)) {
			SAFECOPY(dosemubinloc,(cmdstr(startup->dosemu_path,nulstr,nulstr,tok)));
		}
		else {
			SAFECOPY(dosemubinloc,str);
		}

		/* Attempt to keep dosemu from prompting for a disclaimer. */

		sprintf(str, "%s/.dosemu", cfg.ctrl_dir);
		if (!isdir(str)) {
			mkdir(str, 0755);
		}

		strcat(str, "/disclaimer");
		ftouch(str);

		/* Set up the command for dosemu to execute with 'unix -e'. */

		sprintf(str,"%sexternal.bat",nodedrive);

		/* need TERM=linux for maintenance programs to work
		 * (dosemu won't start with no controlling terminal)
		 * Also, redirect stdout to a log if it's a timed event.
		 */
		 
		if (online==ON_LOCAL) {
			SAFECOPY(dosterm,"TERM=linux");
			sprintf(log_external,">> %sdosevent_%s.log",cfg.logs_dir,fname);
		}
		else {
			dosterm[0]='\0';
			log_external[0] = '\0';
		}

		/* Drum roll. */

		sprintf(fullcmdline,
		"/usr/bin/env %s HOME=%s QUIET=1 DOSDRIVE_D=%s %s -I\"video { none }\" -I\"keystroke \\r\" %s -f%s -E%s -o%sdosemu.log 2> %sdosemu_boot.log %s",
			dosterm,cfg.ctrl_dir,cfg.node_dir,dosemubinloc,virtualconf,dosemuconf,str,cfg.node_dir,cfg.node_dir,log_external);

		fprintf(dosemubat,"REM For debugging: %s\r\n",fullcmdline);
		fclose(dosemubat);

#else
		bprintf("\r\nExternal DOS programs are not yet supported in \r\n%s\r\n"
			,VERSION_NOTICE);
		return(-1);
#endif
	}

	if(!(mode&EX_STDIN) && input_thread_running) {
		lprintf(LOG_DEBUG,"Locking input thread mutex"); 
		if(pthread_mutex_lock(&input_thread_mutex)!=0)
			errormsg(WHERE,ERR_LOCK,"input_thread_mutex",0);
		input_thread_mutex_locked=true;
	}

#ifdef XTERN_LOG_STDERR
	if(pipe(err_pipe)!=0) {
		errormsg(WHERE,ERR_CREATE,"err_pipe",0);
		return(-1);
	}
#endif

	if((mode&EX_STDIO)==EX_STDIO)  {
		struct winsize winsize;
		struct termios term;
		memset(&term,0,sizeof(term));
		cfsetispeed(&term,B19200);
		cfsetospeed(&term,B19200);
		if(mode&EX_BIN)
			cfmakeraw(&term);
		else {
			term.c_iflag = TTYDEF_IFLAG;
			term.c_oflag = TTYDEF_OFLAG;
			term.c_lflag = TTYDEF_LFLAG;
			term.c_cflag = TTYDEF_CFLAG;
			memcpy(term.c_cc,ttydefchars,sizeof(term.c_cc));
		}
		winsize.ws_row=rows;
		winsize.ws_col=cols;
		if((pid=forkpty(&in_pipe[1],NULL,&term,&winsize))==-1) {
			if(input_thread_mutex_locked && input_thread_running) {
				if(pthread_mutex_unlock(&input_thread_mutex)!=0)
					errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
				input_thread_mutex_locked=false;
			}
			errormsg(WHERE,ERR_EXEC,fullcmdline,0);
			return(-1);
		}
		out_pipe[0]=in_pipe[1];
	}
	else  {
		if(mode&EX_STDIN)
			if(pipe(in_pipe)!=0) {
				errormsg(WHERE,ERR_CREATE,"in_pipe",0);
				return(-1);
			}
		if(mode&EX_STDOUT)
			if(pipe(out_pipe)!=0) {
				errormsg(WHERE,ERR_CREATE,"out_pipe",0);
				return(-1);
			}


		if((pid=FORK())==-1) {
			if(input_thread_mutex_locked && input_thread_running) {
				if(pthread_mutex_unlock(&input_thread_mutex)!=0)
					errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
				input_thread_mutex_locked=false;
			}
			errormsg(WHERE,ERR_EXEC,fullcmdline,0);
			return(-1);
		}
	}
	if(pid==0) {	/* child process */
		/* Give away all privs for good now */
		if(startup->setuid!=NULL)
			startup->setuid(TRUE);

		sigset_t        sigs;
		sigfillset(&sigs);
		sigprocmask(SIG_UNBLOCK,&sigs,NULL);
		if(!(mode&EX_BIN))  {
			static char	term_env[256];
			if(term_supports(ANSI))
				sprintf(term_env,"TERM=%s",startup->xtrn_term_ansi);
			else
				sprintf(term_env,"TERM=%s",startup->xtrn_term_dumb);
			putenv(term_env);
		}
#ifdef __FreeBSD__
		if(!native)
			chdir(cfg.node_dir);
		else
#endif
		if(startup_dir!=NULL && startup_dir[0])
			if(chdir(startup_dir)!=0) {
				errormsg(WHERE,ERR_CHDIR,startup_dir,0);
				return(-1);
			}

		if(mode&EX_SH || strcspn(fullcmdline,"<>|;\"")!=strlen(fullcmdline)) {
			argv[0]=comspec;
			argv[1]=(char*)"-c";
			argv[2]=fullcmdline;
			argv[3]=NULL;
		} else {
			argv[0]=fullcmdline;	/* point to the beginning of the string */
			argc=1;
			for(i=0;fullcmdline[i] && argc<MAX_ARGS;i++)	/* Break up command line */
				if(fullcmdline[i]==' ') {
					fullcmdline[i]=0;			/* insert nulls */
					argv[argc++]=fullcmdline+i+1; /* point to the beginning of the next arg */
				}
			argv[argc]=NULL;
		}

		if(mode&EX_STDIN && !(mode&EX_STDOUT))  {
			close(in_pipe[1]);		/* close write-end of pipe */
			dup2(in_pipe[0],0);		/* redirect stdin */
			close(in_pipe[0]);		/* close excess file descriptor */
		}

		if(mode&EX_STDOUT && !(mode&EX_STDIN)) {
			close(out_pipe[0]);		/* close read-end of pipe */
			dup2(out_pipe[1],1);	/* stdout */
#ifndef XTERN_LOG_STDERR
			dup2(out_pipe[1],2);	/* stderr */
#endif
			close(out_pipe[1]);		/* close excess file descriptor */
		}

		if(!(mode & EX_STDIO)) {
			int fd;

			/* Redirect stdio to /dev/null */
			if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
				dup2(fd, STDIN_FILENO);
				dup2(fd, STDOUT_FILENO);
#ifndef XTERN_LOG_STDERR
				dup2(fd, STDERR_FILENO);
#endif
				if (fd > 2)
					close(fd);
			}
		}

		if(mode&EX_BG)	/* background execution, detach child */
		{
			lprintf(LOG_INFO,"Detaching external process");
			daemon(TRUE,FALSE);
   	    }

#ifdef XTERN_LOG_STDERR
		close(err_pipe[0]);		/* close read-end of pipe */
		dup2(err_pipe[1],2);	/* stderr */
#endif
	
		execvp(argv[0],argv);
		lprintf(LOG_ERR,"Node %d !ERROR %d executing %s",cfg.node_num,errno,argv[0]);
		_exit(-1);	/* should never get here */
	}

	if(online==ON_REMOTE)
		lprintf(LOG_INFO,"Node %d executing external: %s",cfg.node_num,fullcmdline);

	/* Disable Ctrl-C checking */
	if(!(mode&EX_OFFLINE))
		rio_abortable=false;
	
#ifdef XTERN_LOG_STDERR
	close(err_pipe[1]);	/* close write-end of pipe */
#endif

	if(mode&EX_STDOUT) {
		if(!(mode&EX_STDIN))
			close(out_pipe[1]);	/* close write-end of pipe */
		while(!terminated) {
			if(waitpid(pid, &i, WNOHANG)!=0)	/* child exited */
				break;

			if(mode&EX_CHKTIME)
				gettimeleft();
			
			if(!online && !(mode&EX_OFFLINE)) {
				sprintf(str,"%s hung-up in external program",useron.alias);
				logline(LOG_NOTICE,"X!",str);
				break;
			}

			/* Input */	
			if(mode&EX_STDIN && RingBufFull(&inbuf)) {
				if((wr=RingBufRead(&inbuf,buf,sizeof(buf)))!=0)
					write(in_pipe[1],buf,wr);
			}
				
			/* Error Output */
			FD_ZERO(&ibits);
#ifdef XTERN_LOG_STDERR
			FD_SET(err_pipe[0],&ibits);
			high_fd=err_pipe[0];
#endif
			FD_SET(out_pipe[0],&ibits);
#ifdef XTERN_LOG_STDERR
			if(out_pipe[0]>err_pipe[0])
				high_fd=out_pipe[0];
#else
			high_fd=out_pipe[0];
#endif
			timeout.tv_sec=0;
			timeout.tv_usec=1000;
			bp=buf;
			i=0;
#ifndef XTERN_LOG_STDERR
			select(high_fd+1,&ibits,NULL,NULL,&timeout);
#else
			while ((select(high_fd+1,&ibits,NULL,NULL,&timeout)>0) && FD_ISSET(err_pipe[0],&ibits) && (i<(int)sizeof(buf)-1))  {
				if((rd=read(err_pipe[0],bp,1))>0)  {
					i+=rd;
					bp++;
					if(*(bp-1)=='\n')
						break;
				}
				else
					break;
				FD_ZERO(&ibits);
				FD_SET(err_pipe[0],&ibits);
				FD_SET(out_pipe[0],&ibits);
				timeout.tv_sec=0;
				timeout.tv_usec=1000;
			}
			if(i)
				lprintf(LOG_NOTICE,"%.*s",i,buf);		/* lprintf mangles i? */

			/* Eat stderr if mode is EX_BIN */
			if(mode&EX_BIN)  {
				bp=buf;
				i=0;
			}
#endif

			data_waiting=FD_ISSET(out_pipe[0],&ibits);
			if(i==0 && data_waiting==0)
				continue;

			avail=(RingBufFree(&outbuf)-i)/2;	// Leave room for wwiv/telnet expansion
			if(avail==0) {
#if 0
				lprintf("Node %d !output buffer full (%u bytes)"
						,cfg.node_num,RingBufFull(&outbuf));
#endif
				YIELD();
				continue;
			}

			rd=avail;

			if(rd>((int)sizeof(buf)-i))
				rd=sizeof(buf)-i;

			if(data_waiting)  {
				rd=read(out_pipe[0],bp,rd);
				if(rd<1 && i==0)
					continue;
				if(rd<0)
					rd=0;
			}
			else
				rd=0;

			rd += i;

			if(mode&EX_BIN) {
				if(telnet_mode&TELNET_MODE_OFF) {
					bp=buf;
					output_len=rd;
				}
				else
   	       			bp=telnet_expand(buf, rd, output_buf, output_len);
			} else			/* LF to CRLF expansion */
				bp=lf_expand(buf, rd, output_buf, output_len);

			/* Did expansion overrun the output buffer? */
			if(output_len>sizeof(output_buf)) {
				lprintf(LOG_ERR,"OUTPUT_BUF OVERRUN");
				output_len=sizeof(output_buf);
			}

			/* Does expanded size fit in the ring buffer? */
			if(output_len>RingBufFree(&outbuf)) {
				lprintf(LOG_ERR,"output buffer overflow");
				output_len=RingBufFree(&outbuf);
			}

			RingBufWrite(&outbuf, bp, output_len);

		}

		if(waitpid(pid, &i, WNOHANG)==0)  {		// Child still running? 
			kill(pid, SIGHUP);					// Tell child user has hung up
			time_t start=time(NULL);			// Wait up to 10 seconds
			while(time(NULL)-start<10) {		// for child to terminate
				if(waitpid(pid, &i, WNOHANG)!=0)
					break;
				mswait(500);
			}
			if(waitpid(pid, &i, WNOHANG)==0)	// Child still running?
				kill(pid, SIGKILL);				// terminate child process
		}
		/* close unneeded descriptors */
		if(mode&EX_STDIN)
			close(in_pipe[1]);
		close(out_pipe[0]);
	}
#if 0
	else {
		/* Enable the Nagle algorithm */
		int nodelay=FALSE;
		setsockopt(client_socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
	}
#endif
#ifdef XTERN_LOG_STDERR
	while(waitpid(pid, &i, WNOHANG)==0)  {
		FD_ZERO(&ibits);
		FD_SET(err_pipe[0],&ibits);
		timeout.tv_sec=1;
		timeout.tv_usec=0;
		bp=buf;
		i=0;
		while ((select(err_pipe[0]+1,&ibits,NULL,NULL,&timeout)>0) && (i<XTRN_IO_BUF_LEN-1))  {
			if((rd=read(err_pipe[0],bp,1))>0)  {
				i+=rd;
				if(*bp=='\n') {
					lprintf(LOG_NOTICE,"%.*s",i-1,buf);
					i=0;
					bp=buf;
				}
				else
					bp++;
			}
			else
				break;
		}
		if(i)
			lprintf(LOG_NOTICE,"%.*s",i,buf);
	}
#else
	waitpid(pid, &i, 0);
#endif

	if(!(mode&EX_OFFLINE)) {	/* !off-line execution */

		/* Re-enable blocking (incase disabled by xtrn program) */
		ulong l=0;
		ioctlsocket(client_socket, FIONBIO, &l);

		/* Re-set socket options */
		if(set_socket_options(&cfg, client_socket, client.protocol, str, sizeof(str)))
			lprintf(LOG_ERR,"%04d !ERROR %s",client_socket, str);

		curatr=~0;			// Can't guarantee current attributes
		attr(LIGHTGRAY);	// Force to "normal"

		rio_abortable=rio_abortable_save;	// Restore abortable state

		/* Got back to Text/NVT mode */
		request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX);
	}

#ifdef XTERN_LOG_STDERR
	close(err_pipe[0]);
#endif

	if(input_thread_mutex_locked && input_thread_running) {
		if(pthread_mutex_unlock(&input_thread_mutex)!=0)
			errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
		input_thread_mutex_locked=false;
	}

	return(errorlevel = WEXITSTATUS(i));
}

#endif	/* !WIN32 */

const char* quoted_string(const char* str, char* buf, size_t maxlen)
{
	if(strchr(str,' ')==NULL)
		return(str);
	safe_snprintf(buf,maxlen,"\"%s\"",str);
	return(buf);
}

#define QUOTED_STRING(ch, str, buf, maxlen) \
	((isalpha(ch) && isupper(ch)) ? str : quoted_string(str,buf,maxlen))
	
/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char* sbbs_t::cmdstr(const char *instr, const char *fpath, const char *fspec, char *outstr)
{
	char	str[MAX_PATH+1],*cmd;
    int		i,j,len;

    if(outstr==NULL)
        cmd=cmdstr_output;
    else
        cmd=outstr;
    len=strlen(instr);
    for(i=j=0;i<len && j<(int)sizeof(cmdstr_output);i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
			char ch=instr[i];
			if(isalpha(ch))
				ch=toupper(ch);
            switch(ch) {
                case 'A':   /* User alias */
                    strcat(cmd,QUOTED_STRING(instr[i],useron.alias,str,sizeof(str)));
                    break;
                case 'B':   /* Baud (DTE) Rate */
                    strcat(cmd,ultoa(dte_rate,str,10));
                    break;
                case 'C':   /* Connect Description */
                    strcat(cmd,connection);
                    break;
                case 'D':   /* Connect (DCE) Rate */
                    strcat(cmd,ultoa((ulong)cur_rate,str,10));
                    break;
                case 'E':   /* Estimated Rate */
                    strcat(cmd,ultoa((ulong)cur_cps*10,str,10));
                    break;
                case 'F':   /* File path */
                    strcat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)));
                    break;
                case 'G':   /* Temp directory */
                    strcat(cmd,cfg.temp_dir);
                    break;
                case 'H':   /* Socket Handle */
                    strcat(cmd,ultoa(client_socket_dup,str,10));
                    break;
                case 'I':   /* IP address */
                    strcat(cmd,cid);
                    break;
                case 'J':
                    strcat(cmd,cfg.data_dir);
                    break;
                case 'K':
                    strcat(cmd,cfg.ctrl_dir);
                    break;
                case 'L':   /* Lines per message */
                    strcat(cmd,ultoa(cfg.level_linespermsg[useron.level],str,10));
                    break;
                case 'M':   /* Minutes (credits) for user */
                    strcat(cmd,ultoa(useron.min,str,10));
                    break;
                case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                    strcat(cmd,cfg.node_dir);
                    break;
                case 'O':   /* SysOp */
                    strcat(cmd,QUOTED_STRING(instr[i],cfg.sys_op,str,sizeof(str)));
                    break;
                case 'P':   /* Client protocol */
                    strcat(cmd,client.protocol);
                    break;
                case 'Q':   /* QWK ID */
                    strcat(cmd,cfg.sys_id);
                    break;
                case 'R':   /* Rows */
                    strcat(cmd,ultoa(rows,str,10));
                    break;
                case 'S':   /* File Spec (or Baja command str) */
                    strcat(cmd,fspec);
                    break;
                case 'T':   /* Time left in seconds */
                    gettimeleft();
                    strcat(cmd,ultoa(timeleft,str,10));
                    break;
                case 'U':   /* UART I/O Address (in hex) */
                    strcat(cmd,ultoa(cfg.com_base,str,16));
                    break;
                case 'V':   /* Synchronet Version */
                    sprintf(str,"%s%c",VERSION,REVISION);
					strcat(cmd,str);
                    break;
                case 'W':   /* Columns (width) */
                    strcat(cmd,ultoa(cols,str,10));
                    break;
                case 'X':
                    strcat(cmd,cfg.shell[useron.shell]->code);
                    break;
                case '&':   /* Address of msr */
                    break;
                case 'Y':
                    strcat(cmd,comspec);
                    break;
                case 'Z':
                    strcat(cmd,cfg.text_dir);
                    break;
				case '~':	/* DOS-compatible (8.3) filename */
#ifdef _WIN32
					char sfpath[MAX_PATH+1];
					SAFECOPY(sfpath,fpath);
					GetShortPathName(fpath,sfpath,sizeof(sfpath));
					strcat(cmd,sfpath);
#else
                    strcat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)));
#endif			
					break;
                case '!':   /* EXEC Directory */
                    strcat(cmd,cfg.exec_dir);
                    break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strcat(cmd,cfg.exec_dir);
#endif
                    break;

                case '#':   /* Node number (same as SBBSNNUM environment var) */
                    sprintf(str,"%d",cfg.node_num);
                    strcat(cmd,str);
                    break;
                case '*':
                    sprintf(str,"%03d",cfg.node_num);
                    strcat(cmd,str);
                    break;
                case '$':   /* Credits */
                    strcat(cmd,ultoa(useron.cdt+useron.freecdt,str,10));
                    break;
                case '%':   /* %% for percent sign */
                    strcat(cmd,"%");
                    break;
				case '.':	/* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strcat(cmd,".exe");
#endif
					break;
				case '?':	/* Platform */
#ifdef __OS2__
					strcpy(str,"OS2");
#else
					strcpy(str,PLATFORM_DESC);
#endif
					strlwr(str);
					strcat(cmd,str);
					break;
                default:    /* unknown specification */
                    if(isdigit(instr[i])) {
                        sprintf(str,"%0*d",instr[i]&0xf,useron.number);
                        strcat(cmd,str); }
                    break; }
            j=strlen(cmd); }
        else
            cmd[j++]=instr[i]; }
    cmd[j]=0;

    return(cmd);
}

/****************************************************************************/
/* Returns command line generated from instr with %c replacments            */
/* This is the C-exported version											*/
/****************************************************************************/
extern "C" 
char* DLLCALL cmdstr(scfg_t* cfg, user_t* user, const char* instr, const char* fpath
						,const char* fspec, char* cmd)
{
	char	str[MAX_PATH+1];
    int		i,j,len;

    len=strlen(instr);
    for(i=j=0;i<len && j<MAX_PATH;i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
			char ch=instr[i];
			if(isalpha(ch))
				ch=toupper(ch);
            switch(ch) {
                case 'A':   /* User alias */
					if(user!=NULL)
						strcat(cmd,QUOTED_STRING(instr[i],user->alias,str,sizeof(str)));
                    break;
                case 'B':   /* Baud (DTE) Rate */
                    break;
                case 'C':   /* Connect Description */
					if(user!=NULL)
						strcat(cmd,user->modem);
                    break;
                case 'D':   /* Connect (DCE) Rate */
                    break;
                case 'E':   /* Estimated Rate */
                    break;
                case 'F':   /* File path */
                    strcat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)));
                    break;
                case 'G':   /* Temp directory */
                    strcat(cmd,cfg->temp_dir);
                    break;
                case 'H':   /* Port Handle or Hardware Flow Control */
                    break;
                case 'I':   /* IP address */
					if(user!=NULL)
						strcat(cmd,user->note);
                    break;
                case 'J':
                    strcat(cmd,cfg->data_dir);
                    break;
                case 'K':
                    strcat(cmd,cfg->ctrl_dir);
                    break;
                case 'L':   /* Lines per message */
					if(user!=NULL)
						strcat(cmd,ultoa(cfg->level_linespermsg[user->level],str,10));
                    break;
                case 'M':   /* Minutes (credits) for user */
					if(user!=NULL)
						strcat(cmd,ultoa(user->min,str,10));
                    break;
                case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                    strcat(cmd,cfg->node_dir);
                    break;
                case 'O':   /* SysOp */
                    strcat(cmd,QUOTED_STRING(instr[i],cfg->sys_op,str,sizeof(str)));
                    break;
                case 'P':   /* Client protocol */
                    break;
                case 'Q':   /* QWK ID */
                    strcat(cmd,cfg->sys_id);
                    break;
                case 'R':   /* Rows */
					if(user!=NULL)
						strcat(cmd,ultoa(user->rows,str,10));
                    break;
                case 'S':   /* File Spec */
                    strcat(cmd,fspec);
                    break;
                case 'T':   /* Time left in seconds */
                    break;
                case 'U':   /* UART I/O Address (in hex) */
                    strcat(cmd,ultoa(cfg->com_base,str,16));
                    break;
                case 'V':   /* Synchronet Version */
                    sprintf(str,"%s%c",VERSION,REVISION);
					strcat(cmd,str);
                    break;
                case 'W':   /* Columns/width */
                    break;
                case 'X':
					if(user!=NULL)
						strcat(cmd,cfg->shell[user->shell]->code);
                    break;
                case '&':   /* Address of msr */
                    break;
                case 'Y':
                    break;
                case 'Z':
                    strcat(cmd,cfg->text_dir);
                    break;
				case '~':	/* DOS-compatible (8.3) filename */
#ifdef _WIN32
					char sfpath[MAX_PATH+1];
					SAFECOPY(sfpath,fpath);
					GetShortPathName(fpath,sfpath,sizeof(sfpath));
					strcat(cmd,sfpath);
#else
                    strcat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)));
#endif			
					break;
                case '!':   /* EXEC Directory */
                    strcat(cmd,cfg->exec_dir);
                    break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strcat(cmd,cfg->exec_dir);
#endif
                    break;

                case '#':   /* Node number (same as SBBSNNUM environment var) */
                    sprintf(str,"%d",cfg->node_num);
                    strcat(cmd,str);
                    break;
                case '*':
                    sprintf(str,"%03d",cfg->node_num);
                    strcat(cmd,str);
                    break;
                case '$':   /* Credits */
					if(user!=NULL)
						strcat(cmd,ultoa(user->cdt+user->freecdt,str,10));
                    break;
                case '%':   /* %% for percent sign */
                    strcat(cmd,"%");
                    break;
				case '.':	/* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strcat(cmd,".exe");
#endif
					break;
				case '?':	/* Platform */
#ifdef __OS2__
					strcpy(str,"OS2");
#else
					strcpy(str,PLATFORM_DESC);
#endif
					strlwr(str);
					strcat(cmd,str);
					break;
                default:    /* unknown specification */
                    if(isdigit(instr[i]) && user!=NULL) {
                        sprintf(str,"%0*d",instr[i]&0xf,user->number);
                        strcat(cmd,str); 
					}
                    break; 
			}
            j=strlen(cmd); 
		}
        else
            cmd[j++]=instr[i]; 
	}
    cmd[j]=0;

    return(cmd);
}

