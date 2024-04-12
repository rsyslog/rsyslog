/* a very simplistic tcp receiver for the rsyslog testbench.
 *
 * Copyright 2016,2024 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#if defined(__FreeBSD__)
#include <netinet/in.h>
#endif

#define MAX_CONNECTIONS 10
#define BUFFER_SIZE 1024

/* OK, we use a lot of global. But after all, this is "just" a small
 * testing ... that has evolved a bit ;-)
 */
char wrkBuf[4096];
ssize_t nRead;
size_t nRcv = 0;
int nConnDrop; /* how often has the connection already been dropped? */
int abortListener = 0; /* act like listener was totally aborted */
int sleepAfterConnDrop = 0; /* number of seconds to sleep() when a connection was dropped */
int dropConnection_NbrRcv = 0;
int dropConnection_MaxTimes = 0;
int opt;
int sleepStartup = 0;
char *targetIP = NULL;
int targetPort = -1;
char *portFileName = NULL;
size_t totalWritten = 0;
int listen_fd, conn_fd, fd, file_fd, nfds, port = 8080;
struct sockaddr_in server_addr;
struct pollfd fds[MAX_CONNECTIONS + 1]; // +1 for the listen socket
char buffer[MAX_CONNECTIONS][BUFFER_SIZE];
int buffer_offs[MAX_CONNECTIONS];

static void
errout(char *reason)
{
	perror(reason);
	fprintf(stderr, "minitcpsrv ABORTS!!!\n\n\n");
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: minitcpsrv -t ip-addr -p port -P portFile -f outfile\n");
	exit (1);
}

static void
createListenSocket(void)
{
	struct sockaddr_in srvAddr;
	unsigned int srvAddrLen;
	static int portFileWritten = 0;
	int r;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		errout("Failed to create listen socket");
	}
	// Set SO_REUSEADDR and SO_REUSEPORT options
	int opt = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
		errout("setsockopt failed");
	}

	fprintf(stderr, "listen on target port %d\n", targetPort);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(targetIP); //htonl(INADDR_ANY);
	server_addr.sin_port = htons(targetPort);
	srvAddrLen = sizeof(server_addr);


	int sockBound = 0;
	int try = 0;
	do {
		r = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if(r < 0) {
			if(errno == EADDRINUSE) {
				perror("minitcpsrv bind listen socket");
				if(try++ < 10) {
					sleep(1);
				} else {
					fprintf(stderr, "minitcpsrv could not bind socket "
						"after trying hard - terminating\n\n");
					exit(2);
				}
			} else {
				errout("bind");
			}
		} else {
			sockBound = 1;
		}
	} while(!sockBound);

	if (listen(listen_fd, MAX_CONNECTIONS) < 0) {
		errout("Listen failed");
	}

	if (getsockname(listen_fd, (struct sockaddr*)&srvAddr, &srvAddrLen) == -1) {
		errout("getsockname");
	}
	targetPort = ntohs(srvAddr.sin_port);

	if(portFileName != NULL && !portFileWritten) {
		FILE *fp;
		if (getsockname(listen_fd, (struct sockaddr*)&srvAddr, &srvAddrLen) == -1) {
			errout("getsockname");
		}
		if((fp = fopen(portFileName, "w+")) == NULL) {
			errout(portFileName);
		}
		fprintf(fp, "%d", ntohs(srvAddr.sin_port));
		fclose(fp);
		portFileWritten= 1;
	}

	fds[0].fd = listen_fd;
	fds[0].events = POLLIN;
}


int
main(int argc, char *argv[])
{
	int fdc;
	int fdf = -1;
	struct sockaddr_in cliAddr;
	unsigned int cliAddrLen;
	memset(fds, 0, sizeof(fds));
	memset(buffer_offs, 0, sizeof(buffer_offs));

	while((opt = getopt(argc, argv, "aB:D:t:p:P:f:s:S:")) != -1) {
		switch (opt) {
		case 'a': // abort listener: act like the server has died (shutdown and re-open listen socket)
			abortListener = 1;
			break;
		case 'S': // sleep time after connection drop
			sleepAfterConnDrop = atoi(optarg);
			break;
		case 's': // sleep time immediately after startup
			sleepStartup = atoi(optarg);
			break;
		case 'B': // max number of time the connection shall be dropped
			dropConnection_MaxTimes = atoi(optarg);
			break;
		case 'D': // drop connection after this number of recv() operations (not messages)
			dropConnection_NbrRcv = atoi(optarg);
			break;
		case 't':
			targetIP = optarg;
			break;
		case 'p':
			targetPort = atoi(optarg);
			break;
		case 'P':
			portFileName = optarg;
			break;
		case 'f':
			if(!strcmp(optarg, "-")) {
				fdf = 1;
			} else {
				fprintf(stderr, "writing to file %s\n", optarg);
				fdf = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
				if(fdf == -1) errout(argv[3]);
			}
			break;
		default:
			fprintf(stderr, "invalid option '%c' or value missing - terminating...\n", opt);
			usage();
			break;
		}
	}

	if(targetIP == NULL) {
		fprintf(stderr, "-t parameter missing -- terminating\n");
		usage();
	}
	if(targetPort == -1) {
		fprintf(stderr, "-p parameter missing -- terminating\n");
		usage();
	}
	if(fdf == -1) {
		fprintf(stderr, "-f parameter missing -- terminating\n");
		usage();
	}

	if(sleepStartup) {
		printf("minitcpsrv: deliberate sleep of %d seconds\n", sleepStartup);
		sleep(sleepStartup);
		printf("minitcpsrv: end sleep\n");
	}


	createListenSocket();
	nfds = 1;

	int bKeepRunning;
	while (1) {
		int poll_count = poll(fds, nfds, -1); // -1 means no timeout
		if (poll_count < 0) {
			errout("poll");
		}

		bKeepRunning = 0;	/* terminate on last connection close */

		for (int i = 0; i < nfds; i++) {
			if (fds[i].revents == 0) continue;

			if (fds[i].revents != POLLIN) {
				fprintf(stderr, "Error! revents = %d\n", fds[i].revents);
				exit(EXIT_FAILURE);
			}

			if (fds[i].fd == listen_fd) {
				// Accept new connection
				fprintf(stderr, "minitcpsrv: NEW CONNECT\n");
				conn_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL);
				if (conn_fd < 0) {
					perror("Accept failed");
					continue;
				}

				fds[nfds].fd = conn_fd;
				fds[nfds].events = POLLIN;
				nfds++;
			} else {
				// Handle data from a client
				fd = fds[i].fd;
				const size_t bytes_to_read = sizeof(buffer[i]) - buffer_offs[i] - 1;
				int read_bytes = read(fd, &(buffer[i][buffer_offs[i]]), bytes_to_read);
				nRcv += read_bytes;
				if (read_bytes < 0) {
					perror("Read error");
					close(fd);
					fds[i].fd = -1; // Remove from poll set
				} else if (read_bytes == 0) {
						// Connection closed
						close(fd);
						fds[i].fd = -1; // Remove from poll set
					} else {
						// last valid char in rcv buffer
						const int last_byte = read_bytes + buffer_offs[i] - 1;
						int last_lf = last_byte;
						while(last_lf > -1 && buffer[i][last_lf] != '\n') {
							--last_lf;
						}
						if(last_lf == -1) { /* no LF found at all */
							buffer_offs[i] = last_byte;
						} else {
							const int bytes_to_write = last_lf + 1;

							const int written_bytes = write(fdf, buffer[i], bytes_to_write);
							if(written_bytes != bytes_to_write)
								errout("write");
							totalWritten += bytes_to_write;
							if(bytes_to_write == last_byte + 1) {
								buffer_offs[i] = 0;
							} else {
								/* keep data in buffer, move it to start
								 * and adjust offset.
								 */
								int unfinished_bytes = last_byte - bytes_to_write + 1;
								memmove(buffer[i], &(buffer[i][bytes_to_write]),
									unfinished_bytes);
								buffer_offs[i] = unfinished_bytes;
							}
						}
					}

					/* simulate connection abort, if requested */
					if((buffer_offs[i] == 0)
					   && (dropConnection_NbrRcv > 0) && (nRcv >= dropConnection_NbrRcv)
					   && (dropConnection_MaxTimes > 0) && (nConnDrop < dropConnection_MaxTimes)) {
						nConnDrop++;
						nRcv = 0;
						if(abortListener) {
							fprintf(stderr, "minitcpsrv: simulating died client\n");
							shutdown(listen_fd, SHUT_RDWR);
						}
						close(fd);
						fds[i].fd = -1; // Remove from poll set

						if(sleepAfterConnDrop > 0) {
							sleep(sleepAfterConnDrop);
						}
						if(abortListener) {
							/* we hope that when we close and immediately re-open, we will
							 * can use the some port again.
							 */
							close(listen_fd);
							createListenSocket();
							fprintf(stderr, "minitcpsrv: reactivated\n");
						}
						bKeepRunning = 1; /* do not abort if sole connection! */
					}
				}
			}

		// Compact the array of monitored file descriptors
		for (int i = 0; i < nfds; i++) {
			if (fds[i].fd == -1) {
				for (int j = i; j < nfds - 1; j++) {
				fds[j] = fds[j + 1];
				}
			i--;
			nfds--;
			}
		}
		// terminate if all connections have been closed
		if(nfds == 1 && !bKeepRunning)
			break;
	}
/* -------------------------------------------------- */
	/* let the OS do the cleanup */
	fprintf(stderr, "minitcpsrv on port %d terminates itself, %zu bytes written\n", targetPort, totalWritten);
	return 0;
}
