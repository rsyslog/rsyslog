#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

static void
errout(char *reason)
{
	perror(reason);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int fds;
	int fdc;
	int fdf;
	struct sockaddr_in srvAddr;
	struct sockaddr_in cliAddr;
	unsigned int srvAddrLen; 
	unsigned int cliAddrLen;
	char wrkBuf[4096];
	ssize_t nRead;

	if(argc != 4) {
		fprintf(stderr, "usage: minitcpsrvr ip-addr port outfile\n");
		exit(1);
	}

	if(!strcmp(argv[3], "-")) {
		fdf = 1;
	} else {
		fdf = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
		if(fdf == -1) errout(argv[3]);
	}

	fds = socket(AF_INET, SOCK_STREAM, 0);
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr(argv[1]);
	srvAddr.sin_port = htons(atoi(argv[2]));
	srvAddrLen = sizeof(srvAddr);
	if(bind(fds, (struct sockaddr *)&srvAddr, srvAddrLen) != 0)
		errout("bind");
	if(listen(fds, 20) != 0) errout("listen");
	cliAddrLen = sizeof(cliAddr);

	fdc = accept(fds, (struct sockaddr *)&cliAddr, &cliAddrLen);
	while(1) {       
		nRead = read(fdc, wrkBuf, sizeof(wrkBuf));
		if(nRead == 0) break;
		if(write(fdf, wrkBuf, nRead) != nRead)
			errout("write");
	}
	/* let the OS do the cleanup */
	return 0;
}
