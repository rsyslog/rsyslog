/* Runs a test suite on the rsyslog (and later potentially
 * other things).
 *
 * The name of the test suite must be given as argv[1]. In this config,
 * rsyslogd is loaded with config ./testsuites/<name>.conf and then
 * test cases ./testsuites/ *.<name> are executed on it. This test driver is
 * suitable for testing cases where a message sent (via UDP) results in
 * exactly one response. It can not be used in cases where no response
 * is expected (that would result in a hang of the test driver).
 * Note: each test suite can contain many tests, but they all need to work
 * with the same rsyslog configuration.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <signal.h>
#include <netinet/in.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#define EXIT_FAILURE 1
#define INVALID_SOCKET -1
/* Name of input file, must match $IncludeConfig in test suite .conf files */
#define NETTEST_INPUT_CONF_FILE "nettest.input.conf" /* name of input file, must match $IncludeConfig in .conf files */

typedef enum { inputUDP, inputTCP } inputMode_t;
inputMode_t inputMode = inputTCP; /* input for which tests are to be run */
static pid_t rsyslogdPid = 0;	/* pid of rsyslog instance being tested */
static char *srcdir;	/* global $srcdir, set so that we can run outside of "make check" */
static char *testSuite = NULL; /* name of current test suite */
static int iPort = 12514; /* port which shall be used for sending data */
static char* pszCustomConf = NULL;	/* custom config file, use -c conf to specify */
static int verbose = 0;	/* verbose output? -v option */
static int IPv4Only = 0;	/* use only IPv4 in rsyslogd call? */
static char **ourEnvp;

/* these two are quick hacks... */
int iFailed = 0;
int iTests = 0;

/* provide user-friednly name of input mode
 */
static char *inputMode2Str(inputMode_t mode)
{
	char *pszMode;

	if(mode == inputUDP)
		pszMode = "udp";
	else
		pszMode = "tcp";

	return pszMode;
}


void readLine(int fd, char *ln)
{
	char *orig = ln;
	char c;
	int lenRead;

	if(verbose)
		fprintf(stderr, "begin readLine\n");
	lenRead = read(fd, &c, 1);

	while(lenRead == 1 && c != '\n') {
		if(c == '\0') {
			*ln = c;
			fprintf(stderr, "Warning: there was a '\\0'-Byte in the read response "
					"right after this string: '%s'\n", orig);
			c = '?';
		}
		*ln++ = c;
		lenRead = read(fd, &c, 1);
	}
	*ln = '\0';

	if(lenRead < 0) {
		printf("read from rsyslogd returned with error '%s' - aborting test\n", strerror(errno));
		exit(1);
	}

	if(verbose)
		fprintf(stderr, "end readLine, val read '%s'\n", orig);
}


/* send a message via TCP
 * We open the connection on the initial send, and never close it
 * (let the OS do that). If a conneciton breaks, we do NOT try to
 * recover, so all test after that one will fail (and the test
 * driver probably hang. returns 0 if ok, something else otherwise.
 * We use traditional framing '\n' at EOR for this tester. It may be
 * worth considering additional framing modes.
 * rgerhards, 2009-04-08
 * Note: we re-create the socket within the retry loop, because this
 * seems to be needed under Solaris. If we do not do that, we run
 * into troubles (maybe something wrongly initialized then?)
 * -- rgerhards, 2010-04-12
 */
int
tcpSend(char *buf, int lenBuf)
{
	static int sock = INVALID_SOCKET;
	struct sockaddr_in addr;
	int retries;
	int ret;
	int iRet = 0; /* 0 OK, anything else error */

	if(sock == INVALID_SOCKET) {
		/* first time, need to connect to target */
		retries = 0;
		while(1) { /* loop broken inside */
			/* first time, need to connect to target */
			if((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
				perror("socket()");
				iRet = 1;
				goto finalize_it;
			}
			memset((char *) &addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(iPort);
			if(inet_aton("127.0.0.1", &addr.sin_addr)==0) {
				fprintf(stderr, "inet_aton() failed\n");
				iRet = 1;
				goto finalize_it;
			}
			if((ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr))) == 0) {
				break;
			} else {
				if(retries++ == 50) {
					fprintf(stderr, "connect() failed\n");
					iRet = 1;
					goto finalize_it;
				} else {
					usleep(100000); /* ms = 1000 us! */
				}
			}
		} 
	}

	/* send test data */
	if((ret = send(sock, buf, lenBuf, 0)) != lenBuf) {
		perror("send test data");
		fprintf(stderr, "send() failed, sock=%d, ret=%d\n", sock, ret);
		iRet = 1;
		goto finalize_it;
	}

	/* send record terminator */
	if(send(sock, "\n", 1, 0) != 1) {
		perror("send record terminator");
		fprintf(stderr, "send() failed\n");
		iRet = 1;
		goto finalize_it;
	}

finalize_it:
	if(iRet != 0) {
		/* need to do some (common) cleanup */
		if(sock != INVALID_SOCKET) {
			close(sock);
			sock = INVALID_SOCKET;
		}
		++iFailed;
	}

	return iRet;
}


/* send a message via UDP
 * returns 0 if ok, something else otherwise.
 */
int
udpSend(char *buf, int lenBuf)
{
	struct sockaddr_in si_other;
	int s, slen=sizeof(si_other);

	if((s=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
		perror("socket()");
		return(1);
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(iPort);
	if(inet_aton("127.0.0.1", &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		return(1);
	}

	if(sendto(s, buf, lenBuf, 0, (struct sockaddr*) &si_other, slen)==-1) {
		perror("sendto");
		fprintf(stderr, "sendto() failed\n");
		return(1);
	}

	close(s);
	return 0;
}


/* open pipe to test candidate - so far, this is
 * always rsyslogd and with a fixed config. Later, we may
 * change this. Returns 0 if ok, something else otherwise.
 * rgerhards, 2009-03-31
 */
int openPipe(char *configFile, pid_t *pid, int *pfd)
{
	int pipefd[2];
	pid_t cpid;
	char *newargv[] = {"../tools/rsyslogd", "dummy", "-u2", "-n", "-irsyslog.pid",
			   "-M../runtime/.libs:../.libs", NULL, NULL};
	char confFile[1024];

	sprintf(confFile, "-f%s/testsuites/%s.conf", srcdir,
		(pszCustomConf == NULL) ? configFile : pszCustomConf);
	newargv[1] = confFile;

	if(IPv4Only)
		newargv[(sizeof(newargv)/sizeof(char*)) - 2] = "-4";

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	cpid = fork();
	if (cpid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if(cpid == 0) {    /* Child reads from pipe */
		fclose(stdout);
		dup(pipefd[1]);
		close(pipefd[1]);
		close(pipefd[0]);
		fclose(stdin);
		execve("../tools/rsyslogd", newargv, ourEnvp);
	} else {            
		usleep(10000);
		close(pipefd[1]);
		*pid = cpid;
		*pfd = pipefd[0];
	}

	return(0);
}


/* This function unescapes a string of testdata. That it, escape sequences
 * are converted into their one-character equivalent. While doing so, it applies
 * C-like semantics. This was made necessary for easy integration of control
 * characters inside test cases.  -- rgerhards, 2009-03-11
 * Currently supported:
 * \\	single backslash
 * \n, \t, \r as in C
 * \nnn where nnn is a 1 to 3 character octal sequence 
 * Note that when a problem occurs, the end result is undefined. After all, this
 * is for a testsuite generatort, it must not be 100% bullet proof (so do not
 * copy this code into something that must be!). Also note that we do in-memory
 * unescaping and assume that the string gets shorter but NEVER longer!
 */
void unescapeTestdata(char *testdata)
{
	char *pDst;
	char *pSrc;
	int i;
	int c;

	pDst = pSrc = testdata;
	while(*pSrc) {
		if(*pSrc == '\\') {
			switch(*++pSrc) {
			case '\\':	*pDst++ = *pSrc++;
					break;
			case 'n':	*pDst++ = '\n';
					++pSrc;
					break;
			case 'r':	*pDst++ = '\r';
					++pSrc;
					break;
			case 't':	*pDst++ = '\t';
					++pSrc;
					break;
			case '0':
			case '1':
			case '2':
			case '3':	c = *pSrc++ - '0';
					i = 1; /* we already processed one digit! */
					while(i < 3 && isdigit(*pSrc)) {
						c = c * 8 + *pSrc++ - '0';
						++i;
					}
					*pDst++ = c;
					break;
			default:	break;
			}
		} else {
			*pDst++ = *pSrc++;
		}
	}
	*pDst = '\0';
}


/* Process a specific test case. File name is provided.
 * Needs to return 0 if all is OK, something else otherwise.
 */
int
processTestFile(int fd, char *pszFileName)
{
	FILE *fp;
	char *testdata = NULL;
	char *expected = NULL;
	int ret = 0;
	size_t lenLn;
	char buf[4096];

	if((fp = fopen((char*)pszFileName, "r")) == NULL) {
		perror((char*)pszFileName);
		return(2);
	}

	/* skip comments at start of file */

	while(!feof(fp)) {
		getline(&testdata, &lenLn, fp);
		while(!feof(fp)) {
			if(*testdata == '#')
				getline(&testdata, &lenLn, fp);
			else
				break; /* first non-comment */
		}

		/* this is not perfect, but works ;) */
		if(feof(fp))
			break;

		++iTests; /* increment test count, we now do one! */

		testdata[strlen(testdata)-1] = '\0'; /* remove \n */
		/* now we have the test data to send (we could use function pointers here...) */
		unescapeTestdata(testdata);
		if(inputMode == inputUDP) {
			if(udpSend(testdata, strlen(testdata)) != 0)
				return(2);
		} else {
			if(tcpSend(testdata, strlen(testdata)) != 0)
				return(2);
		}

		/* next line is expected output 
		 * we do not care about EOF here, this will lead to a failure and thus
		 * draw enough attention. -- rgerhards, 2009-03-31
		 */
		getline(&expected, &lenLn, fp);
		expected[strlen(expected)-1] = '\0'; /* remove \n */

		/* pull response from server and then check if it meets our expectation */
//printf("try pull pipe...\n");
		readLine(fd, buf);
		if(strlen(buf) == 0) {
			printf("something went wrong - read a zero-length string from rsyslogd\n");
			exit(1);
		}
		if(strcmp(expected, buf)) {
			++iFailed;
			printf("\nFile %s:\nExpected Response:\n'%s'\nActual Response:\n'%s'\n",
				pszFileName, expected, buf);
				ret = 1;
		}

		/* we need to free buffers, as we have potentially modified them! */
		free(testdata);
		testdata = NULL;
		free(expected);
		expected = NULL;
	}

	fclose(fp);
	return(ret);
}


/* carry out all tests. Tests are specified via a file name
 * wildcard. Each of the files is read and the test carried
 * out.
 * Returns the number of tests that failed. Zero means all
 * success.
 */
int
doTests(int fd, char *files)
{
	int ret;
	char *testFile;
	glob_t testFiles;
	size_t i = 0;
	struct stat fileInfo;

	glob(files, GLOB_MARK, NULL, &testFiles);

	for(i = 0; i < testFiles.gl_pathc; i++) {
		testFile = testFiles.gl_pathv[i];

		if(stat((char*) testFile, &fileInfo) != 0) 
			continue; /* continue with the next file if we can't stat() the file */

		/* all regular files are run through the test logic. Symlinks don't work. */
		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			if(verbose) printf("processing test case '%s' ... ", testFile);
			ret = processTestFile(fd, testFile);
			if(ret == 0) {
				if(verbose) printf("successfully completed\n");
			} else {
				if(!verbose)
					printf("test '%s' ", testFile);
				printf("failed!\n");
			}
		}
	}
	globfree(&testFiles);

	if(iTests == 0) {
		printf("Error: no test cases found, no tests executed.\n");
		iFailed = 1;
	} else {
		printf("Number of tests run: %3d, number of failures: %d, test: %s/%s\n",
		       iTests, iFailed, testSuite, inputMode2Str(inputMode));
	}

	return(iFailed);
}


/* indicate that our child has died (where it is not permitted to!).
 */
void childDied(__attribute__((unused)) int sig)
{
	printf("ERROR: child died unexpectedly (maybe a segfault?)!\n");
	exit(1);
}


/* cleanup */
void doAtExit(void)
{
	int status;

	/* disarm died-child handler */
	signal(SIGCHLD, SIG_IGN);

	if(rsyslogdPid != 0) {
		kill(rsyslogdPid, SIGTERM);
		waitpid(rsyslogdPid, &status, 0);	/* wait until instance terminates */
	}

	unlink(NETTEST_INPUT_CONF_FILE);
}

/* Run the test suite. This must be called with exactly one parameter, the
 * name of the test suite. For details, see file header comment at the top
 * of this file.
 * rgerhards, 2009-04-03
 */
int main(int argc, char *argv[], char *envp[])
{
	int fd;
	int opt;
	int ret = 0;
	FILE *fp;
	char buf[4096];
	char testcases[4096];

	ourEnvp = envp;
	while((opt = getopt(argc, argv, "4c:i:p:t:v")) != EOF) {
		switch((char)opt) {
                case '4':
			IPv4Only = 1;
			break;
                case 'c':
			pszCustomConf = optarg;
			break;
                case 'i':
			if(!strcmp(optarg, "udp"))
				inputMode = inputUDP;
			else if(!strcmp(optarg, "tcp"))
				inputMode = inputTCP;
			else {
				printf("error: unsupported input mode '%s'\n", optarg);
				exit(1);
			}
			break;
                case 'p':
			iPort = atoi(optarg);
			break;
                case 't':
			testSuite = optarg;
			break;
                case 'v':
			verbose = 1;
			break;
		default:printf("Invalid call of nettester, invalid option '%c'.\n", opt);
			printf("Usage: nettester -d -ttestsuite-name -iudp|tcp [-pport] [-ccustomConfFile] \n");
			exit(1);
		}
	}
	
	if(testSuite == NULL) {
		printf("error: no testsuite given, need to specify -t testsuite!\n");
		exit(1);
	}

	atexit(doAtExit);

	if((srcdir = getenv("srcdir")) == NULL)
		srcdir = ".";

	if(verbose) printf("Start of nettester run ($srcdir=%s, testsuite=%s, input=%s/%d)\n",
		srcdir, testSuite, inputMode2Str(inputMode), iPort);

	/* create input config file */
	if((fp = fopen(NETTEST_INPUT_CONF_FILE, "w")) == NULL) {
		perror(NETTEST_INPUT_CONF_FILE);
		printf("error opening input configuration file\n");
		exit(1);
	}
	if(inputMode == inputUDP) {
		fputs("$ModLoad ../plugins/imudp/.libs/imudp\n", fp);
		fprintf(fp, "$UDPServerRun %d\n", iPort);
	} else {
		fputs("$ModLoad ../plugins/imtcp/.libs/imtcp\n", fp);
		fprintf(fp, "$InputTCPServerRun %d\n", iPort);
	}
	fclose(fp);

	/* arm died-child handler */
	signal(SIGCHLD, childDied);

	/* make sure we do not abort if there is an issue with pipes.
	 * our code does the necessary error handling.
	 */
	sigset(SIGPIPE, SIG_IGN);

	/* start to be tested rsyslogd */
	openPipe(testSuite, &rsyslogdPid, &fd);
	readLine(fd, buf);

	/* generate filename */
	sprintf(testcases, "%s/testsuites/*.%s", srcdir, testSuite);
	if(doTests(fd, testcases) != 0)
		ret = 1;

	if(verbose) printf("End of nettester run (%d).\n", ret);

	exit(ret);
}
