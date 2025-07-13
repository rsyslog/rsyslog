/* This program expands the input file several times. This
 * is done in order to obtain large (and maybe huge) files for
 * testing. Note that the input file is stored in memory. It's
 * last line must properly be terminated.
 * Max input line size is 10K.
 *
 * command line options:
 * -i   file to be used for input (else stdin)
 * -o   file to be used for output (else stdout)
 * -c   number of times the file is to be copied
 * -n   add line numbers (default: off)
 * -w   wait nbr of microsecs between batches
 * -W   number of file lines to generate in a batch
 *      This is useful only if -w is specified as well,
 *      default is 1000.
 *
 * Copyright 2010-2016 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* input file is stored in a single-linked list */
struct line {
    struct line *next;
    char *ln;
} *root, *tail;

static FILE *fpIn;
static FILE *fpOut;
static long long nCopies = 1;
static int linenbrs = 0;
static int waitusecs = 0;
static int batchsize = 1000;


/* read the input file and create in-memory representation
 */
static void
readFile()
{
    char *r;
    char lnBuf[10240];
    struct line *node;

    root = tail = NULL;
    r = fgets(lnBuf, sizeof(lnBuf), fpIn);
    while(r != NULL) {
        node = malloc(sizeof(struct line));
        if(node == NULL) {
            perror("malloc node");
            exit(1);
        }
        node->next = NULL;
        node->ln = strdup(lnBuf);
        if(node->ln == NULL) {
            perror("malloc node");
            exit(1);
        }
        if(tail == NULL) {
            tail = root = node;
        } else {
            tail->next = node;
            tail = node;
        }
        r = fgets(lnBuf, sizeof(lnBuf), fpIn);
    }
    if(!feof(fpIn)) {
        perror("fgets");
        fprintf(stderr, "end of read loop, but not end of file!");
        exit(1);
    }
}


static void
genCopies()
{
    long long i;
    long long unsigned lnnbr;
    struct line *node;

    lnnbr = 1;
    for(i = 0 ; i < nCopies ; ++i) {
        if(i % 10000 == 0)
            fprintf(stderr, "copyrun %lld\n", i);
        if(waitusecs && (i % batchsize == 0)) {
            usleep(waitusecs);
        }
        for(node = root ; node != NULL ; node = node->next) {
            if(linenbrs)
                fprintf(fpOut, "%12.12llu:%s", lnnbr, node->ln);
            else
                fprintf(fpOut, "%s", node->ln);
            ++lnnbr;
        }
    }
}

void main(int argc, char *argv[])
{
    int opt;
    fpIn = stdin;
    fpOut = stdout;

    while((opt = getopt(argc, argv, "i:o:c:nw:W:")) != -1) {
        switch (opt) {
        case 'i': /* input file */
            if((fpIn = fopen(optarg, "r")) == NULL) {
                perror(optarg);
                exit(1);
            }
            break;
        case 'o': /* output file */
            if((fpOut = fopen(optarg, "w")) == NULL) {
                perror(optarg);
                exit(1);
            }
            break;
        case 'c':
            nCopies = atoll(optarg);
            break;
        case 'n':
            linenbrs = 1;
            break;
        case 'w':
            waitusecs = atoi(optarg);
            break;
        case 'W':
            batchsize = atoi(optarg);
            break;
        default:    printf("invalid option '%c' or value missing - terminating...\n", opt);
                exit (1);
                break;
        }
    }

    readFile();
    genCopies();
}
