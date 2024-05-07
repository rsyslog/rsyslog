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

static void
errout(char *reason)
{
	perror(reason);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: minitcpsrvr -t ip-addr -p port -P portFile -f outfile\n");
	exit (1);
}

int
main(int argc, char *argv[])
{
	int fdc;
	int fdf = -1;
	struct sockaddr_in srvAddr;
	struct sockaddr_in cliAddr;
	unsigned int srvAddrLen;
	unsigned int cliAddrLen;
	char wrkBuf[4096];
	ssize_t nRead;
	int nRcv = 0;
	int dropConnection_NbrRcv = 0;
	int opt;
	int sleeptime = 0;
	char *targetIP = NULL;
	int targetPort = -1;
	char *portFileName = NULL;
	size_t totalWritten = 0;
	int listen_fd, conn_fd, fd, file_fd, nfds, port = 8080;
	struct sockaddr_in server_addr;
	struct pollfd fds[MAX_CONNECTIONS + 1]; // +1 for the listen socket
	char buffer[MAX_CONNECTIONS][BUFFER_SIZE];
	int buffer_offs[MAX_CONNECTIONS];
	memset(fds, 0, sizeof(fds));
	memset(buffer_offs, 0, sizeof(buffer_offs));

	while((opt = getopt(argc, argv, "D:t:p:P:f:s:")) != -1) {
		switch (opt) {
		case 's':
			sleeptime = atoi(optarg);
			break;
		case 'D':
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

	if(sleeptime) {
		printf("minitcpsrv: deliberate sleep of %d seconds\n", sleeptime);
		sleep(sleeptime);
		printf("minitcpsrv: end sleep\n");
	}

	// Create listen socket
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		errout("Failed to create listen socket");
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(targetIP); //htonl(INADDR_ANY);
	server_addr.sin_port = htons(targetPort);
	srvAddrLen = sizeof(server_addr);

	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		errout("bind");
	}

	if (listen(listen_fd, MAX_CONNECTIONS) < 0) {
		errout("Listen failed");
	}

	if(portFileName != NULL) {
		FILE *fp;
		if (getsockname(listen_fd, (struct sockaddr*)&srvAddr, &srvAddrLen) == -1) {
			errout("getsockname");
		}
		if((fp = fopen(portFileName, "w+")) == NULL) {
			errout(portFileName);
		}
		fprintf(fp, "%d", ntohs(srvAddr.sin_port));
		fclose(fp);
	}

	fds[0].fd = listen_fd;
	fds[0].events = POLLIN;

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
				fprintf(stderr, "NEW CONNECT\n");
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
				nRcv++;
				fd = fds[i].fd;
				const size_t bytes_to_read = sizeof(buffer[i]) - buffer_offs[i] - 1;
				int read_bytes = read(fd, &(buffer[i][buffer_offs[i]]), bytes_to_read);
				if (read_bytes < 0) {
					perror("Read error");
					close(fd);
					fds[i].fd = -1; // Remove from poll set
				} else if (read_bytes == 0) {
						// Connection closed
						close(fd);
						fds[i].fd = -1; // Remove from poll set
					} else {
						const int last_byte = read_bytes + buffer_offs[i] - 1; // last valid char in rcv buffer
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
								//fprintf(stderr, "mmemmove: unfinished bytes %d, bytes_to_write %d\n", unfinished_bytes, bytes_to_write);
								memmove(buffer[i], &(buffer[i][bytes_to_write]),
									unfinished_bytes);
								buffer_offs[i] = unfinished_bytes;
							}
						}
					}

					/* simulate connection abort, if requested */
					if(dropConnection_NbrRcv > 0 && nRcv > dropConnection_NbrRcv) {
						fprintf(stderr, "## MINITCPSRVR: imulating connection abort after %d receive "
							"calls, bytes written so far %zu\n", (int) nRcv, totalWritten);
						nRcv = 0;
						close(fd);
						fds[i].fd = -1; // Remove from poll set
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
