/* This is a tool for offline signing logfiles via the guardtime API.
 *
 * NOTE: this currently is only a PoC and WiP! NOT suitable for
 *       production use!
 *
 * Current hardcoded timestamper (use this if you do not have an
 * idea of which one else to use):
 *    http://stamper.guardtime.net/gt-signingservice
 * Check the GuardTime website for the URLs of nearest public services.
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


#if 0
void
outputhash(GTDataHash *hash)
{
	int i;
	for(i = 0 ; i < hash->digest_length ; ++i)
		printf("%2.2x", hash->digest[i]);
	printf("\n");
}

void
timestampIt(GTDataHash *hash)
{
	int r = GT_OK;
	GTTimestamp *timestamp = NULL;
	unsigned char *der = NULL;
	char *sigFile = "logsigner.TIMESTAMP";
	size_t der_len;

	/* Get the timestamp. */
	r = GTHTTP_createTimestampHash(hash,
		"http://stamper.guardtime.net/gt-signingservice", &timestamp);

	if(r != GT_OK) {
		fprintf(stderr, "GTHTTP_createTimestampHash() failed: %d (%s)\n",
				r, GTHTTP_getErrorString(r));
		goto done;
	}

	/* Encode timestamp. */
	r = GTTimestamp_getDEREncoded(timestamp, &der, &der_len);
	if(r != GT_OK) {
		fprintf(stderr, "GTTimestamp_getDEREncoded() failed: %d (%s)\n",
				r, GT_getErrorString(r));
		goto done;
	}

	/* Save DER-encoded timestamp to file. */
	r = GT_saveFile(sigFile, der, der_len);
	if(r != GT_OK) {
		fprintf(stderr, "Cannot save timestamp to file %s: %d (%s)\n",
				sigFile, r, GT_getErrorString(r));
		if(r == GT_IO_ERROR) {
			fprintf(stderr, "\t%d (%s)\n", errno, strerror(errno));
		}
		goto done;
	}
	printf("Timestamping succeeded!\n");
done:
	GT_free(der);
	GTTimestamp_free(timestamp);
}


void
sign(const char *buf, const size_t len)
{
	int r;
	GTDataHash *hash = NULL;

	printf("hash for '%s' is ", buf);
	r = GTDataHash_create(GT_HASHALG_SHA256, (const unsigned char*)buf, len, &hash);
	if(r != GT_OK) {
		fprintf(stderr, "GTTDataHash_create() failed: %d (%s)\n",
				r, GT_getErrorString(r));
		goto done;
	}
	outputhash(hash);
	timestampIt(hash); /* of course, this needs to be moved to once at end ;) */
done:	GTDataHash_free(hash);
}
#endif

void
processFile(char *name)
{
	FILE *fp;
	size_t len;
	char line[64*1024+1];
	gtctx ctx = NULL;

	ctx = rsgtCtxNew((unsigned char*)"SIGFILE", GT_HASHALG_SHA256);
	sigblkInit(ctx);
	if(!strcmp(name, "-"))
		fp = stdin;
	else
		fp = fopen(name, "r");

	while(1) {
		if(fgets(line, sizeof(line), fp) == NULL) {
			if(!feof(fp))
				perror(name);
			break;
		}
		len = strlen(line);
		if(line[len-1] == '\n') {
			--len;
			line[len] = '\0';
		}
		//sign(line, len);
		sigblkAddRecord(ctx, (unsigned char*)line, len);
	}

	if(fp != stdin)
		fclose(fp);
	sigblkFinish(ctx);
	rsgtCtxDel(ctx);
}


int
main(int argc, char *argv[])
{
	rsgtInit("rsyslog logsigner " VERSION);
	processFile("-");
	rsgtExit();
	return 0;
}
