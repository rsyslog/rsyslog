/* This tool deliberately logs a message with the a trailing LF */
#include <stdio.h>
#include <syslog.h>

int main()
{
	syslog(LOG_NOTICE, "test\n");
	return 0;
}
