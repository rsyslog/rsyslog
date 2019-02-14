/* imbatchreport.c
 *
 * This is the input module for reading full text file data. A text file is a
 * non-binary file who's lines are delimited by the \n character. The file is
 * treated as a single message. An optional structured data can be written at
 * the end of the file.
 *
 * No state file are used as it should only grow with time. Instead the state
 * is managed using the name of the file. A "glob" allows the module to identify
 * "to be treated" files. The module can be configured either to deleted the
 * the file on success either to rename the file on success. The size limit is
 * fixed by rsyslog max message size global parameter. All files big than this
 * limit produce a message which references it as "too big" and its new location
 * The "too big" files are also renamed to keep them available.
 *
 * This modules allows to centralize batch reports with other standard logs and
 * performance monitoring in a single repository (ElasticSearch, HDFS, ...). This
 * centralization helps to identify cause of global performance issues.
 *
 * Work originally begun on 2014-07-01 by Philippe Duveau @ Pari Mutuel Urbain
 *
 * This file is contribution of rsyslog.
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>		/* do NOT remove: will soon be done by the module generation macros */
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <fnmatch.h>
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include "rsyslog.h"		/* error codes etc... */
#include "dirty.h"
#include "cfsysline.h"		/* access to config file objects */
#include "module-template.h"	/* generic module interface code - very important, read it! */
#include "srUtils.h"		/* some utility functions */
#include "msg.h"
#include "errmsg.h"
#include "glbl.h"
#include "datetime.h"
#include "unicode-helper.h"
#include "prop.h"
#include "stringbuf.h"
#include "ruleset.h"
#include "ratelimit.h"

#include <regex.h>

MODULE_TYPE_INPUT	/* must be present for input modules, do not remove */
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imbatchreport")

/* defines */

/* Module static data */
DEF_IMOD_STATIC_DATA	/* must be present, starts static data */
DEFobjCurrIf(glbl)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(datetime)
DEFobjCurrIf(prop)


/* static int bLegacyCnfModGlobalsPermitted;/ * are legacy module-global config parameters permitted? */
#define BUFFER_MINSIZE 150
#define FILE_TOO_BIG "File too big !"
#define FILE_TOO_BIG_LEN 14

#define NUM_MULTISUB 1024 /* default max number of submits */
#define DFLT_PollInterval 10

#define INIT_FILE_TAB_SIZE 4 /* default file table size - is extended as needed, use 2^x value */
#define INIT_FILE_IN_DIR_TAB_SIZE 1 /* initial size for "associated files tab" in directory table */
#define INIT_WDMAP_TAB_SIZE 1 /* default wdMap table size - is extended as needed, use 2^x value */

#define ADD_METADATA_UNSPECIFIED -1

typedef enum action_batchreport_t {
	action_nothing,
	action_rename,
	action_delete
} action_batchreport_t;

struct instanceConf_s {
	uchar *pszFollow_glob;
	uchar *pszDirName;
	uchar *pszFileBaseName;
	uchar *pszTag;
	int must_stop;
	int lenTag;
	uchar *pszBindRuleset;
	int iFacility;
	int iSeverity;
	char *ff_regex; /* Full treatment : this should contain a regex applied on filename. The matching
						part is then replaced with ff_replace to put the file out of scan criteria */
	regex_t ff_preg;

	char *ff_rename;
	int len_rename;

	char *ff_reject;
	int len_reject;

	int filename_oversize;
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */

	ratelimit_t *ratelimiter;

	struct instanceConf_s *next;

	action_batchreport_t action;

	char *pszNewFName;
	int fd;

	sbool goon;
};

/* config variables */
typedef struct myConfData_s {
	char *hostname;
	size_t lhostname;

	size_t msg_size;

	instanceConf_t *root, *tail;

	uchar *buffer;
	uchar *buffer_minsize;
	int iPollInterval;  /* number of seconds to sleep when there was no file activity */
} myConfData_t;

static myConfData_t fixedModConf;/* modConf ptr to use for the current load process */

/* config variables */
struct modConfData_s {
	rsconf_t *pConf; /* our overall config object */
};

static prop_t *pInputName = NULL;
/* there is only one global inputName for all messages generated by this input */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "pollinginterval", eCmdHdlrPositiveInt, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
		sizeof(modpdescr)/sizeof(struct cnfparamdescr),
		modpdescr
	};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
	{ "file", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "tag", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "rename", eCmdHdlrString, 0 },
	{ "delete", eCmdHdlrString, 0 },
	{ "severity", eCmdHdlrSeverity, 0 },
	{ "facility", eCmdHdlrFacility, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
		sizeof(inppdescr)/sizeof(struct cnfparamdescr),
		inppdescr
	};

#include "im-helper.h" /* must be included AFTER the type definitions! */

/* enqueue the read file line as a message. The provided string is
 * not freed - thuis must be done by the caller.
 */
static rsRetVal send_msg(instanceConf_t *const __restrict__ pInst,
						uchar *const __restrict__ cstr, size_t clen,
						struct timeval *recvts, time_t gents,
						uchar *structuredData, int structuredDataLen,
						uchar *prog, int progLen,
						uchar *pszNewFname)
{
	DEFiRet;
	smsg_t *pMsg;
	struct syslogTime st;

	if(clen == 0) {
		FINALIZE;
	}

	datetime.timeval2syslogTime(recvts, &st, TIME_IN_LOCALTIME);

	CHKiRet(msgConstructWithTime(&pMsg, &st, gents));
	MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY);
	MsgSetInputName(pMsg, pInputName);
	MsgSetRawMsg(pMsg, (char*)cstr, clen);
	MsgSetMSGoffs(pMsg, 0);
	MsgSetHOSTNAME(pMsg, (uchar*)fixedModConf.hostname, (const int) fixedModConf.lhostname);
	MsgSetTAG(pMsg, pInst->pszTag, pInst->lenTag);

	MsgSetStructuredData(pMsg, "");
	if (structuredData && structuredDataLen)
		MsgAddToStructuredData(pMsg, structuredData, structuredDataLen);

	pMsg->iFacility = pInst->iFacility >> 3;
	pMsg->iSeverity = pInst->iSeverity;

	cstrConstruct(&pMsg->pCSPROCID);
	rsCStrAppendStrWithLen(pMsg->pCSPROCID, prog, progLen);

	MsgSetRuleset(pMsg, pInst->pBindRuleset);

	msgAddMetadata(pMsg, (uchar*)"filename", pszNewFname);

	CHKiRet(ratelimitAddMsg(pInst->ratelimiter, NULL, pMsg));

finalize_it:
	RETiRet;
}

/* The following is a cancel cleanup handler for strmReadLine(). It is necessary in case
 * strmReadLine() is cancelled while processing the stream. -- rgerhards, 2008-03-27
 */
static void pollFileCancelCleanup(void *pArg)
{
	instanceConf_t *ppInst = (instanceConf_t*) pArg;
	if (ppInst->fd > 0)
		close(ppInst->fd);
	if (ppInst->pszNewFName)
		free(ppInst->pszNewFName);
}

/* poll a file, need to check file rollover etc. open file if not open */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wempty-body"
#endif
static rsRetVal pollFile(instanceConf_t *pInst)
{
	DEFiRet;
	uchar *structured_data = NULL, *start;
	int structured_data_len = 0;
	int toolarge;
	pInst->fd = 0;

	/* Note: we must do pthread_cleanup_push() immediately, because the POXIS macros
	 * otherwise do not work if I include the _cleanup_pop() inside an if... -- rgerhards, 2008-08-14
	 */
	pthread_cleanup_push(pollFileCancelCleanup, pInst);

	/* loop below will be exited when strmReadLine() returns EOF */
	if (glbl.GetGlobalInputTermState() == 0 && pInst->goon) {
		glob_t res;
		struct stat fstate;

		DBGPRINTF("polling files : %s\n", pInst->pszFollow_glob);

		if (!glob((char*)pInst->pszFollow_glob, GLOB_NOSORT, 0, &res))
		{
			for (size_t i = 0; i < res.gl_pathc && glbl.GetGlobalInputTermState() == 0; i++)
			{
				char *pszCurrFName = res.gl_pathv[i];
				DBGPRINTF("imbatchreport : File found '%s')\n",pszCurrFName);

				toolarge = 0;

				if (!stat(pszCurrFName, &fstate) && S_ISREG(fstate.st_mode) )
				{
					regmatch_t matches[1];

					if (regexec(&pInst->ff_preg, pszCurrFName, 1, matches, 0))
					{ /* as this test was already made on glob's file search it 
						* must not be possible but let's check it anyway */
						LogError(0, RS_RET_INVALID_PARAMS, 
								"imbatchreport: regex does not match to filename: Aborting module to avoid loops.");
						pInst->must_stop = TRUE;

						globfree(&res);
						return RS_RET_INVALID_PARAMS;
					}

					pInst->fd = open(pszCurrFName, O_NOCTTY | O_RDONLY | O_NONBLOCK | O_LARGEFILE, 0);

					if (pInst->fd > 0)
					{
						size_t file_len, file_in_buffer, msg_len;
						char *filename;

						uchar *local_program = (uchar*)"-";
						size_t local_program_len = 1;

						time_t start_ts = fstate.st_ctim.tv_sec;

						file_len = lseek(pInst->fd, 0, SEEK_END);

						filename = strrchr(pszCurrFName, '/');
						if (filename)
							filename++;
						else
							filename = pszCurrFName;

						/* First read the end of the file to get Structured Data */

						msg_len = (file_len < BUFFER_MINSIZE) ? file_len : BUFFER_MINSIZE;

						lseek(pInst->fd, -msg_len, SEEK_END);

						file_in_buffer = read(pInst->fd, fixedModConf.buffer_minsize, msg_len);
						DBGPRINTF("imbatchreport : file end read %ld %ld\n", file_in_buffer, msg_len);


						if (file_in_buffer == msg_len)
						{
							for (; msg_len && fixedModConf.buffer_minsize[msg_len-1] == '\n'; msg_len--)
								;

							if (fixedModConf.buffer_minsize[msg_len-1] == ']')
							{ /* We read the last part of the report to get structured data */

								for (structured_data = fixedModConf.buffer_minsize + msg_len, structured_data_len = 0;
											structured_data >= fixedModConf.buffer_minsize && *structured_data != '[';
											structured_data--, structured_data_len++)
									;

								if (*structured_data == '[')
								{
									uchar *struct_field = (uchar*)strstr((char*)structured_data, "START="), v;
									if (struct_field)
									{
										start_ts = 0;
										for (struct_field += 7; (v = *struct_field - (uchar)'0') <= 9; struct_field++ )
											start_ts = start_ts*10 + v;

									}

									struct_field = (uchar*)strstr((char*)structured_data, "KSH=\"");
									if (struct_field)
									{
										local_program = struct_field + 5;
										struct_field = (uchar*)strstr((char*)local_program, "\"");
										if (struct_field)
											local_program_len = struct_field - local_program;
										else
											local_program = (uchar*)"-";
									}

									msg_len -= structured_data_len;
								}
								else
								{
									structured_data = NULL;
									structured_data_len = 0;
								}
							} /* if (fixedModConf.buffer_minsize[msg_len -1] == ']') */

							if (file_len > fixedModConf.msg_size)
							{
								if ((size_t)(structured_data_len + FILE_TOO_BIG_LEN) > fixedModConf.msg_size)
								{
									LogError(0, RS_RET_INVALID_PARAMS, "pollFile : the msg_size options (%lu) is"
											"really too small even to handle batch reports notification of %ld "
											"octets (file too large)", fixedModConf.msg_size,
											60 + fixedModConf.lhostname + pInst->lenTag  + structured_data_len);
									pInst->must_stop = 1;

									close(pInst->fd);
									pInst->fd = -1;

									globfree(&res);
									return RS_RET_INVALID_PARAMS;
								}

								LogError(0, RS_RET_INVALID_PARAMS,
											"pollFile : the batch report is too large (rejecting) filename=%s,"
											"max_size=%lu, msg_size=%ld",
											pszCurrFName,fixedModConf.msg_size, file_len);

								toolarge = 1;

								start = (uchar*)FILE_TOO_BIG;

								msg_len = FILE_TOO_BIG_LEN;
							}
							else
							{
								if (file_len > file_in_buffer)
								{
									size_t file_to_read = file_len - file_in_buffer, buffer_used = 0, r;
									start = fixedModConf.buffer_minsize - file_to_read;

									lseek(pInst->fd, 0, SEEK_SET);

									do
									{
										r = read(pInst->fd, start + buffer_used, file_to_read - buffer_used);
										buffer_used += r;
									}
									while (r && file_to_read > buffer_used);

									msg_len += file_to_read;

								}
								else
									start = fixedModConf.buffer_minsize;

								for (uchar*p=start+msg_len; p>=start; p--)
									if (!*p) *p = ' ';
							}

							close(pInst->fd);
							pInst->fd = 0;

							struct timeval tv;

							tv.tv_sec = fstate.st_mtime;
							tv.tv_usec = fstate.st_mtim.tv_nsec / 1000;

							if (send_msg(pInst, start, (start[msg_len]=='\n') ? msg_len : msg_len+1,
									&tv, start_ts, structured_data, structured_data_len,
									local_program, local_program_len,
									(uchar*)pszCurrFName) == RS_RET_OK)
							{
								if (pInst->action == action_rename || toolarge)
								{
									char *pszNewFName = (char*)malloc(strlen(pszCurrFName)+pInst->filename_oversize);
									memcpy(pszNewFName, pszCurrFName, matches[0].rm_so);
									strcpy((char*)pszNewFName + matches[0].rm_so,
												 (toolarge) ? pInst->ff_reject : pInst->ff_rename);

									if (rename(pszCurrFName, pszNewFName))
									{
										LogError(0, RS_RET_STREAM_DISABLED,
												"imbatchreport: module stops because it was"
												" unable to rename form %s to %s.",
												pszCurrFName , pszNewFName);
										pInst->must_stop = 1;
									}
									else
										DBGPRINTF("imbatchreport : file %s sent and renamed to %s.\n", 
													pszCurrFName, pszNewFName);
									free(pszNewFName);
								}
								else
								{
									if (unlink(pszCurrFName))
									{
										LogError(0, RS_RET_STREAM_DISABLED,
												"imbatchreport: module stops because it was unable to delete %s.",
												pszCurrFName);
										pInst->must_stop = 1;
									}
									else
										DBGPRINTF("imbatchreport : file %s sent and deleted\n", pszCurrFName);
								}
							}
						} /* if (file_in_buffer == msg_len) */
						else
						{ /* if (file_in_buffer == msg_len) */
							/* failed to read end of file so rename it */
							close(pInst->fd);
							pInst->fd = 0;

							char *pszNewFName = (char*)malloc(strlen(pszCurrFName)+pInst->filename_oversize);
							memcpy(pszNewFName, pszCurrFName, matches[0].rm_so);
							strcpy((char*)pszNewFName + matches[0].rm_so, pInst->ff_reject);

							if (rename(pszCurrFName, pszNewFName))
							{
								LogError(0, RS_RET_STREAM_DISABLED,
										"imbatchreport: module stops because it was unable to rename form %s to %s.",
										pszCurrFName , pszNewFName);
								pInst->must_stop = 1;
							}
							else
								DBGPRINTF("imbatchreport : file %s renamed to %s due to error while reading it.\n", 
											pszCurrFName, pszNewFName);
							free(pszNewFName);

						} /* if (file_in_buffer == msg_len) */
					} /* if (pInst->fd > 0) */
				} /* if stat */
			} /* for */
		} /* glob */
		globfree(&res);
	}

//finalize_it:
	pthread_cleanup_pop(0);

	RETiRet;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/* create input instance, set default paramters, and
 * add it to the list of instances.
 */
static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;

	*pinst = NULL;

	CHKmalloc(inst = (instanceConf_t*)malloc(sizeof(instanceConf_t)));

	inst->next = NULL;
	inst->pBindRuleset = NULL;

	inst->pszBindRuleset = NULL;
	inst->pszFollow_glob = NULL;
	inst->pszTag = NULL;
	inst->iSeverity = LOG_NOTICE;
	inst->iFacility = LOG_LOCAL0;
	inst->ff_regex = NULL;
	inst->ff_rename = NULL;
	inst->len_rename = 0;
	inst->ff_reject = NULL;
	inst->len_reject = 0;
	inst->goon = 0;
	inst->ratelimiter = NULL;

	inst->action = action_nothing;

	inst->must_stop = 0;

	if(fixedModConf.tail == NULL) {
		fixedModConf.tail = fixedModConf.root = inst;
	} else {
		fixedModConf.tail->next = inst;
		fixedModConf.tail = inst;
	}

	*pinst = inst;
finalize_it:
	RETiRet;
}

/* the basen(ame) buffer must be of size MAXFNAME
 * returns the index of the slash in front of basename
 */
static int getBasename(uchar *const __restrict__ basen, uchar *const __restrict__ path)
{
	int i;
	const int lenName = ustrlen(path);
	for(i = lenName ; i >= 0 ; --i) {
		if(path[i] == '/') {

			if(i == lenName)
				basen[0] = '\0';
			else {
				memcpy(basen, path+i+1, lenName-i);
			}
			break;
		}
	}
	return i;
}

/* this function checks instance parameters and does some required pre-processing
 * (e.g. split filename in path and actual name)
 * Note: we do NOT use dirname()/basename() as they have portability problems.
 */
static rsRetVal checkInstance(instanceConf_t *inst)
{
	char dirn[MAXFNAME];
	uchar basen[MAXFNAME];
	int i;
	struct stat sb;
	int r;
	int eno;
	char errStr[512];
	DEFiRet;

	i = getBasename(basen, inst->pszFollow_glob);
	memcpy(dirn, inst->pszFollow_glob, i);
	dirn[i] = '\0';
	CHKmalloc(inst->pszFileBaseName = (uchar*) strdup((char*)basen));
	CHKmalloc(inst->pszDirName = (uchar*) strdup(dirn));

	if(dirn[0] == '\0') {
		dirn[0] = '/';
		dirn[1] = '\0';
	}
	r = stat(dirn, &sb);
	if(r != 0)  {
		eno = errno;
		rs_strerror_r(eno, errStr, sizeof(errStr));
		LogError(0, RS_RET_CONFIG_ERROR, "imbatchreport warning: directory '%s': %s",
				dirn, errStr);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}
	if(!S_ISDIR(sb.st_mode)) {
		LogError(0, RS_RET_CONFIG_ERROR, "imbatchreport warning: configured directory "
				"'%s' is NOT a directory", dirn);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	dbgprintf("imbatchreport: instance checked");

finalize_it:
	RETiRet;
}

BEGINnewInpInst
	struct cnfparamvals *pvals;
	instanceConf_t *inst;
	int i;
	char *temp;
CODESTARTnewInpInst
	DBGPRINTF("newInpInst (imbatchreport)\n");

	pvals = nvlstGetParams(lst, &inppblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("input param blk in imbatchreport:\n");
		cnfparamsPrint(&inppblk, pvals);
	}

	CHKiRet(createInstance(&inst));

	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(inppblk.descr[i].name, "file")) {
			inst->pszFollow_glob = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "tag")) {
			inst->pszTag = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			inst->lenTag = ustrlen(inst->pszTag);
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "severity")) {
			inst->iSeverity = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "facility")) {
			inst->iFacility = pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "rename")) {
			if (inst->action == action_delete)
			{
				LogError(0, RS_RET_PARAM_ERROR,
					"'rename' and 'delete' are exclusive !");
				ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}

			inst->ff_regex = es_str2cstr(pvals[i].val.d.estr, NULL);

			inst->len_reject = 0;

			if ((inst->ff_rename = strchr(inst->ff_regex, ' ')) != NULL ) {
				*inst->ff_rename++ = '\0';
				if ((inst->ff_reject = strchr(inst->ff_rename, ' ')) != NULL ) {

					*inst->ff_reject++ = '\0';
					temp = strchr(inst->ff_reject, ' ');
					if (temp) *temp = '\0';

					if (strcmp(inst->ff_rename, "-")){
					  inst->ff_rename = strdup(inst->ff_rename);
					  inst->len_rename = strlen(inst->ff_rename);
					}else{
					  inst->ff_rename = strdup("");
					  inst->len_rename = 0;
					}

					inst->ff_reject = strdup(inst->ff_reject);
					inst->len_reject = strlen(inst->ff_reject);

					if (inst->len_reject && regcomp(&inst->ff_preg, (char*)inst->ff_regex, REG_EXTENDED))
					{
					  inst->len_reject = 0;
					  LogError(0, RS_RET_SYNTAX_ERROR,
							"The first part of 'rename' parameter does not contain a valid regex");
							ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
					}
				}
			}
			if (inst->len_reject == 0)
			{
				LogError(0, RS_RET_PARAM_ERROR,
					"'rename' must specify THREE parameters separated by exactly ONE space ! The second "
					"parameter can be a null string to get this use a '-'.");
					ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}

			inst->action = action_rename;

		} else if(!strcmp(inppblk.descr[i].name, "delete")) {
			if (inst->action == action_rename)
			{
				LogError(0, RS_RET_PARAM_ERROR,
					"'rename' and 'delete' are exclusive !");
					ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}

			inst->ff_regex = es_str2cstr(pvals[i].val.d.estr, NULL);

			inst->len_reject = 0;

			if ((inst->ff_reject = strchr(inst->ff_regex, ' ')) != NULL ) {
				*inst->ff_reject++ = '\0';

				temp = strchr(inst->ff_reject, ' ');
				if (temp) *temp = '\0';

				inst->ff_reject = strdup(inst->ff_reject);
				inst->len_reject = strlen(inst->ff_reject);

				if (inst->len_reject && regcomp(&inst->ff_preg, (char*)inst->ff_regex, REG_EXTENDED))
				{
					inst->len_reject = 0;
					LogError(0, RS_RET_SYNTAX_ERROR,
							"The first part of 'delete' parameter does not contain a valid regex");
					  ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
				}

			}

			if (inst->len_reject == 0)
			{
				LogError(0, RS_RET_PARAM_ERROR,
					"'delete' must specify TWO parameters separated by exactly ONE space !");
					ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}
			inst->action = action_delete;

		} else {
			dbgprintf("imbatchreport: program error, non-handled "
				"param '%s'\n", inppblk.descr[i].name);
		}
	}

	if(inst->action == action_nothing) {
		LogError(0, RS_RET_PARAM_NOT_PERMITTED,
			"either 'rename' or 'delete' must be set");
			ABORT_FINALIZE(RS_RET_PARAM_NOT_PERMITTED);
	}

	inst->filename_oversize = (inst->len_rename > inst->len_reject) ? inst->len_rename : inst->len_reject;

	CHKiRet(ratelimitNew(&inst->ratelimiter, "imbatchreport", (char*)inst->pszFollow_glob));

	inst->goon = 1;

	CHKiRet(checkInstance(inst));
finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad

	pModConf->pConf = pConf;
	fixedModConf.iPollInterval = DFLT_PollInterval;

ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "imbatchreport: error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for imbatchreport:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "pollinginterval")) {
			fixedModConf.iPollInterval = (int) pvals[i].val.d.n;
		} else {
			dbgprintf("imbatchreport: program error, non-handled "
				"param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}
	fixedModConf.hostname = strdup((char*)glbl.GetLocalHostName());
	fixedModConf.lhostname = strlen(fixedModConf.hostname);
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
	dbgprintf("imbatchreport: polling interval is %d\n",
			fixedModConf.iPollInterval);
ENDendCnfLoad


BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	for(inst = fixedModConf.root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
	}
	if(fixedModConf.root == NULL) {
		LogError(0, RS_RET_NO_LISTNERS,
				"imbatchreport: no files configured to be monitored - "
				"no input will be gathered");
		iRet = RS_RET_NO_LISTNERS;
	}
ENDcheckCnf

/* note: we do access files AFTER we have dropped privileges. This is
 * intentional, user must make sure the files have the right permissions.
 */
BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf

BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf
	for(inst = fixedModConf.root ; inst != NULL ; ) {
		if (inst->pszBindRuleset) free(inst->pszBindRuleset);
		if (inst->pszFollow_glob) free(inst->pszFollow_glob);
		if (inst->pszDirName) free(inst->pszDirName);
		if (inst->pszFileBaseName) free(inst->pszFileBaseName);
		if (inst->pszTag) free(inst->pszTag);

		if (inst->len_reject) regfree(&inst->ff_preg);

		if (inst->ff_regex) free(inst->ff_regex);
		if (inst->ff_rename) free(inst->ff_rename);
		if (inst->ff_reject) free(inst->ff_reject);

		if (inst->ratelimiter) ratelimitDestruct(inst->ratelimiter);
		del = inst;
		inst = inst->next;
		free(del);
	}
	if (fixedModConf.hostname) {
		free(fixedModConf.hostname);
		fixedModConf.hostname = NULL;
	}
ENDfreeCnf

/* This function is called by the framework to gather the input. The module stays
 * most of its lifetime inside this function. It MUST NEVER exit this function. Doing
 * so would end module processing and rsyslog would NOT reschedule the module. If
 * you exit from this function, you violate the interface specification!
 */
BEGINrunInput
CODESTARTrunInput
	while(glbl.GetGlobalInputTermState() == 0) {
		instanceConf_t *pInst;
		for(pInst = fixedModConf.root ; pInst != NULL ; pInst = pInst->next) {
			if (pInst->goon) {
				if(glbl.GetGlobalInputTermState() == 1)
					break;
				pollFile(pInst);
			}
		}

		if(glbl.GetGlobalInputTermState() == 0)
			srSleep(fixedModConf.iPollInterval, 10);
	}
	DBGPRINTF("imbatchreport: terminating upon request of rsyslog core\n");
	RETiRet;
ENDrunInput

/* The function is called by rsyslog before runInput() is called. It is a last chance
 * to set up anything specific. Most importantly, it can be used to tell rsyslog if the
 * input shall run or not. The idea is that if some config settings (or similiar things)
 * are not OK, the input can tell rsyslog it will not execute. To do so, return
 * RS_RET_NO_RUN or a specific error code. If RS_RET_OK is returned, rsyslog will
 * proceed and call the runInput() entry point.
 */
BEGINwillRun
CODESTARTwillRun
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imbatchreport"), sizeof("imbatchreport") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));

	fixedModConf.msg_size = glbl.GetMaxLine();

	size_t alloc_s = ((fixedModConf.msg_size > BUFFER_MINSIZE) ? fixedModConf.msg_size : BUFFER_MINSIZE) + 1;

	CHKmalloc(fixedModConf.buffer = (uchar*)malloc(alloc_s));

	fixedModConf.buffer_minsize = &fixedModConf.buffer[alloc_s - (BUFFER_MINSIZE+1)];

	fixedModConf.buffer_minsize[BUFFER_MINSIZE] = '\0';
finalize_it:
ENDwillRun

/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 */
BEGINafterRun
CODESTARTafterRun
	if (fixedModConf.buffer) free(fixedModConf.buffer);
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/* The following entry points are defined in module-template.h.
 * In general, they need to be present, but you do NOT need to provide
 * any code here.
 */
BEGINmodExit
CODESTARTmodExit

	objRelease(datetime, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);

ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


/* modInit() is called once the module is loaded. It must perform all module-wide
 * initialization tasks. There are also a number of housekeeping tasks that the
 * framework requires. These are handled by the macros. Please note that the
 * complexity of processing is depending on the actual module. However, only
 * thing absolutely necessary should be done here. Actual app-level processing
 * is to be performed in runInput(). A good sample of what to do here may be to
 * set some variable defaults.
 */
BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
ENDmodInit


static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	LogError(0, NO_ERRCODE, "imbatchreport: ruleset '%s' for %s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->pszFollow_glob);
}
