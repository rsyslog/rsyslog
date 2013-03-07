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

#include "librsgt.h"

typedef unsigned char uchar;

void
processFile(char *name)
{
	FILE *fp;
	uchar hdr[9];
	uint16_t tlvtype, tlvlen;
	void *obj;
	
	if(!strcmp(name, "-"))
		fp = stdin;
	else {
		printf("Processing file %s:\n", name);
		if((fp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if(rsgt_tlvrdHeader(fp, hdr) != 0) goto err;
	printf("File Header: '%s'\n", hdr);
	if(rsgt_tlvrd(fp, &tlvtype, &tlvlen, &obj) != 0) goto err;
	rsgt_tlvprint(stdout, tlvtype, obj, 0);

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error processing file %s\n", name);
}


int
main(int argc, char *argv[])
{
	int i;
	if(argc == 1)
		processFile("-");
	else {
		for(i = 1 ; i < argc ; ++i)
			processFile(argv[i]);
	}
	return 0;
}
