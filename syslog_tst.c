/* Program to test daemon logging. */

/*
 * Sat Dec 11 12:07:50 CST 1993:  Dr. Wettstein
 *	Compiles clean with -Wall.  Renamed for first public distribution.
 *	Use this freely but if you make a ton of money with it I
 *	expect a cut...  :-)
 *
 * Thu Jan  6 11:52:10 CST 1994:  Dr. Wettstein
 *	Added support for reading getting log input from the standard
 *	input.  To activate this use a - as the single arguement to the
 *	the program.  Note that there is a hack in the code to pause
 *	after each 1K has been written.  This eliminates what appears
 *	to be a problem with overrunning a UNIX domain socket with
 *	excessive amounts of input.
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/param.h>

extern int main(int, char **);


int main(int argc, char *argv[])
{
	auto char *nl,
	          bufr[512];
	auto int logged = 0;
	
	openlog("DOTEST", LOG_PID, LOG_DAEMON);
	if (argc > 1)
	{
		if ( (*argv[1] == '-') && (*(argv[1]+1) == '\0') )
		{
			while (!feof(stdin))
				if ( fgets(bufr, sizeof(bufr), stdin) != \
				    (char *) 0 )
				{
					if ( (nl = strrchr(bufr, '\n')) != \
					    (char *) 0)
						*nl = '\0';
					syslog(LOG_INFO, bufr);
					logged += strlen(bufr);
					if ( logged > 1024 )
					{
						sleep(1);
						logged = 0;
					}
					
				}
		}
		else
			while (argc-- > 1)
				syslog(LOG_INFO, argv++[1]);
	}
	else
	{
		syslog(LOG_EMERG, "EMERG log.");
		syslog(LOG_ALERT, "Alert log.");
		syslog(LOG_CRIT, "Critical log.");
		syslog(LOG_ERR, "Error log.");
		syslog(LOG_WARNING, "Warning log.");
		syslog(LOG_NOTICE, "Notice log.");
		syslog(LOG_INFO, "Info log.");
		syslog(LOG_DEBUG, "Debug log.");
		closelog();
		return(0);
	}

	return(0);
}
