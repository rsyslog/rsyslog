/* klog for linux, based on the FreeBSD syslogd implementation.
 *
 * This contains OS-specific functionality to read the BSD
 * kernel log. For a general overview, see head comment in
 * imklog.c.
 *
 * This file heavily borrows from the klogd daemon provided by
 * the sysklogd project. Many thanks for this piece of software.
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
#include "cfsysline.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "imklog.h"
#include "unicode-helper.h"


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


static uchar *GetPath(void)
{
	return pszPath ? pszPath : UCHAR_CONSTANT(_PATH_KLOG);
}

static void CloseLogSrc(void)
{
	/* Turn on logging of messages to console, but only if a log level was speficied */
	if(console_log_level != -1)
		ksyslog(7, NULL, 0);
  
        /* Shutdown the log sources. */
	switch(logsrc) {
	    case kernel:
		ksyslog(0, NULL, 0);
		imklogLogIntMsg(LOG_INFO, "Kernel logging (ksyslog) stopped.");
		break;
            case proc:
		close(kmsg);
		imklogLogIntMsg(LOG_INFO, "Kernel logging (proc) stopped.");
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
		imklogLogIntMsg(LOG_WARNING, "Cannot set console log level - disabling "
		       "console output.");
	}

	/*
	 * First do a stat to determine whether or not the proc based
	 * file system is available to get kernel messages from.
	 */
	if ( use_syscall ||
	    ((stat((char*)GetPath(), &sb) < 0) && (errno == ENOENT)) )
	{
	  	/* Initialize kernel logging. */
	  	ksyslog(1, NULL, 0);
		imklogLogIntMsg(LOG_INFO, "imklog %s, log source = ksyslog "
		       "started.", VERSION);
		return(kernel);
	}

	if ( (kmsg = open((char*)GetPath(), O_RDONLY|O_CLOEXEC)) < 0 )
	{
		imklogLogIntMsg(LOG_ERR, "imklog: Cannot open proc file system, %d.\n", errno);
		ksyslog(7, NULL, 0);
		return(none);
	}

	imklogLogIntMsg(LOG_INFO, "imklog %s, log source = %s started.", VERSION, GetPath());
	return(proc);
}


/*     Copy characters from ptr to line until a char in the delim
 *     string is encountered or until min( space, len ) chars have
 *     been copied.
 *
 *     Returns the actual number of chars copied.
 */
static int copyin( uchar *line,      int space,
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

    static uchar line_buff[LOG_LINE_LENGTH];

    static uchar *line =line_buff;
    static enum parse_state_enum parse_state = PARSING_TEXT;
    static int space = sizeof(line_buff)-1;

    static uchar *sym_start;            /* points at the '<' of a symbol */

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

	    //dbgprintf("Line buffer full:\n");
       	    //dbgprintf("\tLine: %s\n", line);

            Syslog(LOG_INFO, line_buff);
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
               delta = copyin(line, space, ptr, len, "\n[" );
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
	          Syslog(LOG_INFO, line_buff);
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
               value  = strtoul((char*)(sym_start+1), (char **) 0, 16);
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

	       // TODO: sprintf!!!!
               delta = sprintf( (char*) sym_start, "%s+%d/%d]",
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
		if(errno == EINTR)
			return;
		imklogLogIntMsg(LOG_ERR, "imklog Error return from sys_sycall: %d\n", errno);
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
	if ( (rdcnt = read(kmsg, log_buffer, sizeof(log_buffer)-1)) < 0 ) {
		if ( errno == EINTR )
			return;
		imklogLogIntMsg(LOG_ERR, "Cannot read proc file system: %d - %s.", errno, strerror(errno));
	} else {
		LogLine(log_buffer, rdcnt);
        }

	return;
}


/* to be called in the module's WillRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogLogKMsg(void)
{
        DEFiRet;
        switch(logsrc) {
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
	RETiRet;
}


/* to be called in the module's WillRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogWillRun(void)
{
        DEFiRet;
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
				imklogLogIntMsg(LOG_WARNING, "cannot find any symbols, turning off symbol lookups");
			}
		}
	}

        RETiRet;
}


/* to be called in the module's AfterRun entry point
 * rgerhards, 2008-04-09
 */
rsRetVal klogAfterRun(void)
{
        DEFiRet;
	/* cleanup here */
	if(logsrc != none)
		CloseLogSrc();

	DeinitKsyms();
	DeinitMsyms();

        RETiRet;
}


/* provide the (system-specific) default facility for internal messages
 * rgerhards, 2008-04-14
 */
int
klogFacilIntMsg(void)
{
	return LOG_KERN;
}


/* vi:set ai:
 */
