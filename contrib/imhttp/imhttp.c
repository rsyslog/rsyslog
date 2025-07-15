/* imhttp.c
 * This is an input module for receiving http input.
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
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "rsyslog.h"
#include "cfsysline.h"		/* access to config file objects */
#include "module-template.h"
#include "ruleset.h"
#include "unicode-helper.h"
#include "rsyslog.h"
#include "errmsg.h"
#include "statsobj.h"
#include "ratelimit.h"
#include "dirty.h"

#include "civetweb.h"
#include <apr_base64.h>
#include <apr_md5.h>

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imhttp")

/* static data */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(statsobj)

#define CIVETWEB_OPTION_NAME_PORTS         "listening_ports"
#define CIVETWEB_OPTION_NAME_DOCUMENT_ROOT "document_root"
#define MAX_READ_BUFFER_SIZE  16384
#define INIT_SCRATCH_BUF_SIZE 4096
/* General purpose buffer size. */
#define IMHTTP_MAX_BUF_LEN (8192)

struct option {
	const char *name;
	const char *val;
};

struct auth_s {
	char workbuf[IMHTTP_MAX_BUF_LEN];
	char* pworkbuf;
	size_t workbuf_len;
	char* pszUser;
	char* pszPasswd;
};

struct data_parse_s {
	sbool  content_compressed;
	sbool bzInitDone; /* did we do an init of zstrm already? */
	z_stream zstrm;	/* zip stream to use for tcp compression */
	// Currently only used for octet specific parsing
	enum {
		eAtStrtFram,
		eInOctetCnt,
		eInMsg,
	} inputState;
	size_t iOctetsRemain;	/* Number of Octets remaining in message */
	enum {
		TCP_FRAMING_OCTET_STUFFING,
		TCP_FRAMING_OCTET_COUNTING
	} framingMode;
};

struct modConfData_s {
	rsconf_t *pConf;  /* our overall config object */
	instanceConf_t *root, *tail;
	struct option ports;
	struct option docroot;
	struct option *options;
	int nOptions;
};

struct instanceConf_s {
	struct instanceConf_s *next;
	uchar *pszBindRuleset;    /* name of ruleset to bind to */
	uchar *pszEndpoint;       /* endpoint to configure */
	uchar *pszBasicAuthFile;       /* file containing basic auth users/pass */
	ruleset_t *pBindRuleset;  /* ruleset to bind listener to (use system default if unspecified) */
	ratelimit_t *ratelimiter;
	unsigned int ratelimitInterval;
	unsigned int ratelimitBurst;
	uchar *pszInputName;	  /* value for inputname property, NULL is OK and handled by core engine */
	prop_t *pInputName;
	sbool flowControl;
	sbool bDisableLFDelim;
	sbool bSuppOctetFram;
	sbool bAddMetadata;
};

struct conn_wrkr_s {
	struct data_parse_s parseState;
	uchar* pMsg;						/* msg scratch buffer */
	size_t iMsg;						/* index of next char to store in msg */
	uchar zipBuf[64*1024];
	multi_submit_t multiSub;
	smsg_t *pMsgs[CONF_NUM_MULTISUB];
	char *pReadBuf;
	size_t readBufSize;
	prop_t *propRemoteAddr;
	const struct mg_request_info *pri; /* do not free me - used to hold a reference only */
	char *pScratchBuf;
	size_t scratchBufSize;
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */
static prop_t *pInputName = NULL;

//static size_t s_iMaxLine = 16; /* get maximum size we currently support */
static size_t s_iMaxLine = 16384; /* get maximum size we currently support */

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "ports", eCmdHdlrString, 0 },
	{ "documentroot", eCmdHdlrString, 0 },
	{ "liboptions", eCmdHdlrArray, 0 },
};

static struct cnfparamblk modpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	modpdescr
};

static struct cnfparamdescr inppdescr[] = {
	{ "endpoint", eCmdHdlrString, 0},
	{ "basicauthfile", eCmdHdlrString, 0},
	{ "ruleset", eCmdHdlrString, 0 },
	{ "flowcontrol", eCmdHdlrBinary, 0 },
	{ "disablelfdelimiter", eCmdHdlrBinary, 0 },
	{ "supportoctetcountedframing", eCmdHdlrBinary, 0 },
	{ "name", eCmdHdlrString, 0 },
	{ "ratelimit.interval", eCmdHdlrInt, 0 },
	{ "ratelimit.burst", eCmdHdlrInt, 0 },
	{ "addmetadata", eCmdHdlrBinary, 0 }
};

#include "im-helper.h" /* must be included AFTER the type definitions! */

static struct cnfparamblk inppblk = {
	CNFPARAMBLK_VERSION,
	sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	inppdescr
};

static struct {
	statsobj_t *stats;
	STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted)
	STATSCOUNTER_DEF(ctrFailed, mutCtrFailed);
	STATSCOUNTER_DEF(ctrDiscarded, mutCtrDiscarded);
} statsCounter;

#include "im-helper.h" /* must be included AFTER the type definitions! */

#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0
#define EXIT_URI "/exit"
volatile int exitNow = 0;

struct mg_callbacks callbacks;

typedef struct httpserv_s {
	struct mg_context *ctx;
	struct mg_callbacks callbacks;
	const char **civetweb_options;
	size_t civetweb_options_count;
} httpserv_t;

static httpserv_t *s_httpserv;

/* FORWARD DECLARATIONS */
static rsRetVal processData(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len);

static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;
	CHKmalloc(inst = calloc(1, sizeof(instanceConf_t)));
	inst->next = NULL;
	inst->pszBindRuleset = NULL;
	inst->pBindRuleset = NULL;
	inst->pszEndpoint = NULL;
	inst->pszBasicAuthFile = NULL;
	inst->ratelimiter = NULL;
	inst->pszInputName = NULL;
	inst->pInputName = NULL;
	inst->ratelimitBurst = 10000; /* arbitrary high limit */
	inst->ratelimitInterval = 0; /* off */
	inst->flowControl = 1;
	inst->bDisableLFDelim = 0;
	inst->bSuppOctetFram = 0;
	inst->bAddMetadata = 0;
	// construct statsobj

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

	*pinst = inst;
finalize_it:
	RETiRet;
}

static rsRetVal
processCivetwebOptions(char *const param,
	const char **const name,
	const char **const paramval)
{
	DEFiRet;
	char *val = strstr(param, "=");
	if(val == NULL) {
		LogError(0, RS_RET_PARAM_ERROR, "missing equal sign in "
				"parameter '%s'", param);
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}
	*val = '\0'; /* terminates name */
	++val; /* now points to begin of value */
	CHKmalloc(*name = strdup(param));
	CHKmalloc(*paramval = strdup(val));

finalize_it:
	RETiRet;
}

static sbool valid_civetweb_option(const struct mg_option *valid_opts, const char* option)
{
	const struct mg_option *pvalid_opts = valid_opts;
	for (; pvalid_opts != NULL && pvalid_opts->name != NULL; pvalid_opts++) {
		if (strcmp(pvalid_opts->name, option) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

#if 0
static int
log_message(__attribute__((unused)) const struct mg_connection *conn, const char *message)
{
	puts(message);
	return 1;
}
#endif
/*
	*   thread_type:
	*     0 indicates the master thread
	*     1 indicates a worker thread handling client connections
	*     2 indicates an internal helper thread (timer thread)
*/
static void*
init_thread(__attribute__((unused)) const struct mg_context *ctx, int thread_type)
{
	DEFiRet;
	struct conn_wrkr_s *data = NULL;
	if (thread_type == 1) {
		CHKmalloc(data = calloc(1, sizeof(struct conn_wrkr_s)));
		data->pMsg = NULL;
		data->iMsg = 0;
		data->parseState.bzInitDone = 0;
		data->parseState.content_compressed = 0;
		data->parseState.inputState = eAtStrtFram;
		data->parseState.iOctetsRemain = 0;
		data->multiSub.maxElem = CONF_NUM_MULTISUB;
		data->multiSub.ppMsgs = data->pMsgs;
		data->multiSub.nElem = 0;
		data->pReadBuf = malloc(MAX_READ_BUFFER_SIZE);
		data->readBufSize = MAX_READ_BUFFER_SIZE;

		data->parseState.bzInitDone = 0;
		data->parseState.content_compressed = 0;
		data->parseState.inputState = eAtStrtFram;
		data->parseState.iOctetsRemain = 0;

		CHKmalloc(data->pMsg = calloc(1, 1 + s_iMaxLine));
		data->iMsg = 0;
		data->propRemoteAddr = NULL;
		data->pScratchBuf = NULL;
		data->scratchBufSize = 0;
	}

finalize_it:
	if (iRet != RS_RET_OK) {
		free(data);
		return NULL;
	}
	return data;
}

static void
exit_thread(__attribute__((unused)) const struct mg_context *ctx,
	__attribute__((unused)) int thread_type, void *thread_pointer)
{
	if (thread_type == 1) {
		struct conn_wrkr_s *data = (struct conn_wrkr_s *) thread_pointer;
		if (data->propRemoteAddr) {
			prop.Destruct(&data->propRemoteAddr);
		}
		if (data->scratchBufSize) {
			free(data->pScratchBuf);
		}
		free(data->pReadBuf);
		free(data->pMsg);
		free(data);
	}
}

static rsRetVal
msgAddMetadataFromHttpHeader(smsg_t *const __restrict__ pMsg, struct conn_wrkr_s *connWrkr)
{
	struct json_object *json = NULL;
	DEFiRet;
	const struct mg_request_info *ri = connWrkr->pri;
	#define MAX_HTTP_HEADERS 64	/* hard limit */
	int count = min(ri->num_headers, MAX_HTTP_HEADERS);

	CHKmalloc(json = json_object_new_object());
	for (int i = 0 ; i < count ; i++ ) {
		struct json_object *const jval = json_object_new_string(ri->http_headers[i].value);
		CHKmalloc(jval);
		/* truncate header names bigger than INIT_SCRATCH_BUF_SIZE */
		strncpy(connWrkr->pScratchBuf, ri->http_headers[i].name, connWrkr->scratchBufSize - 1);
		/* make header lowercase */
		char* pname = connWrkr->pScratchBuf;
		while (pname && *pname != '\0') {
			*pname = tolower(*pname);
			pname++;
		}
		json_object_object_add(json, (const char *const)connWrkr->pScratchBuf, jval);
	}
	CHKiRet(msgAddJSON(pMsg, (uchar*)"!metadata!httpheaders", json, 0, 0));

finalize_it:
	if (iRet != RS_RET_OK && json) {
		json_object_put(json);
	}
	RETiRet;
}

static rsRetVal
msgAddMetadataFromHttpQueryParams(smsg_t *const __restrict__ pMsg, struct conn_wrkr_s *connWrkr)
{
	struct json_object *json = NULL;
	DEFiRet;
	const struct mg_request_info *ri = connWrkr->pri;

	if (ri && ri->query_string) {
		strncpy(connWrkr->pScratchBuf, ri->query_string, connWrkr->scratchBufSize - 1);
		char *pquery_str = connWrkr->pScratchBuf;
		if (pquery_str) {
			CHKmalloc(json = json_object_new_object());

			char* saveptr = NULL;
			char *kv_pair = strtok_r(pquery_str, "&;", &saveptr);

			for ( ; kv_pair != NULL; kv_pair = strtok_r(NULL, "&;", &saveptr)) {
				char *saveptr2 = NULL;
				char *key = strtok_r(kv_pair, "=", &saveptr2);
				if (key) {
					char *value = strtok_r(NULL, "=", &saveptr2);
					struct json_object *const jval = json_object_new_string(value);
					CHKmalloc(jval);
					json_object_object_add(json, (const char *)key, jval);
				}
			}
			CHKiRet(msgAddJSON(pMsg, (uchar*)"!metadata!queryparams", json, 0, 0));
		}
	}
finalize_it:
	if (iRet != RS_RET_OK && json) {
		json_object_put(json);
	}
	RETiRet;
}

static rsRetVal
doSubmitMsg(const instanceConf_t *const __restrict__ inst,
	struct conn_wrkr_s *connWrkr, const uchar* msg, size_t len)
{
	smsg_t *pMsg;
	DEFiRet;

	assert(len <= s_iMaxLine);
	if (len == 0) {
		DBGPRINTF("discarding zero-sized message\n");
		FINALIZE;
	}

	CHKiRet(msgConstruct(&pMsg));
	MsgSetFlowControlType(pMsg, inst->flowControl
			            ? eFLOWCTL_LIGHT_DELAY : eFLOWCTL_NO_DELAY);
	if (inst->pInputName) {
		MsgSetInputName(pMsg, inst->pInputName);
	} else {
		MsgSetInputName(pMsg, pInputName);
	}
	MsgSetRawMsg(pMsg, (const char*)msg, len);
	MsgSetMSGoffs(pMsg, 0);	/* we do not have a header... */
	if (connWrkr->propRemoteAddr) {
		MsgSetRcvFromIP(pMsg, connWrkr->propRemoteAddr);
	}
	if (inst) {
		MsgSetRuleset(pMsg, inst->pBindRuleset);
	}
	// TODO: make these flags configurable.
	pMsg->msgFlags = NEEDS_PARSING | PARSE_HOSTNAME;

	if (inst->bAddMetadata) {
		CHKiRet(msgAddMetadataFromHttpHeader(pMsg, connWrkr));
		CHKiRet(msgAddMetadataFromHttpQueryParams(pMsg, connWrkr));
	}

	ratelimitAddMsg(inst->ratelimiter, &connWrkr->multiSub, pMsg);
	STATSCOUNTER_INC(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);
finalize_it:
	connWrkr->iMsg = 0;
	if (iRet != RS_RET_OK) {
		STATSCOUNTER_INC(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);
	}
	RETiRet;
}


static rsRetVal
processOctetMsgLen(const instanceConf_t *const inst, struct conn_wrkr_s *connWrkr, char ch)
{
	DEFiRet;
	if (connWrkr->parseState.inputState == eAtStrtFram) {
		if (inst->bSuppOctetFram && isdigit(ch)) {
			connWrkr->parseState.inputState = eInOctetCnt;
			connWrkr->parseState.iOctetsRemain = 0;
			connWrkr->parseState.framingMode = TCP_FRAMING_OCTET_COUNTING;
		} else {
			connWrkr->parseState.inputState = eInMsg;
			connWrkr->parseState.framingMode = TCP_FRAMING_OCTET_STUFFING;
		}
	}

	// parsing character.
	if (connWrkr->parseState.inputState == eInOctetCnt) {
		if (isdigit(ch)) {
			if (connWrkr->parseState.iOctetsRemain <= 200000000) {
				connWrkr->parseState.iOctetsRemain = connWrkr->parseState.iOctetsRemain * 10 + ch - '0';
			}
			// temporarily save this character into the message buffer
			if(connWrkr->iMsg + 1 < s_iMaxLine) {
				connWrkr->pMsg[connWrkr->iMsg++] = ch;
			}
		} else {
			const char *remoteAddr = "";
			if (connWrkr->propRemoteAddr) {
				remoteAddr = (const char *)propGetSzStr(connWrkr->propRemoteAddr);
			}

			/* handle space delimeter */
			if (ch != ' ') {
				LogError(0, NO_ERRCODE, "Framing Error in received TCP message "
					"from peer: (ip) %s: to input: %s, delimiter is not "
					"SP but has ASCII value %d.",
					remoteAddr, inst->pszInputName, ch);
			}

			if (connWrkr->parseState.iOctetsRemain < 1) {
				LogError(0, NO_ERRCODE, "Framing Error in received TCP message"
					" from peer: (ip) %s: delimiter is not "
					"SP but has ASCII value %d.",
					remoteAddr, ch);
			} else if (connWrkr->parseState.iOctetsRemain > s_iMaxLine) {
				DBGPRINTF("truncating message with %lu octets - max msg size is %lu\n",
									connWrkr->parseState.iOctetsRemain, s_iMaxLine);
				LogError(0, NO_ERRCODE, "received oversize message from peer: "
					"(hostname) (ip) %s: size is %lu bytes, max msg "
					"size is %lu, truncating...",
					remoteAddr, connWrkr->parseState.iOctetsRemain, s_iMaxLine);
			}
			connWrkr->parseState.inputState = eInMsg;
		}
		/* reset msg len for actual message processing */
		connWrkr->iMsg = 0;
		/* retrieve next character */
	}
	RETiRet;
}

static rsRetVal
processOctetCounting(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len)
{
	DEFiRet;
	const uchar* pbuf = (const uchar*)buf;
	const uchar* pbufLast = pbuf + len;

	while (pbuf < pbufLast) {
		char ch = *pbuf;

		if (connWrkr->parseState.inputState == eAtStrtFram || connWrkr->parseState.inputState == eInOctetCnt) {
			processOctetMsgLen(inst, connWrkr, ch);
			if (connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_COUNTING) {
				pbuf++;
			}
		} else if (connWrkr->parseState.inputState == eInMsg) {
			if (connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_STUFFING) {
				if (connWrkr->iMsg < s_iMaxLine) {
					if (ch == '\n') {
						doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
						connWrkr->parseState.inputState = eAtStrtFram;
					} else {
						connWrkr->pMsg[connWrkr->iMsg++] = ch;
					}
				} else {
					doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
					connWrkr->parseState.inputState = eAtStrtFram;
				}
				pbuf++;
			} else {
				assert (connWrkr->parseState.framingMode == TCP_FRAMING_OCTET_COUNTING);
				/* parsing payload */
				size_t remainingBytes = pbufLast - pbuf;
				// figure out how much is in block
				size_t count = min (connWrkr->parseState.iOctetsRemain, remainingBytes);
				if (connWrkr->iMsg + count >= s_iMaxLine) {
					count = s_iMaxLine - connWrkr->iMsg;
				}

				// just copy the bytes
				if (count) {
					memcpy(connWrkr->pMsg + connWrkr->iMsg, pbuf, count);
					pbuf += count;
					connWrkr->iMsg += count;
					connWrkr->parseState.iOctetsRemain -= count;
				}

				if (connWrkr->parseState.iOctetsRemain == 0) {
					doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
					connWrkr->parseState.inputState = eAtStrtFram;
				}
			}
		} else {
			// unexpected
			assert(0);
			break;
		}
	}
	RETiRet;
}

static rsRetVal
processDisableLF(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len)
{
	DEFiRet;
	const uchar *pbuf = (const uchar*)buf;
	size_t remainingBytes = len;
	const uchar* pbufLast = pbuf + len;

	while (pbuf < pbufLast) {
		size_t count = 0;
		if (connWrkr->iMsg + remainingBytes >= s_iMaxLine) {
			count = s_iMaxLine - connWrkr->iMsg;
		} else {
			count = remainingBytes;
		}

		if (count) {
			memcpy(connWrkr->pMsg + connWrkr->iMsg, pbuf, count);
			pbuf += count;
			connWrkr->iMsg += count;
			remainingBytes -= count;
		}
		doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
	}
	RETiRet;
}

static rsRetVal
processDataUncompressed(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len)
{
	const uchar *pbuf = (const uchar*)buf;
	DEFiRet;

	if (inst->bDisableLFDelim) {
		/* do block processing */
		iRet = processDisableLF(inst, connWrkr, buf, len);
	} else if (inst->bSuppOctetFram) {
		iRet = processOctetCounting(inst, connWrkr, buf, len);
	} else {
		const uchar* pbufLast = pbuf + len;
		while (pbuf < pbufLast) {
			char ch = *pbuf;
			if (connWrkr->iMsg < s_iMaxLine) {
				if (ch == '\n') {
					doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
				} else {
					connWrkr->pMsg[connWrkr->iMsg++] = ch;
				}
			} else {
				doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
			}
			pbuf++;
		}
	}
	RETiRet;
}

static rsRetVal
processDataCompressed(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len)
{
	DEFiRet;

	if (!connWrkr->parseState.bzInitDone) {
		/* allocate deflate state */
		connWrkr->parseState.zstrm.zalloc = Z_NULL;
		connWrkr->parseState.zstrm.zfree = Z_NULL;
		connWrkr->parseState.zstrm.opaque = Z_NULL;
		int rc = inflateInit2(&connWrkr->parseState.zstrm, (MAX_WBITS | 16));
		if (rc != Z_OK) {
			dbgprintf("imhttp: error %d returned from zlib/inflateInit()\n", rc);
			ABORT_FINALIZE(RS_RET_ZLIB_ERR);
		}
		connWrkr->parseState.bzInitDone = 1;
	}

	connWrkr->parseState.zstrm.next_in = (Bytef*) buf;
	connWrkr->parseState.zstrm.avail_in = len;
	/* run inflate() on buffer until everything has been uncompressed */
	int outtotal = 0;
	do {
		int zRet = 0;
		int outavail = 0;
		dbgprintf("imhttp: in inflate() loop, avail_in %d, total_in %ld\n",
				connWrkr->parseState.zstrm.avail_in, connWrkr->parseState.zstrm.total_in);

		connWrkr->parseState.zstrm.avail_out = sizeof(connWrkr->zipBuf);
		connWrkr->parseState.zstrm.next_out = connWrkr->zipBuf;
		zRet = inflate(&connWrkr->parseState.zstrm, Z_SYNC_FLUSH);
		dbgprintf("imhttp: inflate(), ret: %d, avail_out: %d\n", zRet, connWrkr->parseState.zstrm.avail_out);
		outavail = sizeof(connWrkr->zipBuf) - connWrkr->parseState.zstrm.avail_out;
		if (outavail != 0) {
			outtotal += outavail;
			CHKiRet(processDataUncompressed(inst, connWrkr, (const char*)connWrkr->zipBuf, outavail));
		}
	} while (connWrkr->parseState.zstrm.avail_out == 0);

	dbgprintf("imhttp: processDataCompressed complete, sizes: in %lld, out %llu\n", (long long) len,
		(long long unsigned) outtotal);

finalize_it:
	RETiRet;
}

static rsRetVal
processData(const instanceConf_t *const inst,
	struct conn_wrkr_s *connWrkr, const char* buf, size_t len)
{
	DEFiRet;

	//inst->bDisableLFDelim = 0;
	if (connWrkr->parseState.content_compressed) {
		iRet = processDataCompressed(inst, connWrkr, buf, len);
	} else {
		iRet = processDataUncompressed(inst, connWrkr, buf, len);
	}

	RETiRet;
}

/* Return 1 on success. Always initializes the auth structure. */
static int
parse_auth_header(struct mg_connection *conn, struct auth_s *auth)
{
	if (!auth || !conn) {
		return 0;
	}

	const char *auth_header = NULL;
	if (((auth_header = mg_get_header(conn, "Authorization")) == NULL) ||
			strncasecmp(auth_header, "Basic ", 6) != 0) {
		return 0;
	}

	/* Parse authorization header */
	const char* src = auth_header + 6;
	size_t len = apr_base64_decode_len((const char*)src);
	auth->pworkbuf = auth->workbuf;
	if (len > sizeof(auth->workbuf)) {
		auth->pworkbuf = calloc(0, len);
		auth->workbuf_len = len;
	}
	len = apr_base64_decode(auth->pworkbuf, src);
	if (len == 0) {
		return 0;
	}

	char *passwd = NULL, *saveptr = NULL;
	char *user = strtok_r(auth->pworkbuf, ":", &saveptr);
	if (user) {
		passwd = strtok_r(NULL, ":", &saveptr);
	}

	auth->pszUser = user;
	auth->pszPasswd = passwd;

	return 1;
}

static int
read_auth_file(FILE* filep, struct auth_s *auth)
{
	if (!filep) {
		return 0;
	}
	char workbuf[IMHTTP_MAX_BUF_LEN];
	size_t l = 0;
	char* user;
	char* passwd;

	while (fgets(workbuf, sizeof(workbuf), filep)) {
		l = strnlen(workbuf, sizeof(workbuf));
		while (l > 0) {
			if (isspace(workbuf[l-1]) || iscntrl(workbuf[l-1])) {
				l--;
				workbuf[l] = 0;
			} else {
				break;
			}
		}

		if (l < 1) {
			continue;
		}

		if (workbuf[0] == '#') {
			continue;
		}

		user = workbuf;
		passwd = strchr(workbuf, ':');
		if (!passwd) {
			continue;
		}
		*passwd = '\0';
		passwd++;

		if (!strcasecmp(auth->pszUser, user)) {
			return (apr_password_validate(auth->pszPasswd, passwd) == APR_SUCCESS);
		}
	}
	return 0;
}

/* Authorize against the opened passwords file. Return 1 if authorized. */
static int
authorize(struct mg_connection* conn, FILE* filep)
{
	if (!conn || !filep) {
		return 0;
	}

	struct auth_s auth = { .workbuf_len=0, .pworkbuf=NULL, .pszUser=NULL, .pszPasswd=NULL};
	if (!parse_auth_header(conn, &auth)) {
		return 0;
	}

	/* validate against htpasswd file */
	return read_auth_file(filep, &auth);
}

/* Provides Basic Authorization handling that validates against a 'htpasswd' file.
	see also: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Authorization
*/
static int
basicAuthHandler(struct mg_connection *conn, void *cbdata)
{
	const instanceConf_t* inst = (const instanceConf_t*) cbdata;
	char errStr[512];
	FILE *fp = NULL;
	int ret = 1;

	if (!inst->pszBasicAuthFile) {
		mg_cry(conn, "warning: 'BasicAuthFile' not configured.\n");
		ret = 0;
		goto finalize;
	}

	fp = fopen((const char *)inst->pszBasicAuthFile, "r");
	if (fp == NULL) {
		if (strerror_r(errno, errStr, sizeof(errStr)) == 0) {
			mg_cry(conn,
					"error: 'BasicAuthFile' file '%s' could not be accessed: %s\n",
					inst->pszBasicAuthFile, errStr);
		} else {
			mg_cry(conn,
					"error: 'BasicAuthFile' file '%s' could not be accessed: %d\n",
					inst->pszBasicAuthFile, errno);
		}
		ret = 0;
		goto finalize;
	}

	ret = authorize(conn, fp);

finalize:
	if (!ret) {
		mg_send_http_error(conn, 401, "WWW-Authenticate: Basic realm=\"User Visible Realm\"\n");
	}
	if (fp ) {
		fclose(fp);
	}
	return ret;
}

/* cbdata should actually contain instance data and we can actually use this instance data
 * to hold reusable scratch buffer.
 */
static int
postHandler(struct mg_connection *conn, void *cbdata)
{
	int rc = 1;
	instanceConf_t* inst = (instanceConf_t*) cbdata;
	const struct mg_request_info *ri = mg_get_request_info(conn);
	struct conn_wrkr_s *connWrkr = mg_get_thread_pointer(conn);
	connWrkr->multiSub.nElem = 0;
	memset(&connWrkr->parseState, 0, sizeof(connWrkr->parseState));
	connWrkr->pri = ri;

	if (inst->bAddMetadata && connWrkr->scratchBufSize == 0) {
		connWrkr->pScratchBuf = calloc(1, INIT_SCRATCH_BUF_SIZE);
		if (!connWrkr->pScratchBuf) {
			mg_cry(conn, "%s() - could not alloc scratch buffer!\n", __FUNCTION__);
			rc = 500;
			FINALIZE;
		}
		connWrkr->scratchBufSize = INIT_SCRATCH_BUF_SIZE;
	}

	if (0 != strcmp(ri->request_method, "POST")) {
		/* Not a POST request */
		int ret = mg_get_request_link(conn, connWrkr->pReadBuf, connWrkr->readBufSize);
		mg_printf(conn,
		          "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n");
		mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
		mg_printf(conn,
		          "%s method not allowed in the POST handler\n",
		          ri->request_method);
		if (ret >= 0) {
			mg_printf(conn,
			          "use a web tool to send a POST request to %s\n",
			          connWrkr->pReadBuf);
		}
		STATSCOUNTER_INC(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
		rc = 405;
		FINALIZE;
	}

	if (ri->remote_addr[0] != '\0') {
		size_t len = strnlen(ri->remote_addr, sizeof(ri->remote_addr));
		prop.CreateOrReuseStringProp(&connWrkr->propRemoteAddr, (const uchar*)ri->remote_addr, len);
	}

	if (ri->content_length >= 0) {
		/* We know the content length in advance */
		if (ri->content_length > (long long) connWrkr->readBufSize) {
			connWrkr->pReadBuf = realloc(connWrkr->pReadBuf, ri->content_length+1);
			if (!connWrkr->pReadBuf) {
				mg_cry(conn, "%s() - realloc failed!\n", __FUNCTION__);
				FINALIZE;
			}
			connWrkr->readBufSize = ri->content_length+1;
		}
	} else {
		/* We must read until we find the end (chunked encoding
		 * or connection close), indicated my mg_read returning 0 */
	}

	if (ri->num_headers > 0) {
		int i;
		for (i = 0; i < ri->num_headers; i++) {
			if (!strcasecmp(ri->http_headers[i].name, "content-encoding") &&
					!strcasecmp(ri->http_headers[i].value, "gzip")) {
				connWrkr->parseState.content_compressed = 1;
			}
		}
	}

	while (1) {
		int count = mg_read(conn, connWrkr->pReadBuf, connWrkr->readBufSize);
		if (count > 0) {
			processData(inst, connWrkr, (const char*)connWrkr->pReadBuf, count);
		} else {
			break;
		}
	}

	/* submit remainder */
	doSubmitMsg(inst, connWrkr, connWrkr->pMsg, connWrkr->iMsg);
	multiSubmitFlush(&connWrkr->multiSub);

	mg_send_http_ok(conn, "text/plain", 0);
	rc = 200;

finalize_it:
	if (connWrkr->parseState.bzInitDone) {
		inflateEnd(&connWrkr->parseState.zstrm);
	}
	/* reset */
	connWrkr->iMsg = 0;

	return rc;
}

static int runloop(void)
{
	dbgprintf("imhttp started.\n");

	/* Add handler for form data */
	for(instanceConf_t *inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		assert(inst->pszEndpoint);
		if (inst->pszEndpoint) {
			dbgprintf("setting request handler: '%s'\n", inst->pszEndpoint);
			mg_set_request_handler(s_httpserv->ctx, (char *)inst->pszEndpoint, postHandler, inst);
			if (inst->pszBasicAuthFile) {
				mg_set_auth_handler(s_httpserv->ctx, (char *)inst->pszEndpoint, basicAuthHandler, inst);
			}
		}
	}

	/* Wait until the server should be closed */
	while(glbl.GetGlobalInputTermState() == 0) {
		sleep(1);
	}
	return EXIT_SUCCESS;
}

BEGINnewInpInst
	struct cnfparamvals *pvals;
	instanceConf_t *inst;
	int i;
CODESTARTnewInpInst;
	DBGPRINTF("newInpInst (imhttp)\n");
	pvals = nvlstGetParams(lst, &inppblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS,
			        "imhttp: required parameter are missing\n");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("input param blk in imtcp:\n");
		cnfparamsPrint(&inppblk, pvals);
	}

	CHKiRet(createInstance(&inst));

	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(inppblk.descr[i].name, "endpoint")) {
			inst->pszEndpoint = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "basicauthfile")) {
			inst->pszBasicAuthFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "name")) {
			inst->pszInputName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.burst")) {
			inst->ratelimitBurst = (unsigned int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ratelimit.interval")) {
			inst->ratelimitInterval = (unsigned int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "flowcontrol")) {
			inst->flowControl = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "disablelfdelimiter")) {
			inst->bDisableLFDelim = (int) pvals[i].val.d.n;
		} else if (!strcmp(inppblk.descr[i].name, "supportoctetcountedframing")) {
			inst->bSuppOctetFram = (int) pvals[i].val.d.n;
		} else if (!strcmp(inppblk.descr[i].name, "addmetadata")) {
			inst->bAddMetadata = (int) pvals[i].val.d.n;
		} else {
			dbgprintf("imhttp: program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}

	if (inst->pszInputName) {
		CHKiRet(prop.Construct(&inst->pInputName));
		CHKiRet(prop.SetString(inst->pInputName, inst->pszInputName, ustrlen(inst->pszInputName)));
		CHKiRet(prop.ConstructFinalize(inst->pInputName));
	}
	CHKiRet(ratelimitNew(&inst->ratelimiter, "imphttp", NULL));
	ratelimitSetLinuxLike(inst->ratelimiter, inst->ratelimitInterval, inst->ratelimitBurst);

finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad;
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	loadModConf->ports.name = NULL;
	loadModConf->docroot.name = NULL;
	loadModConf->nOptions = 0;
	loadModConf->options = NULL;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
CODESTARTsetModCnf;
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "imhttp: error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for imhttp:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(int i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "ports")) {
			assert(loadModConf->ports.name == NULL);
			assert(loadModConf->ports.val == NULL);
			loadModConf->ports.name = strdup(CIVETWEB_OPTION_NAME_PORTS);
			loadModConf->ports.val = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(modpblk.descr[i].name, "documentroot")) {
			assert(loadModConf->docroot.name == NULL);
			assert(loadModConf->docroot.val == NULL);
			loadModConf->docroot.name = strdup(CIVETWEB_OPTION_NAME_DOCUMENT_ROOT);
			loadModConf->docroot.val = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(modpblk.descr[i].name, "liboptions")) {
			loadModConf->nOptions = pvals[i].val.d.ar->nmemb;
			CHKmalloc(loadModConf->options = malloc(sizeof(struct option) *
			                                      pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(processCivetwebOptions(cstr, &loadModConf->options[j].name,
					&loadModConf->options[j].val));
				free(cstr);
			}
		} else {
			dbgprintf("imhttp: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}

finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad;
	loadModConf = NULL; /* done loading */
ENDendCnfLoad

/* function to generate error message if framework does not find requested ruleset */
static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	LogError(0, NO_ERRCODE, "imhttp: ruleset '%s' for %s not found - "
			"using default ruleset instead", inst->pszBindRuleset,
			inst->pszEndpoint);
}

BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf;
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		std_checkRuleset(pModConf, inst);
	}
	/* verify civetweb options are valid */
	const struct mg_option *valid_opts = mg_get_valid_options();
	for (int i = 0; i < pModConf->nOptions; ++i) {
		if (!valid_civetweb_option(valid_opts, pModConf->options[i].name)) {
			LogError(0, RS_RET_CONF_PARSE_WARNING, "imhttp: module loaded, but "
			"invalid civetweb option found - imhttp may not receive connections.");
			iRet = RS_RET_CONF_PARSE_WARNING;
		}
	}
ENDcheckCnf


BEGINactivateCnf
CODESTARTactivateCnf;
	runModConf = pModConf;

	if (!s_httpserv) {
		CHKmalloc(s_httpserv = calloc(1, sizeof(httpserv_t)));
	}
	/* options represents (key, value) so allocate 2x, and null terminated */
	size_t count = 1;
	if (runModConf->ports.val) {
		count += 2;
	}
	if (runModConf->docroot.val) {
		count += 2;
	}
	count += (2 * runModConf->nOptions);
	CHKmalloc(s_httpserv->civetweb_options = calloc(count, sizeof(*s_httpserv->civetweb_options)));

	const char **pcivetweb_options = s_httpserv->civetweb_options;
	if (runModConf->nOptions) {
		s_httpserv->civetweb_options_count = count;
		for (int i = 0; i < runModConf->nOptions; ++i) {
			*pcivetweb_options = runModConf->options[i].name;
			pcivetweb_options++;
			*pcivetweb_options = runModConf->options[i].val;
			pcivetweb_options++;
		}
	}
	/* append port, docroot */
	if (runModConf->ports.val) {
		*pcivetweb_options = runModConf->ports.name;
		pcivetweb_options++;
		*pcivetweb_options = runModConf->ports.val;
		pcivetweb_options++;
	}
	if (runModConf->docroot.val) {
		*pcivetweb_options = runModConf->docroot.name;
		pcivetweb_options++;
		*pcivetweb_options = runModConf->docroot.val;
		pcivetweb_options++;
	}

	const char **option = s_httpserv->civetweb_options;
	for (; option && *option != NULL; option++) {
		dbgprintf("imhttp: civetweb option: %s\n", *option);
	}

	CHKiRet(statsobj.Construct(&statsCounter.stats));
	CHKiRet(statsobj.SetName(statsCounter.stats, UCHAR_CONSTANT("imhttp")));
	CHKiRet(statsobj.SetOrigin(statsCounter.stats, UCHAR_CONSTANT("imhttp")));
	STATSCOUNTER_INIT(statsCounter.ctrSubmitted, statsCounter.mutCtrSubmitted);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrSubmitted)));

	STATSCOUNTER_INIT(statsCounter.ctrFailed, statsCounter.mutCtrFailed);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("failed"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrFailed)));

	STATSCOUNTER_INIT(statsCounter.ctrDiscarded, statsCounter.mutCtrDiscarded);
	CHKiRet(statsobj.AddCounter(statsCounter.stats, UCHAR_CONSTANT("discarded"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(statsCounter.ctrDiscarded)));

	CHKiRet(statsobj.ConstructFinalize(statsCounter.stats));

	/* init civetweb libs and start server w/no input */
	mg_init_library(MG_FEATURES_TLS);
	memset(&callbacks, 0, sizeof(callbacks));
	//callbacks.log_message = log_message;
	//callbacks.init_ssl = init_ssl;
	callbacks.init_thread = init_thread;
	callbacks.exit_thread = exit_thread;
	s_httpserv->ctx = mg_start(&callbacks, NULL, s_httpserv->civetweb_options);
	/* Check return value: */
	if (s_httpserv->ctx == NULL) {
		LogError(0, RS_RET_INTERNAL_ERROR, "Cannot start CivetWeb - mg_start failed.\n");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}

	finalize_it:
	if (iRet != RS_RET_OK) {
		free(s_httpserv);
		s_httpserv = NULL;
		LogError(0, NO_ERRCODE, "imhttp: error %d trying to activate configuration", iRet);
	}
	RETiRet;
ENDactivateCnf

BEGINfreeCnf
	instanceConf_t *inst, *del;
CODESTARTfreeCnf;
	for(inst = pModConf->root ; inst != NULL ; ) {
		if (inst->ratelimiter) {
			ratelimitDestruct(inst->ratelimiter);
		}
		if (inst->pInputName) {
			prop.Destruct(&inst->pInputName);
		}
		free(inst->pszEndpoint);
		free(inst->pszBasicAuthFile);
		free(inst->pszBindRuleset);
		free(inst->pszInputName);

		del = inst;
		inst = inst->next;
		free(del);
	}

	for (int i = 0; i < pModConf->nOptions; ++i) {
		free((void*) pModConf->options[i].name);
		free((void*) pModConf->options[i].val);
	}
	free(pModConf->options);

	free((void*)pModConf->ports.name);
	free((void*)pModConf->ports.val);
	free((void*)pModConf->docroot.name);
	free((void*)pModConf->docroot.val);

	if (statsCounter.stats) {
		statsobj.Destruct(&statsCounter.stats);
	}
ENDfreeCnf


/* This function is called to gather input.
 */
BEGINrunInput
CODESTARTrunInput;
	runloop();
ENDrunInput

/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun;
ENDwillRun

BEGINafterRun
CODESTARTafterRun;
	if (s_httpserv) {
		mg_stop(s_httpserv->ctx);
		mg_exit_library();
		free(s_httpserv->civetweb_options);
		free(s_httpserv);
	}
ENDafterRun


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature;
	// if(eFeat == sFEATURENonCancelInputTermination)
	// 	iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINmodExit
CODESTARTmodExit;
	if(pInputName != NULL) {
		prop.Destruct(&pInputName);
	}

	/* release objects we used */
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_IMOD_QUERIES;
CODEqueryEtryPt_STD_CONF2_QUERIES;
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES;
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit;
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imhttp"), sizeof("imhttp") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit
