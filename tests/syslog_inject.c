/* This tool deliberately logs a message with the a trailing LF */
/* Options:
 * -l: inject linefeed at end of message
 * -c: inject control character in middle of message
 */
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

static inline void usage(void) {
	fprintf(stderr, "Usage: syslog_inject [-l] [-c]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	if(argc != 2)
		usage();
	if(!strcmp(argv[1], "-l"))
		syslog(LOG_NOTICE, "test\n");
	else if(!strcmp(argv[1], "-c"))
		syslog(LOG_NOTICE, "test 1\t2");
	else
		usage();

	return 0;
}
