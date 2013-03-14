/* This is a tool for dumpoing the content of GuardTime TLV
 * files in a (somewhat) human-readable manner.
 * 
 * Copyright 2013 Adiscon GmbH
 *
 * This file is part of rsyslog.
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either exprs or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <gt_base.h>
#include <gt_http.h>
#include <getopt.h>

#include "librsgt.h"

typedef unsigned char uchar;

static enum { MD_DUMP, MD_DETECT_FILE_TYPE, MD_SHOW_SIGBLK_PARAMS
} mode = MD_DUMP;
static int verbose = 0;

static void
dumpFile(char *name)
{
	FILE *fp;
	uchar hdr[9];
	uint16_t tlvtype, tlvlen;
	void *obj;
	int r = -1;
	
	if(!strcmp(name, "-"))
		fp = stdin;
	else {
		printf("Processing file %s:\n", name);
		if((fp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if((r = rsgt_tlvrdHeader(fp, hdr)) != 0) goto err;
	printf("File Header: '%s'\n", hdr);
	while(1) { /* we will err out on EOF */
		if((r = rsgt_tlvrd(fp, &tlvtype, &tlvlen, &obj)) != 0) {
			if(feof(fp))
				break;
			else
				goto err;
		}
		rsgt_tlvprint(stdout, tlvtype, obj, verbose);
	}

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d processing file %s\n", r, name);
}

static void
showSigblkParams(char *name)
{
	FILE *fp;
	block_sig_t *bs;
	uint8_t bHasRecHashes, bHasIntermedHashes;
	uint64_t blkCnt = 0;
	int r = -1;
	
	if(!strcmp(name, "-"))
		fp = stdin;
	else {
		if((fp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if((r = rsgt_chkFileHdr(fp, "LOGSIG10")) != 0) goto err;

	while(1) { /* we will err out on EOF */
		if((r = rsgt_getBlockParams(fp, 0, &bs, &bHasRecHashes,
				        &bHasIntermedHashes)) != 0)
			goto err;
		++blkCnt;
		rsgt_printBLOCK_SIG(stdout, bs, verbose);
		printf("\t***META INFORMATION:\n");
		printf("\tBlock Nbr in File......: %llu\n", blkCnt);
		printf("\tHas Record Hashes......: %d\n", bHasRecHashes);
		printf("\tHas Intermediate Hashes: %d\n", bHasIntermedHashes);
	}

	if(fp != stdin)
		fclose(fp);
	return;
err:
	if(r != RSGTE_EOF)
		fprintf(stderr, "error %d processing file %s\n", r, name);
}

static void
detectFileType(char *name)
{
	FILE *fp;
	char *typeName;
	char hdr[9];
	int r = -1;
	
	if(!strcmp(name, "-"))
		fp = stdin;
	else {
		if((fp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if((r = rsgt_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10"))
		typeName = "Log Signature File, Version 10";
	else
		typeName = "unknown";

	printf("%s: %s [%s]\n", name, hdr, typeName);

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d processing file %s\n", r, name);
}

static void
processFile(char *name)
{
	switch(mode) {
	case MD_DETECT_FILE_TYPE:
		detectFileType(name);
		break;
	case MD_DUMP:
		dumpFile(name);
		break;
	case MD_SHOW_SIGBLK_PARAMS:
		showSigblkParams(name);
		break;
	}
}


static struct option long_options[] = 
{ 
	{"dump", no_argument, NULL, 'D'},
	{"verbose", no_argument, NULL, 'v'},
	{"version", no_argument, NULL, 'V'},
	{"detect-file-type", no_argument, NULL, 'T'},
	{"show-sigblock-params", no_argument, NULL, 'B'},
	{NULL, 0, NULL, 0} 
}; 

int
main(int argc, char *argv[])
{
	int i;
	int opt;

	while(1) {
		opt = getopt_long(argc, argv, "v", long_options, NULL);
		if(opt == -1)
			break;
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		case 'V':
			fprintf(stderr, "rsgtutil " VERSION "\n");
			exit(0);
		case 'D':
			mode = MD_DUMP;
			break;
		case 'B':
			mode = MD_SHOW_SIGBLK_PARAMS;
			break;
		case 'T':
			mode = MD_DETECT_FILE_TYPE;
			break;
		case '?':
			break;
		default:fprintf(stderr, "getopt_long() returns unknown value %d\n", opt);
			return 1;
		}
	}

	if(optind == argc)
		processFile("-");
	else {
		for(i = optind ; i < argc ; ++i)
			processFile(argv[i]);
	}

	return 0;
}
