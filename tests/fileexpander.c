/* This program expands the input file several times. This
 * is done in order to obtain large (and maybe huge) files for
 * testing. Note that the input file is stored in memory. It's
 * last line must properly be terminated.
 * Max input line size is 10K.
 *
 * command line options:
 * -f	file to be used for input (else stdin)
 * -c	number of times the file is to be copied
 * -n	add line numbers (default: off)
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
static int nCopies = 1;
static int linenbrs = 0;


/* read the input file and create in-memory representation
 */
static inline void
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
	int i;
	long long unsigned lnnbr;
	struct line *node;

	lnnbr = 0;
	for(i = 0 ; i < nCopies ; ++i) {
		if(i % 1000 == 0)
			printf("In copyrun %d\n", i);
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

	while((opt = getopt(argc, argv, "i:o:c:n")) != -1) {
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
			nCopies = atoi(optarg);
			break;
		case 'n':
			linenbrs = 1;
			break;
		default:	printf("invalid option '%c' or value missing - terminating...\n", opt);
				exit (1);
				break;
		}
	}

	readFile();
	genCopies();
}
