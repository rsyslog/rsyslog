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
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <netinet/in.h>

#define EXIT_FAILURE 1

static char *srcdir;	/* global $srcdir, set so that we can run outside of "make check" */
static char *testSuite; /* name of current test suite */


void readLine(int fd, char *ln)
{
	char c;
	int lenRead;
	lenRead = read(fd, &c, 1);
	while(lenRead == 1 && c != '\n') {
		*ln++ = c;
		 lenRead = read(fd, &c, 1);
	}
	*ln = '\0';
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
	si_other.sin_port = htons(12514);
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
	char *newargv[] = {"../tools/rsyslogd", "dummy", "-c4", "-u2", "-n", "-irsyslog.pid",
			   "-M../runtime//.libs", NULL };
	char confFile[1024];
	char *newenviron[] = { NULL };


	sprintf(confFile, "-f%s/testsuites/%s.conf", srcdir, configFile);
	newargv[1] = confFile;

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
		execve("../tools/rsyslogd", newargv, newenviron);
	} else {            
		close(pipefd[1]);
		*pid = cpid;
		*pfd = pipefd[0];
	}

	return(0);
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

	getline(&testdata, &lenLn, fp);
	while(!feof(fp)) {
		if(*testdata == '#')
			getline(&testdata, &lenLn, fp);
		else
			break; /* first non-comment */
	}


	testdata[strlen(testdata)-1] = '\0'; /* remove \n */
	/* now we have the test data to send */
	if(udpSend(testdata, strlen(testdata)) != 0)
		return(2);

	/* next line is expected output 
	 * we do not care about EOF here, this will lead to a failure and thus
	 * draw enough attention. -- rgerhards, 2009-03-31
	 */
	getline(&expected, &lenLn, fp);
	expected[strlen(expected)-1] = '\0'; /* remove \n */

	/* pull response from server and then check if it meets our expectation */
	readLine(fd, buf);
	if(strcmp(expected, buf)) {
		printf("\nExpected Response:\n'%s'\nActual Response:\n'%s'\n",
			expected, buf);
			ret = 1;
	}

	free(testdata);
	free(expected);
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
	int iFailed = 0;
	int iTests = 0;
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

		++iTests;
		/* all regular files are run through the test logic. Symlinks don't work. */
		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			printf("processing test case '%s' ... ", testFile);
			ret = processTestFile(fd, testFile);
			if(ret == 0) {
				printf("successfully completed\n");
			} else {
				printf("failed!\n");
				++iFailed;
			}
		}
	}
	globfree(&testFiles);

	if(iTests == 0) {
		printf("Error: no test cases found, no tests executed.\n");
		iFailed = 1;
	} else {
		printf("Number of tests run: %d, number of failures: %d\n", iTests, iFailed);
	}

	return(iFailed);
}


/* Run the test suite. This must be called with exactly one parameter, the
 * name of the test suite. For details, see file header comment at the top
 * of this file.
 * rgerhards, 2009-04-03
 */
int main(int argc, char *argv[])
{
	int fd;
	pid_t pid;
	int status;
	int ret = 0;
	char buf[4096];
	char testcases[4096];

	if(argc != 2) {
		printf("Invalid call of udptester\n");
		printf("Usage: udptester testsuite-name\n");
		exit(1);
	}

	testSuite = argv[1];

	if((srcdir = getenv("srcdir")) == NULL)
		srcdir = ".";

	printf("Start of udptester run ($srcdir=%s, testsuite=%s)\n", srcdir, testSuite);

	openPipe(argv[1], &pid, &fd);
	readLine(fd, buf);

	/* generate filename */
	sprintf(testcases, "%s/testsuites/*.%s", srcdir, testSuite);
	if(doTests(fd, testcases) != 0)
		ret = 1;

	/* cleanup */
	kill(pid, SIGTERM);
	waitpid(pid, &status, 0);	/* wait until instance terminates */
	printf("End of udptester run.\n");
	exit(ret);
}
