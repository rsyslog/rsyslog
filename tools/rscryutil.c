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
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <gcrypt.h>


static enum { MD_DECRYPT
} mode = MD_DECRYPT;
static int verbose = 0;
static gcry_cipher_hd_t gcry_chd;
static size_t blkLength;

static int
initCrypt(int gcry_mode, char *iv, char *key)
{
	#define GCRY_CIPHER GCRY_CIPHER_3DES  // TODO: make configurable
 	int r = 0;
	gcry_error_t     gcryError;

	blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER);
	size_t keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
	if(strlen(key) != keyLength) {
		fprintf(stderr, "invalid key lengtjh; key is %u characters, but "
			"exactly %u characters are required\n", strlen(key),
			keyLength);
		r = 1; goto done;
	}

	gcryError = gcry_cipher_open(&gcry_chd, GCRY_CIPHER, gcry_mode, 0);
	if (gcryError) {
		printf("gcry_cipher_open failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		r = 1; goto done;
	}

	gcryError = gcry_cipher_setkey(gcry_chd, key, keyLength);
	if (gcryError) {
		printf("gcry_cipher_setkey failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		r = 1; goto done;
	}

	gcryError = gcry_cipher_setiv(gcry_chd, iv, blkLength);
	if (gcryError) {
		printf("gcry_cipher_setiv failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		r = 1; goto done;
	}
done: return r;
}

static inline void
removePadding(char *buf, size_t *plen)
{
	unsigned len = (unsigned) *plen;
	unsigned iSrc, iDst;
	char *frstNUL;

	frstNUL = strchr(buf, 0x00);
	if(frstNUL == NULL)
		goto done;
	iDst = iSrc = frstNUL - buf;

	while(iSrc < len) {
		if(buf[iSrc] != 0x00)
			buf[iDst++] = buf[iSrc];
		++iSrc;
	}

	*plen = iDst;
done:	return;
}

static void
doDeCrypt(FILE *fpin, FILE *fpout)
{
	gcry_error_t     gcryError;
	char	buf[64*1024];
	size_t	nRead, nWritten;
	size_t nPad;
	
	while(1) {
		nRead = fread(buf, 1, sizeof(buf), fpin);
		if(nRead == 0)
			break;
		nPad = (blkLength - nRead % blkLength) % blkLength;
		fprintf(stderr, "--->read %d chars, blkLength %d, mod %d, pad %d\n", nRead, blkLength,
			nRead % blkLength, nPad);
		gcryError = gcry_cipher_decrypt(
				gcry_chd, // gcry_cipher_hd_t
				buf,    // void *
				nRead,    // size_t
				NULL,    // const void *
				0);   // size_t
		if (gcryError) {
			fprintf(stderr, "gcry_cipher_encrypt failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
			return;
		}
		removePadding(buf, &nRead);
		nWritten = fwrite(buf, 1, nRead, fpout);
		if(nWritten != nRead) {
			perror("fpout");
			return;
		}
	}
}


static void
decrypt(char *name, char *key)
{
	FILE *logfp = NULL;
	//, *sigfp = NULL;
	int r = 0;
	//char sigfname[4096];
	
	if(!strcmp(name, "-")) {
		fprintf(stderr, "decrypt mode cannot work on stdin\n");
		goto err;
	} else {
		if((logfp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
#if 0
		snprintf(sigfname, sizeof(sigfname), "%s.gtsig", name);
		sigfname[sizeof(sigfname)-1] = '\0';
		if((sigfp = fopen(sigfname, "r")) == NULL) {
			perror(sigfname);
			goto err;
		}
#endif
	}

	if(initCrypt(GCRY_CIPHER_MODE_CBC, "TODO: init value", key) != 0)
		goto err;
	doDeCrypt(logfp, stdout);
	gcry_cipher_close(gcry_chd);
	fclose(logfp); logfp = NULL;
	return;

err:
	fprintf(stderr, "error %d processing file %s\n", r, name);
	if(logfp != NULL)
		fclose(logfp);
}


static struct option long_options[] = 
{ 
	{"verbose", no_argument, NULL, 'v'},
	{"version", no_argument, NULL, 'V'},
	{"decrypt", no_argument, NULL, 'd'},
	{"key", required_argument, NULL, 'k'},
	{NULL, 0, NULL, 0} 
}; 

int
main(int argc, char *argv[])
{
	int i;
	int opt;
	char *key = "";

	while(1) {
		opt = getopt_long(argc, argv, "dk:vV", long_options, NULL);
		if(opt == -1)
			break;
		switch(opt) {
		case 'd':
			mode = MD_DECRYPT;
			break;
		case 'k':
			fprintf(stderr, "WARNING: specifying the actual key "
				"via the command line is highly insecure\n"
				"Do NOT use this for PRODUCTION use.\n");
			key = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			fprintf(stderr, "rsgtutil " VERSION "\n");
			exit(0);
			break;
		case '?':
			break;
		default:fprintf(stderr, "getopt_long() returns unknown value %d\n", opt);
			return 1;
		}
	}

	if(optind == argc)
		decrypt("-", key);
	else {
		for(i = optind ; i < argc ; ++i)
			decrypt(argv[i], key); /* currently only mode ;) */
	}

	memset(key, 0, strlen(key)); /* zero-out key store */
	return 0;
}
	//char *aesSymKey = "123456789012345678901234"; // TODO: TEST ONLY
