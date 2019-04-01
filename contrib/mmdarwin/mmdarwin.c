/// \file     mmdarwin.c
/// \authors  gcatto
/// \version  1.0
/// \date     08/11/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "parserif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "protocol.h" /* custom file written for Darwin */

#define JSON_IPLOOKUP_NAME "!srcip"
#define JSON_LOOKUP_NAME "!mmdarwin"
#define INVLD_SOCK -1
#define INITIAL_BUFFER_SIZE 32

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmdarwin")

DEFobjCurrIf(glbl)
DEF_OMOD_STATIC_DATA

/* config variables */
typedef struct _instanceData {
	char *pCertitudeKey; /* the key name to save in the enriched log line the certitude obtained from Darwin */
	uchar *pSockName; /* the socket path of the filter which will be used by Darwin */
	int sock; /* the socket of the filter which will be used by Darwin */
	unsigned long long int filterCode; /* the filter code associated to the filter which will be used by Darwin */
	enum darwin_filter_response_type response; /* the type of response for Darwin: no / back / darwin / both */
	struct sockaddr_un addr; /* the sockaddr_un used to connect to the Darwin filter */
	struct {
		int    nmemb;
		char **name;
		char **varname;
	} fieldList; /* our keys (fields) to be extracted from the JSON-parsed log line */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

struct modConfData_s {
	/* our overall config object */
	rsconf_t *pConf;
	const char *container;
};

static pthread_mutex_t mutDoAct = PTHREAD_MUTEX_INITIALIZER;

/* modConf ptr to use for the current load process */
static modConfData_t *loadModConf = NULL;
/* modConf ptr to use for the current exec process */
static modConfData_t *runModConf  = NULL;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "container", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* tables for interfacing with the v6 config system
 * action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "key",      eCmdHdlrGetWord, CNFPARAM_REQUIRED },
	{ "socketpath", eCmdHdlrGetWord, CNFPARAM_REQUIRED },
	{ "fields",   eCmdHdlrArray,   CNFPARAM_REQUIRED },
	{ "filtercode", eCmdHdlrGetWord, 0 }, /* optional parameter */
	{ "response", eCmdHdlrGetWord, 0 }, /* optional parameter */
};
static struct cnfparamblk actpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	actpdescr
};


/* custom functions */
#define min(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

static rsRetVal openSocket(instanceData *pData);
static rsRetVal closeSocket(instanceData *pData);
static rsRetVal doTryResume(instanceData *pData);

static rsRetVal sendMsg(instanceData *pData, void *msg, size_t len);
static rsRetVal receiveMsg(instanceData *pData, void *response, size_t len);

rsRetVal get_json_body(smsg_t **ppMsg, struct json_object **ppBody, char *pPropertyString,
					   size_t propertySize);
int get_json_string(char **ppString, struct json_object *pBody);

/* open socket to remote system
 */
static rsRetVal openSocket(instanceData *pData) {
	DEFiRet;
	assert(pData->sock == INVLD_SOCK);

	if ((pData->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		char errStr[1024];
		int eno = errno;
		DBGPRINTF("mmdarwin::openSocket:: error %d creating AF_UNIX/SOCK_STREAM: %s.\n",
				  eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		pData->sock = INVLD_SOCK;
		ABORT_FINALIZE(RS_RET_NO_SOCKET);
	}

	memset(&pData->addr, 0, sizeof(struct sockaddr_un));
	pData->addr.sun_family = AF_UNIX;
	strncpy(pData->addr.sun_path, (char *)pData->pSockName, sizeof(pData->addr.sun_path) - 1);

	dbgprintf("mmdarwin::openSocket:: connecting to Darwin...\n");

	if (connect(pData->sock, (struct sockaddr *)&pData->addr, sizeof(struct sockaddr_un)) == -1) {
		char errStr[1024];
		int eno = errno;
		DBGPRINTF("mmdarwin::openSocket:: error %d connecting to Darwin: %s.\n",
				  eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		pData->sock = INVLD_SOCK;
		ABORT_FINALIZE(RS_RET_NO_SOCKET);
	}

finalize_it:
	if (iRet != RS_RET_OK) {
		closeSocket(pData);
	}
	RETiRet;
}

/* close socket to remote system
 */
static rsRetVal closeSocket(instanceData *pData) {
	DEFiRet;
	if (pData->sock != INVLD_SOCK) {
		if (close(pData->sock) != 0) {
			char errStr[1024];
			int eno = errno;
			DBGPRINTF("mmdarwin::closeSocket:: error %d closing the socket: %s.\n",
				eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
		pData->sock = INVLD_SOCK;
	}
	RETiRet;
}


/* try to resume connection if it is not ready
 */
static rsRetVal doTryResume(instanceData *pData) {
	DEFiRet;

	DBGPRINTF("mmdarwin::doTryResume:: trying to resume\n");
	closeSocket(pData);
	iRet = openSocket(pData);

	if (iRet != RS_RET_OK) {
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}

/* send a message via TCP
 * inspired by rgehards, 2007-12-20
 */
static rsRetVal sendMsg(instanceData *pData, void *msg, size_t len) {
	DEFiRet;

	dbgprintf("mmdarwin::sendMsg:: sending message to Darwin...\n");

	if (pData->sock == INVLD_SOCK) {
		CHKiRet(doTryResume(pData));
	}

	if (pData->sock != INVLD_SOCK) {
		if (send(pData->sock, msg, len, 0) == -1) {
			int eno = errno;
			char errStr[1024];
			DBGPRINTF("mmdarwin suspending: send(), socket %d, error: %d = %s.\n",
				pData->sock, eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
	}

finalize_it:
	RETiRet;
}

/* receive a message via TCP
 * inspired by rgehards, 2007-12-20
 */
static rsRetVal receiveMsg(instanceData *pData, void *response, size_t len) {
	DEFiRet;

	dbgprintf("mmdarwin::receiveMsg:: receiving message from Darwin...\n");

	if (pData->sock == INVLD_SOCK) {
		CHKiRet(doTryResume(pData));
	}

	if (pData->sock != INVLD_SOCK) {
		if (recv(pData->sock, response, len, MSG_WAITALL) <= 0) {
			int eno = errno;
			char errStr[1024];
			DBGPRINTF("mmdarwin::receiveMsg:: suspending: recv(), socket %d, error: %d = %s.\n",
				pData->sock, eno, rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
	}

finalize_it:
	RETiRet;
}

/* retrieve a json object from a stringified json
*/
rsRetVal get_json_body(smsg_t **ppMsg, struct json_object **ppBody, char *pPropertyString,
					   size_t propertySize) {
	dbgprintf("mmdarwin::get_json_body:: getting key '%s' of size %zu in the message '%s'\n", pPropertyString,
			  propertySize, getMSG(*ppMsg));

	/* ... getting the JSON object... */
	msgPropDescr_t propertyDescr; /* the property description */
	/* we fill the description... */
	msgPropDescrFill(&propertyDescr, (uchar*)pPropertyString, propertySize);
	/* ... then get the string related to our field... */
	rsRetVal localRet = msgGetJSONPropJSON(*ppMsg, &propertyDescr, ppBody);
	msgPropDescrDestruct(&propertyDescr);

	return localRet;
}

/* stringify a json object
*/
int get_json_string(char **ppString, struct json_object *pBody) {
	*ppString = (char*)json_object_get_string(pBody);

	if (*ppString == NULL) { /* json null object returns NULL! */
		dbgprintf("mmdarwin::get_json_string:: empty body retrieved. Aborting\n");

		return 0;
	}

	return 1;
}

BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
ENDbeginCnfLoad

BEGINendCnfLoad
CODESTARTendCnfLoad
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free((void*)runModConf->container);
ENDfreeCnf

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	DBGPRINTF("%s\n", pData->pSockName);
ENDdbgPrintInstInfo

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	if (pData->fieldList.name != NULL) {
		for (int i = 0; i < pData->fieldList.nmemb; ++i) {
			free(pData->fieldList.name[i]);
			free(pData->fieldList.varname[i]);
		}
		free(pData->fieldList.name);
		free(pData->fieldList.varname);
	}
	free(pData->pCertitudeKey);
	free(pData->pSockName);
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	loadModConf->container = NULL;
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if (pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS,
				 "mmdarwin: error processing module config parameters missing [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	if (Debug) {
		dbgprintf("mmdarwin::setModCnf:: module (global) param blk for mmdarwin:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for (i = 0; i < modpblk.nParams; ++i) {
		if (!pvals[i].bUsed)
			continue;
		if (!strcmp(modpblk.descr[i].name, "container")) {
			loadModConf->container = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("mmdarwin::setModCnf:: program error, non-handled "
					  "param '%s'\n", modpblk.descr[i].name);
		}
	}

	if (loadModConf->container == NULL) {
		CHKmalloc(loadModConf->container = strdup(JSON_IPLOOKUP_NAME));
	}

finalize_it:
	if (pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

static inline void setInstParamDefaults(instanceData *pData) {
	DBGPRINTF("mmdarwin::setInstParamDefaults::\n");
	pData->pCertitudeKey = NULL;
	pData->pSockName = NULL;
	pData->fieldList.nmemb = 0;
	pData->sock = INVLD_SOCK;
	pData->filterCode = DARWIN_FILTER_CODE_NO;
	pData->response = DARWIN_RESPONSE_SEND_BACK;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	dbgprintf("mmdarwin::newActInst::\n");
	if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CODE_STD_STRING_REQUESTnewActInst(1)
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for (i = 0; i < actpblk.nParams; ++i) {
		if (!pvals[i].bUsed)
			continue;

		if (!strcmp(actpblk.descr[i].name, "key")) {
			pData->pCertitudeKey = es_str2cstr(pvals[i].val.d.estr, NULL);

		} else if (!strcmp(actpblk.descr[i].name, "socketpath")) {
			pData->pSockName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);

		}  else if (!strcmp(actpblk.descr[i].name, "response")) {
			char *response = es_str2cstr(pvals[i].val.d.estr, NULL);

			if (!strcmp(response, "no") != 0) {
				pData->response = DARWIN_RESPONSE_SEND_NO;
			} else if (!strcmp(response, "back") != 0) {
				pData->response = DARWIN_RESPONSE_SEND_BACK;
			} else if (!strcmp(response, "darwin") != 0) {
				pData->response = DARWIN_RESPONSE_SEND_DARWIN;
			} else if (!strcmp(response, "both") != 0) {
				pData->response = DARWIN_RESPONSE_SEND_BOTH;
			} else {
				dbgprintf("mmdarwin::newActInst:: invalid 'response' value: %s. 'No response' set.\n", response);

				pData->response = DARWIN_RESPONSE_SEND_NO;
			}

			free(response);

		} else if (!strcmp(actpblk.descr[i].name, "filtercode")) {
			char *filterCode = es_str2cstr(pvals[i].val.d.estr, NULL);
			pData->filterCode = strtoull(filterCode, NULL, 16);
			free(filterCode);

		} else if (!strcmp(actpblk.descr[i].name, "fields")) {
			pData->fieldList.nmemb = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->fieldList.name = calloc(pData->fieldList.nmemb, sizeof(char *)));
			CHKmalloc(pData->fieldList.varname = calloc(pData->fieldList.nmemb, sizeof(char *)));

			for (int j = 0; j <  pvals[i].val.d.ar->nmemb; ++j) {
				char *const param = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				char *varname = NULL;
				char *name;
				if (*param == ':') {
					char *b = strchr(param+1, ':');
					if (b == NULL) {
						parser_errmsg("mmdarwin::newActInst:: missing closing colon: '%s'", param);
						ABORT_FINALIZE(RS_RET_ERR);
					}

					*b = '\0'; /* split name & varname */
					varname = param+1;
					name = b+1;
				} else {
					name = param;
				}
				CHKmalloc(pData->fieldList.name[j] = strdup(name));
				char vnamebuf[1024];
				snprintf(vnamebuf, sizeof(vnamebuf),
					"%s!%s", loadModConf->container,
					(varname == NULL) ? name : varname);
				CHKmalloc(pData->fieldList.varname[j] = strdup(vnamebuf));
				free(param);
			}

		} else {
			dbgprintf("mmdarwin::newActInst:: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINtryResume
CODESTARTtryResume
	iRet = doTryResume(pWrkrData->pData);
ENDtryResume

/* here is the "main" function of the plugin, called everytime a line is processed */
/* first, we initialize our parameters */
BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData; /* the raw data */
	smsg_t *pMsg = ppMsg[0]; /* the raw log line */
	struct json_object *pFieldBody = NULL; /* the body of the current field processed */
	char *pFieldValueString = NULL; /* the string converted body of the current field processed */
	instanceData *pData = pWrkrData->pData; /* the parameters given for the plugin */
	size_t headerSize; /* the size of the header sent to Darwin */
	size_t bodySize = 0; /* the size of the body sent to Darwin */
	size_t responseSize; /* the size of the response received from Darwin */
	int maxSize = glbl.GetMaxLine(); /* this variable used when setting the size of the packet sent */
	rsRetVal localRet; /* this variable is used when checking an error from a Rsyslog function */
	size_t bufferSize = INITIAL_BUFFER_SIZE; /* our buffer size for temporary strings */
	size_t bufferBodySize = INITIAL_BUFFER_SIZE; /* our buffer size for the body to be sent to Darwin */
	char *stringBuffer = NULL; /* our variable for temporary strings */
	size_t fieldSize = 0; /* the size we check for the buffer */

/* then, we process our log line */
CODESTARTdoAction
	dbgprintf("mmdarwin::doAction:: processing log line: '%s'\n", getMSG(pMsg));

	stringBuffer = malloc(sizeof(char) * bufferSize);
	size_t bodyElementsNumber = pData->fieldList.nmemb;
	char *body = malloc(sizeof(char) * bufferBodySize * bodyElementsNumber + 1); /* + 1 for '\0' */

	if (stringBuffer == NULL) {
		dbgprintf("mmdarwin::doAction:: error: something went wrong while allocating stringBuffer\n");
		goto finalize_it;
	}

	*stringBuffer = '\0';

	dbgprintf("mmdarwin::doAction:: starting sending fields to Darwin\n");

	if (body == NULL) {
		dbgprintf("mmdarwin::doAction:: error: something went wrong while allocating body\n");
		goto finalize_it;
	}

	*body = '\0';

	char *currentBodyIndex = body;

	/* the Darwin header to be sent to the filter */
	darwin_filter_packet_t header = {
		.type = DARWIN_PACKET_OTHER,
		.response = pData->response,
		.filter_code = pData->filterCode,
		.certitude = 0,
		.ip_type = DARWIN_IP_UNKNOWN,
	};

	/* for each field, we add the value in the body */
	for (int i = 0; i <  pData->fieldList.nmemb; ++i) {
		dbgprintf("mmdarwin::doAction:: processing field '%s'\n", pData->fieldList.name[i]);
		fieldSize = strlen(pData->fieldList.name[i]);

		/* case 1: static field. We simply forward it to Darwin */
		if (fieldSize > 0 && pData->fieldList.name[i][0] != '!') {
			pFieldValueString = pData->fieldList.name[i];
		/* case 2: dynamic field. We retrieve its value from the JSON logline and forward it to Darwin */
		} else {
			if (bufferSize < fieldSize) {
				dbgprintf("mmdarwin::doAction:: reallocating stringBuffer. Current size is %zu. Size needed is %zu\n",
						  bufferSize, fieldSize);

				while (bufferSize < fieldSize) bufferSize += INITIAL_BUFFER_SIZE;

				char *tmpStringBuffer = NULL;

				if ((tmpStringBuffer = realloc(stringBuffer, bufferSize * sizeof(char)))) {
					stringBuffer = tmpStringBuffer;
				} else {
					dbgprintf("mmdarwin::doAction:: error: something went wrong while reallocating stringBuffer\n");
					/* stringBuffer is still allocated, but we will free it later */
					goto finalize_it;
				}
			}

			/* the actual property size: field + '\0' = strlen(field) + 1 */
			size_t propertySize = min(fieldSize, bufferSize);
			stpncpy(stringBuffer, pData->fieldList.name[i], fieldSize);
			*(stringBuffer + fieldSize) = '\0';
			localRet = get_json_body(&pMsg, &pFieldBody, stringBuffer, propertySize);

			if (localRet != RS_RET_OK) {
				/* for some reason, we couldn't get the value.
				   So we consider the size to be zero and we continue the processing */
				dbgprintf("mmdarwin::doAction:: could not extract the json body from the message. "
						  "rsRetVal error code: %d\n", localRet);
				header.body_elements_sizes[i] = 0;
				goto end_processing_field;
			}
			if (get_json_string(&pFieldValueString, pFieldBody) == 0) {
				/* for some really weird reason, we couldn't convert our JSON object to a string.
				   So we consider the size to be zero and we continue the processing */
				dbgprintf("mmdarwin::doAction:: could not parse the json body to string\n");
				header.body_elements_sizes[i] = 0;
				goto end_processing_field;
			}
		}

		/* pFieldValueString will be freed when its JSON object will be dereferenced */
		dbgprintf("mmdarwin::doAction:: value retrieved from field '%s': '%s'\n", pData->fieldList.name[i],
				  pFieldValueString);

		/* we set the size for this value... */
		header.body_elements_sizes[i] = strlen(pFieldValueString);

		if (bodySize + header.body_elements_sizes[i] >= bufferBodySize) {
			while (bodySize + header.body_elements_sizes[i] >= bufferBodySize) bufferBodySize += INITIAL_BUFFER_SIZE;

			char *tmpStringBuffer = realloc(body, bufferBodySize * sizeof(char) + 1);

			if (tmpStringBuffer) {
				body = tmpStringBuffer;
				currentBodyIndex = body + bodySize;

				if (bodySize != 0) currentBodyIndex++;
			} else {
				dbgprintf("mmdarwin::doAction:: error: something went wrong while reallocating body\n");
				/* body is still allocated, but we will free it later */
				goto finalize_it;
			}
		}

		/* then, we copy it to our body */
		currentBodyIndex = stpncpy(currentBodyIndex, pFieldValueString, strlen(pFieldValueString));
		*currentBodyIndex = '\0';
		bodySize = strlen(body);

		end_processing_field:
			json_object_put(pFieldBody); /* we can free our pFieldBody */
			pFieldBody = NULL;
			dbgprintf("mmdarwin::doAction:: ended processing field '%s'\n", pData->fieldList.name[i]);
	}

	pthread_mutex_lock(&mutDoAct);

	dbgprintf("mmdarwin::doAction:: getting Darwin session\n");
	CHKiRet(doTryResume(pWrkrData->pData)); /* connecting to darwin */

	if ((int) bodySize > maxSize) bodySize = maxSize; /* we truncate, if the message is too long */

	header.body_size = bodySize;
	header.body_elements_number = bodyElementsNumber;
	headerSize = sizeof(darwin_filter_packet_t);

	if ((int) headerSize > maxSize) headerSize = maxSize; /* we truncate, if the message is too long */

	dbgprintf("mmdarwin::doAction:: sending header to Darwin\n");
	CHKiRet(sendMsg(pWrkrData->pData, &header, headerSize));

	dbgprintf("mmdarwin::doAction:: sending body (cookie/header value) to Darwin\n");
	CHKiRet(sendMsg(pWrkrData->pData, body, bodySize));

	/* there is no need to wait for a response that will never come */
	if (pData->response == DARWIN_RESPONSE_SEND_NO || pData->response == DARWIN_RESPONSE_SEND_DARWIN) {
		dbgprintf("mmdarwin::doAction:: no response will be sent back "
				  "(darwin response type is set to 'no' or 'darwin')\n");
		goto finalize_it;
	}

	darwin_filter_packet_t response = {
		.type = DARWIN_PACKET_OTHER,
		.response = DARWIN_RESPONSE_SEND_NO,
		.ip_type = DARWIN_IP_UNKNOWN,
		.filter_code = DARWIN_FILTER_CODE_NO,
		.certitude = 10,
		.body_size = 0,
		.body_elements_number = 0,
		.body_elements_sizes = {0}
	};

	dbgprintf("mmdarwin::doAction:: receiving from Darwin\n");
	responseSize = sizeof(response);

	if ((int) responseSize > maxSize) responseSize = maxSize; /* we truncate, if the message is too long */

	CHKiRet(receiveMsg(pWrkrData->pData, &response, responseSize));

	dbgprintf("mmdarwin::doAction:: end of the transaction\n");
	pthread_mutex_unlock(&mutDoAct);

	/* we cast our certitude to a char* */
	int charProcessed = snprintf(stringBuffer, bufferSize, "%d", response.certitude);

	if (charProcessed < 0 || (unsigned int)charProcessed >= bufferSize) {
		dbgprintf("mmdarwin::doAction:: warning: the certitude was truncated (only %zu characters were written)\n",
				  bufferSize);
	}
	dbgprintf("mmdarwin::doAction:: certitude obtained: %s\n", stringBuffer);

	dbgprintf("mmdarwin::doAction:: adding certitude to message...\n");
	struct json_object *pDarwinJSON = json_object_new_object();
	json_object_object_add(pDarwinJSON, pData->pCertitudeKey, json_object_new_string(stringBuffer));

	msgAddJSON(pMsg, (uchar *)JSON_LOOKUP_NAME, pDarwinJSON, 0, 0);

	finalize_it:
		if (stringBuffer != NULL) {
			free(stringBuffer);
			stringBuffer = NULL;
		}
		if (body != NULL) {
			free(body);
			body = NULL;
		}
		if (pFieldBody != NULL) {
			json_object_put(pFieldBody);
			pFieldBody = NULL;
		}
ENDdoAction


NO_LEGACY_CONF_parseSelectorAct


BEGINmodExit
CODESTARTmodExit
	objRelease(glbl, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	/* we only support the current interface specification */
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	dbgprintf("mmdarwin::modInit:: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDmodInit
