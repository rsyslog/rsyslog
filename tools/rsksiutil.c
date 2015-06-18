/* This is a tool for dumpoing the content of GuardTime KSI
 * files in a (somewhat) human-readable manner.
 * 
 * Copyright 2015 Adiscon GmbH
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
#include <getopt.h>

#include "librsksi.h"

typedef unsigned char uchar;

static enum { MD_DUMP, MD_DETECT_FILE_TYPE, MD_SHOW_SIGBLK_PARAMS,
              MD_VERIFY, MD_EXTEND
} mode = MD_DUMP;
static int verbose = 0;
static int debug = 0; 

static void
dumpFile(char *name)
{
	FILE *fp;
	uchar hdr[9];
	void *obj;
	tlvrecord_t rec;
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
	if((r = rsksi_tlvrdHeader(fp, hdr)) != 0) goto err;
	printf("File Header: '%s'\n", hdr);
	while(1) { /* we will err out on EOF */
		if((r = rsksi_tlvrd(fp, &rec, &obj)) != 0) {
			if(feof(fp))
				break;
			else
				goto err;
		}
		rsksi_tlvprint(stdout, rec.tlvtype, obj, verbose);
		rsksi_objfree(rec.tlvtype, obj);
	}

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d (%s) processing file %s\n", r, RSKSIE2String(r), name);
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
	if((r = rsksi_chkFileHdr(fp, "LOGSIG10")) != 0) goto err;

	while(1) { /* we will err out on EOF */
		if((r = rsksi_getBlockParams(fp, 0, &bs, &bHasRecHashes,
				        &bHasIntermedHashes)) != 0)
			goto err;
		++blkCnt;
		rsksi_printBLOCK_SIG(stdout, bs, verbose);
		printf("\t***META INFORMATION:\n");
		printf("\tBlock Nbr in File...: %llu\n", (long long unsigned) blkCnt);
		printf("\tHas Record Hashes...: %d\n", bHasRecHashes);
		printf("\tHas Tree Hashes.....: %d\n", bHasIntermedHashes);
	}

	if(fp != stdin)
		fclose(fp);
	return;
err:
	if(r != RSGTE_EOF)
		fprintf(stderr, "error %d (%s) processing file %s\n", r, RSKSIE2String(r), name);
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
	if((r = rsksi_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10"))
		typeName = "Log Signature File, Version 10";
	else if(!strcmp(hdr, "KSISTAT10"))
		typeName = "rsyslog GuardTime KSI Signature State File, Version 10";
	else
		typeName = "unknown";

	printf("%s: %s [%s]\n", name, hdr, typeName);

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d (%s) processing file %s\n", r, RSKSIE2String(r), name);
}

static inline int
doVerifyRec(FILE *logfp, FILE *sigfp, FILE *nsigfp,
		/*block_sig_t *bs, */ ksifile ksi, gterrctx_t *ectx, uint8_t bInBlock)
{
	int r;
	size_t lenRec;
	char line[128*1024];

	if(fgets(line, sizeof(line), logfp) == NULL) {
		if(feof(logfp)) {
			r = RSGTE_EOF;
		} else {
			perror("log file input");
			r = RSGTE_IO;
		}
		goto done;
	}
	lenRec = strlen(line);
	if(line[lenRec-1] == '\n') {
		line[lenRec-1] = '\0';
		--lenRec;
		rsksi_errctxSetErrRec(ectx, line);
	}

	/* we need to preserve the first line (record) of each block for
	 * error-reporting purposes (bInBlock==0 meanst start of block)
	 */
	if(bInBlock == 0)
		rsksi_errctxFrstRecInBlk(ectx, line);

	r = rsksi_vrfy_nextRec(ksi, sigfp, nsigfp, (unsigned char*)line, lenRec, ectx);
done:
	return r;
}

/* We handle both verify and extend with the same function as they
 * are very similiar.
 *
 * note: here we need to have the LOG file name, not signature!
 */
static void
verify(char *name)
{
	FILE *logfp = NULL, *sigfp = NULL, *nsigfp = NULL;
	block_sig_t *bs = NULL;
	ksifile ksi;
	uint8_t bHasRecHashes, bHasIntermedHashes;
	uint8_t bInBlock;
	int r = 0;
	char sigfname[4096];
	char oldsigfname[4096];
	char nsigfname[4096];
	gterrctx_t ectx;
	int bInitDone = 0;
	
	if(!strcmp(name, "-")) {
		fprintf(stderr, "%s mode cannot work on stdin\n",
			mode == MD_VERIFY ? "verify" : "extend");
		goto err;
	} else {
		snprintf(sigfname, sizeof(sigfname), "%s.ksisig", name);
		sigfname[sizeof(sigfname)-1] = '\0';
		if((logfp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
		if((sigfp = fopen(sigfname, "r")) == NULL) {
			perror(sigfname);
			goto err;
		}
		if(mode == MD_EXTEND) {
			snprintf(nsigfname, sizeof(nsigfname), "%s.ksisig.new", name);
			nsigfname[sizeof(nsigfname)-1] = '\0';
			if((nsigfp = fopen(nsigfname, "w")) == NULL) {
				perror(nsigfname);
				goto err;
			}
			snprintf(oldsigfname, sizeof(oldsigfname),
			         "%s.ksisig.old", name);
			oldsigfname[sizeof(oldsigfname)-1] = '\0';
		}
	}

	rsksiInit("rsyslog rsksiutil " VERSION);
	rsksi_errctxInit(&ectx);
	bInitDone = 1;
	ectx.verbose = verbose;
	ectx.fp = stderr;
	ectx.filename = strdup(sigfname);
	if((r = rsksi_chkFileHdr(sigfp, "LOGSIG10")) != 0) {
		fprintf(stderr, "error %d: rsksi_chkFileHdr", r); 
		goto done;
	}
	if(mode == MD_EXTEND) {
		if(fwrite("LOGSIG10", 8, 1, nsigfp) != 1) {
			perror(nsigfname);
			r = RSGTE_IO;
			goto done;
		}
	}
	ksi = rsksi_vrfyConstruct_gf();
	if(ksi == NULL) {
		fprintf(stderr, "error initializing signature file structure\n");
		goto done;
	}

	bInBlock = 0;
	ectx.blkNum = 0;
	ectx.recNumInFile = 0;

	while(!feof(logfp)) {
		if(bInBlock == 0) {
			if(bs != NULL)
				rsksi_objfree(0x0902, bs);
			if((r = rsksi_getBlockParams(sigfp, 1, &bs, &bHasRecHashes,
							&bHasIntermedHashes)) != 0) {
				if(ectx.blkNum == 0) {
					fprintf(stderr, "Error %d before finding any signature block - "
						"is the file still open and being written to?\n", r);
				} else {
					if(verbose)
						fprintf(stderr, "EOF after signature block %lld\n",
							(long long unsigned) ectx.blkNum);
				}
				goto done;
			}
			rsksi_vrfyBlkInit(ksi, bs, bHasRecHashes, bHasIntermedHashes);
			ectx.recNum = 0;
			++ectx.blkNum;
		}
		++ectx.recNum, ++ectx.recNumInFile;
		if((r = doVerifyRec(logfp, sigfp, nsigfp, /*bs,*/ ksi, &ectx, bInBlock)) != 0)
			goto done;
		if(ectx.recNum == bs->recCount) {
			if((r = verifyBLOCK_SIG(bs, ksi, sigfp, nsigfp, 
			    (mode == MD_EXTEND) ? 1 : 0, &ectx)) != 0)
				goto done;
			bInBlock = 0;
		} else	bInBlock = 1;
	}
done:
	if(r != RSGTE_EOF)
		goto err;

	/* Make sure we've reached the end of file in both log and signature file */
	if (fgetc(logfp) != EOF) {
		fprintf(stderr, "There are unsigned records in the end of log.\n");
		fprintf(stderr, "Last signed record: %s\n", ectx.errRec);
		r = RSGTE_END_OF_SIG;
		goto err;
	}
	if (fgetc(sigfp) != EOF) {
		fprintf(stderr, "There are records missing from the end of the log file.\n");
		r = RSGTE_END_OF_LOG;
		goto err;
	}


	fclose(logfp); logfp = NULL;
	fclose(sigfp); sigfp = NULL;
	if(nsigfp != NULL) {
		fclose(nsigfp); nsigfp = NULL;
	}

	/* everything went fine, so we rename files if we updated them */
	if(mode == MD_EXTEND) {
		if(unlink(oldsigfname) != 0) {
			if(errno != ENOENT) {
				perror("unlink oldsig");
				r = RSGTE_IO;
				goto err;
			}
		}
		if(link(sigfname, oldsigfname) != 0) {
			perror("link oldsig");
			r = RSGTE_IO;
			goto err;
		}
		if(unlink(sigfname) != 0) {
			perror("unlink cursig");
			r = RSGTE_IO;
			goto err;
		}
		if(link(nsigfname, sigfname) != 0) {
			perror("link  newsig");
			fprintf(stderr, "WARNING: current sig file has been "
			        "renamed to %s - you need to manually recover "
				"it.\n", oldsigfname);
			r = RSGTE_IO;
			goto err;
		}
		if(unlink(nsigfname) != 0) {
			perror("unlink newsig");
			fprintf(stderr, "WARNING: current sig file has been "
			        "renamed to %s - you need to manually recover "
				"it.\n", oldsigfname);
			r = RSGTE_IO;
			goto err;
		}
	}
	/* OLDCODE rsksiExit();*/
	rsksi_errctxExit(&ectx);
	return;

err:
	if(r != 0)
		fprintf(stderr, "error %d (%s) processing file %s\n",
			r, RSKSIE2String(r), name);
	if(logfp != NULL)
		fclose(logfp);
	if(sigfp != NULL)
		fclose(sigfp);
	if(nsigfp != NULL) {
		fclose(nsigfp);
		unlink(nsigfname);
	}
	if(bInitDone) {
		/* OLDCODE rsksiExit();*/
		rsksi_errctxExit(&ectx);
	}
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
	case MD_VERIFY:
	case MD_EXTEND:
		verify(name);
		break;
	}
}


static struct option long_options[] = 
{ 
	{"dump", no_argument, NULL, 'D'},
	{"verbose", no_argument, NULL, 'v'},
	{"debug", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'V'},
	{"detect-file-type", no_argument, NULL, 'T'},
	{"show-sigblock-params", no_argument, NULL, 'B'},
	{"verify", no_argument, NULL, 't'}, /* 't' as in "test signatures" */
	{"extend", no_argument, NULL, 'e'},
	{"publications-server", required_argument, NULL, 'P'},
	{"show-verified", no_argument, NULL, 's'},
	{NULL, 0, NULL, 0} 
}; 

int
main(int argc, char *argv[])
{
	int i;
	int opt;

	while(1) {
		opt = getopt_long(argc, argv, "dDvVTBPtes", long_options, NULL);
		if(opt == -1)
			break;
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			debug = 1;
			rsksi_set_debug(debug); 
			break;
		case 's':
			rsksi_read_showVerified = 1;
			break;
		case 'V':
			fprintf(stderr, "rsksiutil " VERSION "\n");
			exit(0);
		case 'D':
			mode = MD_DUMP;
			break;
		case 'B':
			mode = MD_SHOW_SIGBLK_PARAMS;
			break;
		case 'P':
			rsksi_read_puburl = optarg;
			break;
		case 'T':
			mode = MD_DETECT_FILE_TYPE;
			break;
		case 't':
			mode = MD_VERIFY;
			break;
		case 'e':
			mode = MD_EXTEND;
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
