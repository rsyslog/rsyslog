#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	int i;
	int sev = 0;
	if(argc != 2) {
		fprintf(stderr, "usage: syslog_caller num-messages\n");
		exit(1);
	}

	int msgs = atoi(argv[1]);

	for(i = 0 ; i < msgs ; ++i) {
		syslog(sev % 8, "test message nbr %d, severity=%d", i, sev % 8);
		sev++;
	}
}
