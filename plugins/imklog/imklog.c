/* The kernel log input module for Linux. This file heavily
 * borrows from the klogd daemon provided by the sysklogd project.
 * Many thanks for this piece of software.
 *
 * Please note that this file replaces the klogd daemon that was
 * also present in pre-v3 versions of rsyslog.
 *
 * I have begun to convert this to an input module on 2007-12-17.
 * IMPORTANT: more than a single instance is currently not supported. This
 * needs to be revisited once the config file and input module interface
 * supports multiple instances!
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
*/
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include "syslogd.h"
#include "cfsysline.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "imklog.h"

MODULE_TYPE_INPUT

/* Module static data */
DEF_IMOD_STATIC_DATA

/* configuration settings TODO: move to instance data? */
int dbgPrintSymbols = 0; /* this one is extern so the helpers can access it! */
static int symbols_twice = 0;
static int use_syscall = 0;
static int symbol_lookup = 1;
/* TODO: configuration for the following directives must be implemented. It 
 * was not done yet because we either do not yet have a config handler for
 * that type or I thought it was acceptable to push it to a later stage when
 * I gained more handson experience with the input module interface (and the
 * changes resulting from that). -- rgerhards, 2007-12-20
 */
static char *symfile = NULL; 
static int console_log_level = -1;


/* Includes. */
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#if HAVE_TIME_H
#	include <time.h>
#endif

#include <stdarg.h>
#include <paths.h>
#include "ksyms.h"

#define __LIBRARY__
#include <unistd.h>


#if !defined(__GLIBC__)
# define __NR_ksyslog __NR_syslog
_syscall3(int,ksyslog,int, type, char *, buf, int, len);
#else
#include <sys/klog.h>
#define ksyslog klogctl
#endif



#ifndef _PATH_KLOG
#define _PATH_KLOG  "/proc/kmsg"
#endif

#define LOG_BUFFER_SIZE 4096
#define LOG_LINE_LENGTH 1000

static int	kmsg;
static char	log_buffer[LOG_BUFFER_SIZE];

static enum LOGSRC {none, proc, kernel} logsrc;



/* Function prototypes. */
extern int ksyslog(int type, char *buf, int len);


/* Write a message to the message queue.
 * returns -1 if it fails, something else otherwise
 */
static rsRetVal writeSyslogV(int iPRI, const char *szFmt, va_list va)
{
	DEFiRet;
	int iChars;
	int iLen;
	time_t tNow;
	char msgBuf[2048]; /* we use the same size as sysklogd to remain compatible */

	assert(szFmt != NULL);

	/* build the message */
	time(&tNow);
	/* we can use sprintf safely below, because we know the size of the constants.
	 * By doing so, we save some cpu cycles and code complexity (for unnecessary
	 * error checking).
	 */
	iLen = sprintf(msgBuf, "<%d>%.15s kernel: ", iPRI, ctime(&tNow) + 4);

	iChars = vsnprintf(msgBuf + iLen, sizeof(msgBuf) / sizeof(char) - iLen, szFmt, va);

	/* here we must create our message object and supply it to the message queue
	 */
	CHKiRet(parseAndSubmitMessage(LocalHostName, msgBuf, strlen(msgBuf), MSG_DONT_PARSE_HOSTNAME, NOFLAG, eFLOWCTL_LIGHT_DELAY));

finalize_it:
	RETiRet;
}

/* And now the same with variable arguments */
static int writeSyslog(int iPRI, const char *szFmt, ...)
{
	int iRet;
	va_list va;

	assert(szFmt != NULL);
	va_start(va, szFmt);
	iRet = writeSyslogV(iPRI, szFmt, va);
	va_end(va);

	return(iRet);
}

rsRetVal Syslog(int priority, char *fmt, ...) __attribute__((format(printf,2, 3)));
rsRetVal Syslog(int priority, char *fmt, ...)
{
	DEFiRet;
	va_list ap;
	char *argl;

	/* Output using syslog. */
	if (!strcmp(fmt, "%s")) {
		va_start(ap, fmt);
		argl = va_arg(ap, char *);
		if (argl[0] == '<' && argl[1] && argl[2] == '>') {
			switch ( argl[1] )
			{
			case '0':
				priority = LOG_EMERG;
				break;
			case '1':
				priority = LOG_ALERT;
				break;
			case '2':
				priority = LOG_CRIT;
				break;
			case '3':
				priority = LOG_ERR;
				break;
			case '4':
				priority = LOG_WARNING;
				break;
			case '5':
				priority = LOG_NOTICE;
				break;
			case '6':
				priority = LOG_INFO;
				break;
			case '7':
			default:
				priority = LOG_DEBUG;
			}
			argl += 3;
		}
		iRet = writeSyslog(priority, fmt, argl);
		va_end(ap);
	} else {
		va_start(ap, fmt);
		iRet = writeSyslogV(priority, fmt, ap);
		va_end(ap);
	}

	RETiRet;
}


static void CloseLogSrc(void)
{
	/* Turn on logging of messages to console, but only if we had the -c
	 * option -- rgerhards, 2007-08-01
	 */
	if (console_log_level != -1)
		ksyslog(7, NULL, 0);
  
        /* Shutdown the log sources. */
	switch ( logsrc )
	{
	    case kernel:
		ksyslog(0, 0, 0);
		Syslog(LOG_INFO, "Kernel logging (ksyslog) stopped.");
		break;
            case proc:
		close(kmsg);
		Syslog(LOG_INFO, "Kernel logging (proc) stopped.");
		break;
	    case none:
		break;
	}

	return;
}


static enum LOGSRC GetKernelLogSrc(void)
{
	auto struct stat sb;

	/* Set level of kernel console messaging.. */
	if (   (console_log_level != -1) &&
	       (ksyslog(8, NULL, console_log_level) < 0) &&
	     (errno == EINVAL) )
	{
		/*
		 * An invalid arguement error probably indicates that
		 * a pre-0.14 kernel is being run.  At this point we
		 * issue an error message and simply shut-off console
		 * logging completely.
		 */
		Syslog(LOG_WARNING, "Cannot set console log level - disabling "
		       "console output.");
	}

	/*
	 * First do a stat to determine whether or not the proc based
	 * file system is available to get kernel messages from.
	 */
	if ( use_syscall ||
	    ((stat(_PATH_KLOG, &sb) < 0) && (errno == ENOENT)) )
	{
	  	/* Initialize kernel logging. */
	  	ksyslog(1, NULL, 0);
		Syslog(LOG_INFO, "imklogd %s, log source = ksyslog "
		       "started.", VERSION);
		return(kernel);
	}

	if ( (kmsg = open(_PATH_KLOG, O_RDONLY)) < 0 )
	{
		char sz[512];
		snprintf(sz, sizeof(sz), "imklog: Cannot open proc file system, %d - %s.\n", errno, strerror(errno));
		logmsgInternal(LOG_SYSLOG|LOG_ERR, sz, ADDDATE);
		ksyslog(7, NULL, 0); /* TODO: check this, implement more */
		return(none);
	}

	Syslog(LOG_INFO, "imklog %s, log source = %s started.", \
	       VERSION, _PATH_KLOG);
	return(proc);
}


/*     Copy characters from ptr to line until a char in the delim
 *     string is encountered or until min( space, len ) chars have
 *     been copied.
 *
 *     Returns the actual number of chars copied.
 */
static int copyin( char *line,      int space,
                   const char *ptr, int len,
                   const char *delim )
{
    auto int i;
    auto int count;

    count = len < space ? len : space;

    for(i=0; i<count && !strchr(delim, *ptr); i++ ) {
	*line++ = *ptr++;
    }

    return(i);
}

/*
 * Messages are separated by "\n".  Messages longer than
 * LOG_LINE_LENGTH are broken up.
 *
 * Kernel symbols show up in the input buffer as : "[<aaaaaa>]",
 * where "aaaaaa" is the address.  These are replaced with
 * "[symbolname+offset/size]" in the output line - symbolname,
 * offset, and size come from the kernel symbol table.
 *
 * If a kernel symbol happens to fall at the end of a message close
 * in length to LOG_LINE_LENGTH, the symbol will not be expanded.
 * (This should never happen, since the kernel should never generate
 * messages that long.
 *
 * To preserve the original addresses, lines containing kernel symbols
 * are output twice.  Once with the symbols converted and again with the
 * original text.  Just in case somebody wants to run their own Oops
 * analysis on the syslog, e.g. ksymoops.
 */
static void LogLine(char *ptr, int len)
{
    enum parse_state_enum {
        PARSING_TEXT,
        PARSING_SYMSTART,      /* at < */
        PARSING_SYMBOL,        
        PARSING_SYMEND         /* at ] */
    };

    static char line_buff[LOG_LINE_LENGTH];

    static char *line                        =line_buff;
    static enum parse_state_enum parse_state = PARSING_TEXT;
    static int space                         = sizeof(line_buff)-1;

    static char *sym_start;            /* points at the '<' of a symbol */

    auto   int delta = 0;              /* number of chars copied        */
    auto   int symbols_expanded = 0;   /* 1 if symbols were expanded */
    auto   int skip_symbol_lookup = 0; /* skip symbol lookup on this pass */
    auto   char *save_ptr = ptr;       /* save start of input line */
    auto   int save_len = len;         /* save length at start of input line */

    while( len > 0 )
    {
        if( space == 0 )    /* line buffer is full */
        {
            /*
            ** Line too long.  Start a new line.
            */
            *line = 0;   /* force null terminator */

	    dbgprintf("Line buffer full:\n");
       	    dbgprintf("\tLine: %s\n", line);

            Syslog( LOG_INFO, "%s", line_buff );
            line  = line_buff;
            space = sizeof(line_buff)-1;
            parse_state = PARSING_TEXT;
	    symbols_expanded = 0;
	    skip_symbol_lookup = 0;
	    save_ptr = ptr;
	    save_len = len;
        }

        switch( parse_state )
        {
        case PARSING_TEXT:
               delta = copyin( line, space, ptr, len, "\n[" );
               line  += delta;
               ptr   += delta;
               space -= delta;
               len   -= delta;

               if( space == 0 || len == 0 )
               {
		  break;  /* full line_buff or end of input buffer */
               }

               if( *ptr == '\0' )  /* zero byte */
               {
                  ptr++;	/* skip zero byte */
                  space -= 1;
                  len   -= 1;

		  break;
	       }

               if( *ptr == '\n' )  /* newline */
               {
                  ptr++;	/* skip newline */
                  space -= 1;
                  len   -= 1;

                  *line = 0;  /* force null terminator */
	          Syslog( LOG_INFO, "%s", line_buff );
                  line  = line_buff;
                  space = sizeof(line_buff)-1;
		  if (symbols_twice) {
		      if (symbols_expanded) {
			  /* reprint this line without symbol lookup */
			  symbols_expanded = 0;
			  skip_symbol_lookup = 1;
			  ptr = save_ptr;
			  len = save_len;
		      }
		      else
		      {
			  skip_symbol_lookup = 0;
			  save_ptr = ptr;
			  save_len = len;
		      }
		  }
                  break;
               }
               if( *ptr == '[' )   /* possible kernel symbol */
               {
                  *line++ = *ptr++;
                  space -= 1;
                  len   -= 1;
	          if (!skip_symbol_lookup)
                     parse_state = PARSING_SYMSTART;      /* at < */
                  break;
               }
               /* Now that line_buff is no longer fed to *printf as format
                * string, '%'s are no longer "dangerous".
		*/
               break;
        
        case PARSING_SYMSTART:
               if( *ptr != '<' )
               {
                  parse_state = PARSING_TEXT;        /* not a symbol */
                  break;
               }

               /*
               ** Save this character for now.  If this turns out to
               ** be a valid symbol, this char will be replaced later.
               ** If not, we'll just leave it there.
               */

               sym_start = line; /* this will point at the '<' */

               *line++ = *ptr++;
               space -= 1;
               len   -= 1;
               parse_state = PARSING_SYMBOL;     /* symbol... */
               break;

        case PARSING_SYMBOL:
               delta = copyin( line, space, ptr, len, ">\n[" );
               line  += delta;
               ptr   += delta;
               space -= delta;
               len   -= delta;
               if( space == 0 || len == 0 )
               {
                  break;  /* full line_buff or end of input buffer */
               }
               if( *ptr != '>' )
               {
                  parse_state = PARSING_TEXT;
                  break;
               }

               *line++ = *ptr++;  /* copy the '>' */
               space -= 1;
               len   -= 1;

               parse_state = PARSING_SYMEND;

               break;

        case PARSING_SYMEND:
               if( *ptr != ']' )
               {
                  parse_state = PARSING_TEXT;        /* not a symbol */
                  break;
               }

               /*
               ** It's really a symbol!  Replace address with the
               ** symbol text.
               */
           {
	       auto int sym_space;

	       unsigned long value;
	       auto struct symbol sym;
	       auto char *symbol;

               *(line-1) = 0;    /* null terminate the address string */
               value  = strtoul(sym_start+1, (char **) 0, 16);
               *(line-1) = '>';  /* put back delim */

               if ( !symbol_lookup || (symbol = LookupSymbol(value, &sym)) == (char *)0 )
               {
                  parse_state = PARSING_TEXT;
                  break;
               }

               /*
               ** verify there is room in the line buffer
               */
               sym_space = space + ( line - sym_start );
               if( (unsigned) sym_space < strlen(symbol) + 30 ) /*(30 should be overkill)*/
               {
                  parse_state = PARSING_TEXT;  /* not enough space */
                  break;
               }

               delta = sprintf( sym_start, "%s+%d/%d]",
                                symbol, sym.offset, sym.size );

               space = sym_space + delta;
               line  = sym_start + delta;
	       symbols_expanded = 1;
           }
               ptr++;
               len--;
               parse_state = PARSING_TEXT;
               break;

        default: /* Can't get here! */
               parse_state = PARSING_TEXT;

        }
    }

    return;
}


static void LogKernelLine(void)
{
	auto int rdcnt;

	/*
	 * Zero-fill the log buffer.  This should cure a multitude of
	 * problems with klogd logging the tail end of the message buffer
	 * which will contain old messages.  Then read the kernel log
	 * messages into this fresh buffer.
	 */
	memset(log_buffer, '\0', sizeof(log_buffer));
	if ( (rdcnt = ksyslog(2, log_buffer, sizeof(log_buffer)-1)) < 0 )
	{
		char sz[512];
		if(errno == EINTR)
			return;
		snprintf(sz, sizeof(sz), "imklog: Error return from sys_sycall: %d - %s\n", errno, strerror(errno));
		logmsgInternal(LOG_SYSLOG|LOG_ERR, sz, ADDDATE);
	}
	else
		LogLine(log_buffer, rdcnt);
	return;
}


static void LogProcLine(void)
{
	auto int rdcnt;

	/*
	 * Zero-fill the log buffer.  This should cure a multitude of
	 * problems with klogd logging the tail end of the message buffer
	 * which will contain old messages.  Then read the kernel messages
	 * from the message pseudo-file into this fresh buffer.
	 */
	memset(log_buffer, '\0', sizeof(log_buffer));
	if ( (rdcnt = read(kmsg, log_buffer, sizeof(log_buffer)-1)) < 0 )
	{
		if ( errno == EINTR )
			return;
		Syslog(LOG_ERR, "Cannot read proc file system: %d - %s.", errno, strerror(errno));
	}
	else
		LogLine(log_buffer, rdcnt);

	return;
}


BEGINrunInput
CODESTARTrunInput
	/* this is an endless loop - it is terminated when the thread is
	 * signalled to do so. This, however, is handled by the framework,
	 * right into the sleep below.
	 */
	while(!pThrd->bShallStop) {
		/* we do not need to handle the RS_RET_TERMINATE_NOW case any
	   	 * special because we just need to terminate. This may be different
	   	 * if a cleanup is needed. But for now, we can just use CHKiRet().
	   	 * rgerhards, 2007-12-17
	   	 */
		switch ( logsrc )
		{
			case kernel:
	  			LogKernelLine();
				break;
			case proc:
				LogProcLine();
				break;
		        case none:
				/* TODO: We need to handle this case here somewhat more intelligent 
	  			 * This is now at least partly done - code should never reach this point
	  			 * as willRun() already checked for the "none" status -- rgerhards, 2007-12-17
	  			 */
				pause();
				break;
		}
	}
	RETiRet;
ENDrunInput


BEGINwillRun
	/* Initialize this module. If that fails, we tell the engine we don't like to run */
	/* Determine where kernel logging information is to come from. */
	logsrc = GetKernelLogSrc();
	if(logsrc == none) {
		iRet = RS_RET_NO_KERNEL_LOGSRC;
	} else {
		if (symbol_lookup) {
			symbol_lookup  = (InitKsyms(symfile) == 1);
			symbol_lookup |= InitMsyms();
			if (symbol_lookup == 0) {
				Syslog(LOG_WARNING, "cannot find any symbols, turning off symbol lookups\n");
			}
		}
	}
CODESTARTwillRun
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* cleanup here */
	if(logsrc != none)
		CloseLogSrc();

	DeinitKsyms();
	DeinitMsyms();
ENDafterRun


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	dbgPrintSymbols = 0;
	symbols_twice = 0;
	use_syscall = 0;
	symfile = NULL;
	symbol_lookup = 1;
	return RS_RET_OK;
}

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"debugprintkernelsymbols", 0, eCmdHdlrBinary, NULL, &dbgPrintSymbols, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbollookup", 0, eCmdHdlrBinary, NULL, &symbol_lookup, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogsymbolstwice", 0, eCmdHdlrBinary, NULL, &symbols_twice, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"klogusesyscallinterface", 0, eCmdHdlrBinary, NULL, &use_syscall, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
