/* This is a tool for dumping the content of GuardTime TLV
 * files in a (somewhat) human-readable manner.
 * 
 * Copyright 2013-2015 Adiscon GmbH
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

#ifdef ENABLEGT
	/* Guardtime Includes */
	#include <gt_base.h>
	#include <gt_http.h>
	#include "librsgt_common.h"
	#include "librsgt.h"
#endif
#ifdef ENABLEKSI
	/* KSI Includes */
	#include <stdint.h>
	#include "librsgt_common.h"
	#include "librsksi.h"
#endif

typedef unsigned char uchar;

static enum { MD_DUMP, MD_DETECT_FILE_TYPE, MD_SHOW_SIGBLK_PARAMS,
              MD_VERIFY, MD_EXTEND, MD_CONVERT
} mode = MD_DUMP;
static enum { API_GT, API_KSI } apimode = API_GT;
static int verbose = 0;
static int debug = 0; 

#ifdef ENABLEGT
static void
dumpFile(char *name)
{
	FILE *fp;
	char hdr[9];
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
	if((r = rsgt_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10"))
		printf("File Header: Version 10 (deprecated) - conversion needed.\n");
	else if(!strcmp(hdr, "LOGSIG11"))
		printf("File Header: Version 11\n");
	else
		printf("File Header: '%s'\n", hdr);

	while(1) { /* we will err out on EOF */
		if((r = rsgt_tlvrd(fp, &rec, &obj)) != 0) {
			if(feof(fp))
				break;
			else
				goto err;
		}
		rsgt_tlvprint(stdout, rec.tlvtype, obj, verbose);
		rsgt_objfree(rec.tlvtype, obj);
	}

	if(fp != stdin)
		fclose(fp);
	return;
err:	
	fprintf(stderr, "error %d (%s) processing file %s\n", r, RSGTE2String(r), name);
}

static void
showSigblkParams(char *name)
{
	FILE *fp;
	block_sig_t *bs;
	block_hdr_t *bh;
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
	if((r = rsgt_chkFileHdr(fp, "LOGSIG11")) != 0) goto err;

	while(1) { /* we will err out on EOF */
		if((r = rsgt_getBlockParams(fp, 0, &bs, &bh, &bHasRecHashes,
				        &bHasIntermedHashes)) != 0)
			goto err;
		++blkCnt;
		rsgt_printBLOCK_HDR(stdout, bh, verbose);
		rsgt_printBLOCK_SIG(stdout, bs, verbose);
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
		fprintf(stderr, "error %d (%s) processing file %s\n", r, RSGTE2String(r), name);
}

static void
convertFile(char *name)
{
	FILE *oldsigfp = NULL, *newsigfp = NULL;
	char hdr[9];
	int r = -1;
	char newsigfname[4096];
	char oldsigfname[4096];
	
	if(!strcmp(name, "-"))
		oldsigfp = stdin;
	else {
		printf("Processing file %s:\n", name);
		if((oldsigfp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if((r = rsgt_tlvrdHeader(oldsigfp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10")) {
		printf("Found Signature File with Version 10 - starting conversion.\n");
		snprintf(newsigfname, sizeof(newsigfname), "%s.LOGSIG11", name);
		snprintf(oldsigfname, sizeof(oldsigfname), "%s.LOGSIG10", name);
		if((newsigfp = fopen(newsigfname, "w")) == NULL) {
			perror(newsigfname);
			r = RSGTE_IO;
			goto err;
		}
		else {
			/* Write FileHeader first */
			if ( fwrite(LOGSIGHDR, sizeof(LOGSIGHDR)-1, 1, newsigfp) != 1) goto err;
		}

		if ((r = rsgt_ConvertSigFile(name, oldsigfp, newsigfp, verbose)) != 0)
			goto err;
		else {
			/* Close FILES */
			if(oldsigfp != stdin)
				fclose(oldsigfp);
			if (newsigfp != NULL)	
				fclose(newsigfp); 

			/* Delete OLDFILE if there is one*/
			if(unlink(oldsigfname) != 0) {
				if(errno != ENOENT) {
					perror("Error removing old file");
					r = RSGTE_IO;
					goto err;
				}
			}
			/* Copy main sigfile to oldfile */
			if(link(name, oldsigfname) != 0) {
				perror("Error moving old file");
				r = RSGTE_IO;
				goto err;
			}

			/* Delete current sigfile*/
			if(unlink(name) != 0) {
				if(errno != ENOENT) {
					perror("Error removing old file");
					r = RSGTE_IO;
					goto err;
				}
			}
			/* Copy new sigfile to main sigfile */
			if(link(newsigfname, name) != 0) {
				perror("Error moving new file");
				r = RSGTE_IO;
				goto err;
			}

			/* Delete temporary new sigfile*/
			if(unlink(newsigfname) != 0) {
				if(errno != ENOENT) {
					perror("Error removing new file");
					r = RSGTE_IO;
					goto err;
				}
			}
			
			printf("File %s was converted to Version 11.\n", name);
		}
	}
	else
		printf("File does not need to be converted, File Header is: '%s'\n", hdr);
	return;
err:	
	fprintf(stderr, "error %d (%s) converting file %s\n", r, RSGTE2String(r), name);
}

#endif

#ifdef ENABLEKSI
static void
dumpFileKSI(char *name)
{
	FILE *fp;
	char hdr[9];
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
	if((r = rsksi_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10"))
		printf("File Header: Version 10 (deprecated) - conversion needed.\n");
	else if(!strcmp(hdr, "LOGSIG11"))
		printf("File Header: Version 11\n");
	else
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
showSigblkParamsKSI(char *name)
{
	FILE *fp;
	block_sig_t *bs;
	block_hdr_t *bh;
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
	if((r = rsksi_chkFileHdr(fp, "LOGSIG11")) != 0) goto err;

	while(1) { /* we will err out on EOF */
		if((r = rsksi_getBlockParams(fp, 0, &bs, &bh, &bHasRecHashes,
				        &bHasIntermedHashes)) != 0)
			goto err;
		++blkCnt;
		rsksi_printBLOCK_HDR(stdout, bh, verbose);
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
convertFileKSI(char *name)
{
	FILE *oldsigfp = NULL, *newsigfp = NULL;
	char hdr[9];
	int r = -1;
	char newsigfname[4096];
	char oldsigfname[4096];
	
	if(!strcmp(name, "-"))
		oldsigfp = stdin;
	else {
		printf("Processing file %s:\n", name);
		if((oldsigfp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}
	}
	if((r = rsksi_tlvrdHeader(oldsigfp, (uchar*)hdr)) != 0) goto err;
	if(!strcmp(hdr, "LOGSIG10")) {
		printf("Found Signature File with Version 10 - starting conversion.\n");
		snprintf(newsigfname, sizeof(newsigfname), "%s.LOGSIG11", name);
		snprintf(oldsigfname, sizeof(oldsigfname), "%s.LOGSIG10", name);
		if((newsigfp = fopen(newsigfname, "w")) == NULL) {
			perror(newsigfname);
			r = RSGTE_IO;
			goto err;
		}
		else {
			/* Write FileHeader first */
			if ( fwrite(LOGSIGHDR, sizeof(LOGSIGHDR)-1, 1, newsigfp) != 1) goto err;
		}

		if ((r = rsksi_ConvertSigFile(name, oldsigfp, newsigfp, verbose)) != 0)
			goto err;
		else {
			/* Close FILES */
			if(oldsigfp != stdin)
				fclose(oldsigfp);
			if (newsigfp != NULL)	
				fclose(newsigfp); 

			/* Delete OLDFILE if there is one*/
			if(unlink(oldsigfname) != 0) {
				if(errno != ENOENT) {
					perror("Error removing old file");
					r = RSGTE_IO;
					goto err;
				}
			}
			/* Copy main sigfile to oldfile */
			if(link(name, oldsigfname) != 0) {
				perror("Error moving old file");
				r = RSGTE_IO;
				goto err;
			}

			/* Delete current sigfile*/
			if(unlink(name) != 0) {
				if(errno != ENOENT) {
					perror("Error removing old file");
					r = RSGTE_IO;
					goto err;
				}
			}
			/* Copy new sigfile to main sigfile */
			if(link(newsigfname, name) != 0) {
				perror("Error moving new file");
				r = RSGTE_IO;
				goto err;
			}

			/* Delete temporary new sigfile*/
			if(unlink(newsigfname) != 0) {
				if(errno != ENOENT) {
					perror("Error removing new file");
					r = RSGTE_IO;
					goto err;
				}
			}
			
			printf("File %s was converted to Version 11.\n", name);
		}
	}
	else
		printf("File does not need to be converted, File Header is: '%s'\n", hdr);
	return;
err:	
	fprintf(stderr, "error %d (%s) converting file %s\n", r, RSKSIE2String(r), name);
}

#endif

#ifdef ENABLEGT
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
		typeName = "Log Signature File, Version 10 (deprecated)";
	else if(!strcmp(hdr, "LOGSIG11"))
		typeName = "Log Signature File, Version 11";
	else if(!strcmp(hdr, "GTSTAT10"))
		typeName = "rsyslog GuardTime Signature State File, Version 10";
	else
		typeName = "unknown";

	printf("%s: %s [%s]\n", name, hdr, typeName);

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d (%s) processing file %s\n", r, RSGTE2String(r), name);
}

static inline int
doVerifyRec(FILE *logfp, FILE *sigfp, FILE *nsigfp,
	    block_sig_t *bs, gtfile gf, gterrctx_t *ectx, uint8_t bInBlock)
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
		rsgt_errctxSetErrRec(ectx, line);
	}

	/* we need to preserve the first line (record) of each block for
	 * error-reporting purposes (bInBlock==0 meanst start of block)
	 */
	if(bInBlock == 0)
		rsgt_errctxFrstRecInBlk(ectx, line);

	r = rsgt_vrfy_nextRec(bs, gf, sigfp, nsigfp, (unsigned char*)line, lenRec, ectx);
done:
	return r;
}

/* VERIFY Function using GT API 
 * We handle both verify and extend with the same function as they
 * are very similiar.
 *
 * note: here we need to have the LOG file name, not signature!
 */
static int
verifyGT(char *name, char *errbuf, char *sigfname, char *oldsigfname, char *nsigfname, FILE *logfp, FILE *sigfp, FILE *nsigfp)
{
	block_sig_t *bs = NULL;
	block_hdr_t *bh = NULL;
	gtfile gf;
	uint8_t bHasRecHashes, bHasIntermedHashes;
	uint8_t bInBlock;
	int r = 0;
	int bInitDone = 0;
	gterrctx_t ectx;
	rsgt_errctxInit(&ectx);

	rsgtInit("rsyslog rsgtutil " VERSION);
	bInitDone = 1;
	ectx.verbose = verbose;
	ectx.fp = stderr;
	ectx.filename = strdup(sigfname);

	if((r = rsgt_chkFileHdr(sigfp, "LOGSIG11")) != 0) {
		if (debug)
			fprintf(stderr, "error %d in rsgt_chkFileHdr\n", r); 
		goto done;
	}

	if(mode == MD_EXTEND) {
		if(fwrite("LOGSIG11", 8, 1, nsigfp) != 1) {
			perror(nsigfname);
			r = RSGTE_IO;
			goto done;
		}
	}
	gf = rsgt_vrfyConstruct_gf();
	if(gf == NULL) {
		fprintf(stderr, "error initializing signature file structure\n");
		goto done;
	}

	bInBlock = 0;
	ectx.blkNum = 0;
	ectx.recNumInFile = 0;

	while(!feof(logfp)) {
		if(bInBlock == 0) {
			if(bs != NULL)
				rsgt_objfree(0x0904, bs);
			if (bh != NULL)
				rsgt_objfree(0x0901, bh);
			if((r = rsgt_getBlockParams(sigfp, 1, &bs, &bh, &bHasRecHashes,
							&bHasIntermedHashes)) != 0) {
				if(ectx.blkNum == 0) {
					fprintf(stderr, "EOF before finding any signature block - "
						"is the file still open and being written to?\n");
				} else {
					if(verbose)
						fprintf(stderr, "EOF after signature block %lld\n",
							(long long unsigned) ectx.blkNum);
				}
				goto done;
			}
			/* Copy block header */
			if ((r = verifyBLOCK_HDR(sigfp, nsigfp)) != 0) goto done;

			rsgt_vrfyBlkInit(gf, bh, bHasRecHashes, bHasIntermedHashes);
			ectx.recNum = 0;
			++ectx.blkNum;
		}
		++ectx.recNum, ++ectx.recNumInFile;
		if((r = doVerifyRec(logfp, sigfp, nsigfp, bs, gf, &ectx, bInBlock)) != 0)
			goto done;
		if(ectx.recNum == bs->recCount) {
			if((r = verifyBLOCK_SIG(bs, gf, sigfp, nsigfp, 
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

	rsgtExit();
	rsgt_errctxExit(&ectx);
	return 1;

err:
	if(r != 0)
		sprintf(errbuf, "error %d (%s) processing file %s\n",
			r, RSGTE2String(r), name);
	else
		errbuf[0] = '\0';

	if(logfp != NULL)
		fclose(logfp);
	if(sigfp != NULL)
		fclose(sigfp);
	if(nsigfp != NULL) {
		fclose(nsigfp);
		unlink(nsigfname);
	}
	if(bInitDone) {
		rsgtExit();
		rsgt_errctxExit(&ectx);
	}
	return 0;
}
#endif

#ifdef ENABLEKSI
static void
detectFileTypeKSI(char *name)
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
		typeName = "Log Signature File, Version 10 (deprecated)";
	else if(!strcmp(hdr, "LOGSIG11"))
		typeName = "Log Signature File, Version 11";
	else if(!strcmp(hdr, "GTSTAT10"))
		typeName = "rsyslog GuardTime Signature State File, Version 10";
	else
		typeName = "unknown";

	printf("%s: %s [%s]\n", name, hdr, typeName);

	if(fp != stdin)
		fclose(fp);
	return;
err:	fprintf(stderr, "error %d (%s) processing file %s\n", r, RSKSIE2String(r), name);
}

static inline int
doVerifyRecKSI(FILE *logfp, FILE *sigfp, FILE *nsigfp,
		/*block_sig_t *bs, */ ksifile ksi, ksierrctx_t *ectx, uint8_t bInBlock)
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

/* VERIFY Function using KSI API 
 * We handle both verify and extend with the same function as they
 * are very similiar.
 *
 * note: here we need to have the LOG file name, not signature!
 */
static int
verifyKSI(char *name, char *errbuf, char *sigfname, char *oldsigfname, char *nsigfname, FILE *logfp, FILE *sigfp, FILE *nsigfp)
{
	block_sig_t *bs = NULL;
	block_hdr_t *bh = NULL;
	ksifile ksi;
	uint8_t bHasRecHashes, bHasIntermedHashes;
	uint8_t bInBlock;
	int r = 0;
	int bInitDone = 0;
	ksierrctx_t ectx;
	rsksi_errctxInit(&ectx);

	rsksiInit("rsyslog rsksiutil " VERSION);
	bInitDone = 1;
	ectx.verbose = verbose;
	ectx.fp = stderr;
	ectx.filename = strdup(sigfname);

	if((r = rsksi_chkFileHdr(sigfp, "LOGSIG11")) != 0) {
		if (debug)
			fprintf(stderr, "error %d in rsksi_chkFileHdr\n", r); 
		goto done;
	}
	if(mode == MD_EXTEND) {
		if(fwrite("LOGSIG11", 8, 1, nsigfp) != 1) {
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
				rsksi_objfree(0x0904, bs);
			if (bh != NULL)
				rsksi_objfree(0x0901, bh);
			if((r = rsksi_getBlockParams(sigfp, 1, &bs, &bh, &bHasRecHashes,
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
			/* Copy block header */
			if ((r = verifyBLOCK_HDRKSI(sigfp, nsigfp)) != 0) goto done;

			rsksi_vrfyBlkInit(ksi, bh, bHasRecHashes, bHasIntermedHashes);
			ectx.recNum = 0;
			++ectx.blkNum;
		}
		++ectx.recNum, ++ectx.recNumInFile;
		if((r = doVerifyRecKSI(logfp, sigfp, nsigfp, /*bs,*/ ksi, &ectx, bInBlock)) != 0)
			goto done;
		if(ectx.recNum == bs->recCount) {
			if((r = verifyBLOCK_SIGKSI(bs, ksi, sigfp, nsigfp, 
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
	return 1;

err:
	if(r != 0)
		sprintf(errbuf, "error %d (%s) processing file %s\n",
			r, RSKSIE2String(r), name);
	else
		errbuf[0] = '\0';
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
	return 0; 
}
#endif

/* VERIFY if logfile has a Guardtime Signfile 
*/
static void
verify(char *name, char *errbuf)
{
	int iSuccess = 1; 
	char sigfname[4096];
	char oldsigfname[4096];
	char nsigfname[4096];
	FILE *logfp = NULL, *sigfp = NULL, *nsigfp = NULL;
	
	if(!strcmp(name, "-")) {
		fprintf(stderr, "%s mode cannot work on stdin\n",
			mode == MD_VERIFY ? "verify" : "extend");
		goto err;
	} else {
		/* First check for the logfile itself */
		if((logfp = fopen(name, "r")) == NULL) {
			perror(name);
			goto err;
		}

		/* Check for .gtsig file */
		snprintf(sigfname, sizeof(sigfname), "%s.gtsig", name);
		sigfname[sizeof(sigfname)-1] = '\0';
		if((sigfp = fopen(sigfname, "r")) == NULL) {
			/* Use errbuf to output error later */
			sprintf(errbuf, "%s: No such file or directory\n", sigfname);

			/* Check for .ksisig file */
			snprintf(sigfname, sizeof(sigfname), "%s.ksisig", name);
			sigfname[sizeof(sigfname)-1] = '\0';
			if((sigfp = fopen(sigfname, "r")) == NULL) {
				/* Output old error first*/
				fputs(errbuf, stderr); 

				sprintf(errbuf, "%s: No such file or directory\n", sigfname);
				fputs(errbuf, stderr); 
				/*perror(sigfname);*/
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
			goto verifyKSI;
		}
		if(mode == MD_EXTEND) {
			snprintf(nsigfname, sizeof(nsigfname), "%s.gtsig.new", name);
			nsigfname[sizeof(nsigfname)-1] = '\0';
			if((nsigfp = fopen(nsigfname, "w")) == NULL) {
				perror(nsigfname);
				goto err;
			}
			snprintf(oldsigfname, sizeof(oldsigfname),
			         "%s.gtsig.old", name);
			oldsigfname[sizeof(oldsigfname)-1] = '\0';
		}
		goto verifyGT;
	}

verifyGT:
#ifdef ENABLEGT
	iSuccess = verifyGT(name, errbuf, sigfname, oldsigfname, nsigfname, logfp, sigfp, nsigfp); 
#else
	iSuccess = 0; 
	sprintf(errbuf, "ERROR, unable to perform verify using GuardTime library, rsyslog need to be configured with --enable-guardtime.\n"); 
#endif
	goto done; 

verifyKSI:
#ifdef ENABLEKSI
	iSuccess = verifyKSI(name, errbuf, sigfname, oldsigfname, nsigfname, logfp, sigfp, nsigfp); 
#else
	iSuccess = 0; 
	sprintf(errbuf, "ERROR, unable to perform verify using GuardTime KSI library, rsyslog need to be configured with --enable-gt-ksi.\n"); 
#endif
	goto done; 

err:
done:
	/* Output error if return was 0*/
	if (iSuccess == 0)
		fputs(errbuf, stderr); 
	return;
}

static void
processFile(char *name)
{
	char errbuf[4096];

	switch(mode) {
	case MD_DETECT_FILE_TYPE:
		if(verbose)
			fprintf(stdout, "ProcessMode: Detect Filetype\n"); 
#ifdef ENABLEGT
		if (apimode == API_GT)
			detectFileType(name);
#endif
#ifdef ENABLEKSI
		if (apimode == API_KSI)
			detectFileTypeKSI(name);
#endif
		break;
	case MD_DUMP:
		if(verbose)
			fprintf(stdout, "ProcessMode: Dump FileHashes\n"); 

		if (apimode == API_GT)
#ifdef ENABLEGT
			dumpFile(name);
#else
			fprintf(stderr, "ERROR, unable to perform dump using GuardTime Api, rsyslog need to be configured with --enable-guardtime.\n"); 
#endif
		if (apimode == API_KSI)
#ifdef ENABLEKSI
			dumpFileKSI(name);
#else
			fprintf(stderr, "ERROR, unable to perform dump using GuardTime KSI Api, rsyslog need to be configured with --enable-gt-ksi.\n"); 
#endif
		break;
	case MD_SHOW_SIGBLK_PARAMS:
		if(verbose)
			fprintf(stdout, "ProcessMode: Show SigBlk Params\n"); 
#ifdef ENABLEGT
		if (apimode == API_GT)
			showSigblkParams(name);
#endif
#ifdef ENABLEKSI
		if (apimode == API_KSI)
			showSigblkParamsKSI(name);
#endif
		break;
	case MD_CONVERT:
#ifdef ENABLEGT
		if (apimode == API_GT)
			convertFile(name);
#endif
#ifdef ENABLEKSI
		if (apimode == API_KSI)
			convertFileKSI(name);
#endif
		break;
	case MD_VERIFY:
	case MD_EXTEND:
		if(verbose)
			fprintf(stdout, "ProcessMode: Verify/Extend\n"); 
		verify(name, errbuf);
		break;
	}
}


static struct option long_options[] = 
{ 
	{"help", no_argument, NULL, 'h'},
	{"convert", no_argument, NULL, 'c'},
	{"dump", no_argument, NULL, 'D'},
	{"verbose", no_argument, NULL, 'v'},
	{"debug", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'V'},
	{"detect-file-type", no_argument, NULL, 'T'},
	{"show-sigblock-params", no_argument, NULL, 'B'},
	{"verify", no_argument, NULL, 't'}, /* 't' as in "test signatures" */
	{"extend", no_argument, NULL, 'e'},
	{"show-verified", no_argument, NULL, 's'},
	{"publications-server", required_argument, NULL, 'P'},
	{"api", required_argument, NULL, 'a'},
	{NULL, 0, NULL, 0} 
}; 

/* Helper function to show some HELP */
void 
rsgtutil_usage(void)
{
	fprintf(stderr, "usage: rsgtutil [options]\n"
			"Use \"man rsgtutil\" for more details.\n\n"
			"\t-h, --help \t\t\t Show this help.\n"
			"\t-D, --dump \t\t\t dump operations mode.\n"
			"\t-t, --verify \t\t\t Verify operations mode.\n"
			"\t-e, --extend \t\t\t Extends the RFC3161 signatures.\n"
			"\t-B, --show-sigblock-params \t Show signature block parameters.\n"
			"\t-T, --detect-file-type \t Show Type of signature file.\n"
			"\t-c, --convert \t\t\t Convert Signature Format Version 10 to 11.\n"
			"\t-V, --Version \t\t\t Print utility version\n"
			"\t\tOptional parameters\n"
			"\t-a <GT|KSI>, --api  <GT|KSI> \t Set which API to use.\n"
			"\t\tGT = Guardtime Client Library\n"
			"\t\tKSI = Guardtime KSI Library\n"
			"\t-s, --show-verified \t\t Also show correctly verified blocks.\n"
			"\t-P <URL>, --publications-server <URL> \t Sets the publications server.\n"
			"\t-v, --verbose \t\t\t Verbose output.\n"
			"\t-d, --debug \t\t\t Debug (developer) output.\n"
			);
}

int
main(int argc, char *argv[])
{
	int i;
	int opt;

	while(1) {
		opt = getopt_long(argc, argv, "aBcdDeHPstTvV", long_options, NULL);
		if(opt == -1)
			break;
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		case 'a':
#ifdef ENABLEGT
			if (optarg != NULL && strncasecmp("gt", optarg, 2) == 0)
				apimode = API_GT; 
#endif
#ifdef ENABLEKSI
			if (optarg != NULL && strncasecmp("ksi", optarg, 2) == 0)
				apimode = API_KSI; 
#endif
			if(verbose)
				fprintf(stdout, "Setting Apimode: %s (%d)\n", optarg, apimode); 
			break;
		case 'd':
			debug = 1;
#ifdef ENABLEGT
			rsgt_set_debug(debug); 
#endif
#ifdef ENABLEKSI
			rsksi_set_debug(debug); 
#endif
			break;
		case 's':
#ifdef ENABLEGT
			rsgt_read_showVerified = 1;
#endif
#ifdef ENABLEKSI
			rsksi_read_showVerified = 1;
#endif
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
		case 'P':
#ifdef ENABLEGT
			rsgt_read_puburl = optarg;
#endif
#ifdef ENABLEKSI
			rsksi_read_puburl = optarg;
#endif
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
		case 'c':
			mode = MD_CONVERT;
			break;
		case 'h':
		case '?':
			rsgtutil_usage();
			return 0;
		default:
			fprintf(stderr, "getopt_long() returns unknown value %d\n", opt);
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

