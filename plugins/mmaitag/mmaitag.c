/* mmaitag.c
 * AI-based message classification plugin.
 * Stores classification tag in a message variable.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "module-template.h"
#include "msg.h"
#include "cfsysline.h"
#include "ai_provider.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmaitag")

DEF_OMOD_STATIC_DATA

static struct cnfparamdescr actpdescr[] = {
	{ "provider", eCmdHdlrString, 0 },
	{ "tag", eCmdHdlrString, 0 },
	{ "model", eCmdHdlrString, 0 },
	{ "prompt", eCmdHdlrString, 0 },
	{ "inputproperty", eCmdHdlrString, 0 },
	{ "apikey", eCmdHdlrString, 0 },
	{ "apikey_file", eCmdHdlrString, 0 }
};
static struct cnfparamblk actpblk = {
	CNFPARAMBLK_VERSION,
	sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	actpdescr
};

typedef struct _instanceData {
	char *provider_name;
	char *tag;
	char *model;
	char *prompt;
	msgPropDescr_t *inputProp;
	char *apikey;
	char *apikey_file;
	ai_provider_t *provider;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;

static ai_provider_t *get_provider(const char *name)
{
	if(!strcmp(name, "gemini"))
			return &gemini_provider;
	if(!strcmp(name, "gemini_mock"))
		return &gemini_mock_provider;
	return NULL;
}

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->provider_name);
	free(pData->tag);
	free(pData->model);
	free(pData->prompt);
	free(pData->apikey);
	free(pData->apikey_file);
	if(pData->inputProp) {
	msgPropDescrDestruct(pData->inputProp);
	free(pData->inputProp);
	}
ENDfreeInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	pWrkrData->pData = pData;
ENDcreateWrkrInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("mmaitag provider=%s tag=%s\n", pData->provider_name, pData->tag);
ENDdbgPrintInstInfo

BEGINtryResume
CODESTARTtryResume
ENDtryResume

static void
setInstParamDefaults(instanceData *pData)
{
	pData->provider_name = strdup("gemini");
	pData->tag = strdup(".aitag");
	pData->model = strdup("gemini-1.5-pro");
	pData->prompt = strdup(
	"You are a tool that classifies log messages. In the next prompts, "
	"log messages are given one by one. Classify them with a single word "
	"whichever fits best. No other output. Classifications are 'NOISE', "
	"'REGULAR', 'IMPORTANT', 'CRITICAL'.");
	pData->inputProp = NULL;
	pData->apikey = NULL;
	pData->apikey_file = NULL;
	pData->provider = NULL;
}

BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
	instanceData *const pData = pWrkrData->pData;
	uchar *val;
	rs_size_t len;
	unsigned short freeBuf = 0;
	char *text;
	char **tags = NULL;
	DEFiRet;
CODESTARTdoAction
	if(pData->inputProp == NULL) {
	getRawMsg(pMsg, &val, &len);
	} else {
	val = MsgGetProp(pMsg, NULL, pData->inputProp, &len, &freeBuf, NULL);
	}
	text = strndup((char*)val, len);
	if(freeBuf)
	free(val);
	if(pData->provider == NULL) {
	pData->provider = get_provider(pData->provider_name);
	if(pData->provider && pData->provider->init)
	    pData->provider->init(pData->provider, pData->model, pData->apikey, pData->prompt);
	}
	if(pData->provider && pData->provider->classify)
	pData->provider->classify(pData->provider, (const char**)&text, 1, &tags);
	if(tags && tags[0]) {
	struct json_object *j = json_object_new_string(tags[0]);
	msgAddJSON(pMsg, (uchar*)pData->tag, j, 0, 0);
	free(tags[0]);
	}
	free(tags);
	free(text);
ENDdoAction

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
	ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	CODE_STD_STRING_REQUESTnewActInst(1)
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);
	for(i = 0 ; i < actpblk.nParams ; ++i) {
	if(!pvals[i].bUsed)
	    continue;
	if(!strcmp(actpblk.descr[i].name, "provider")) {
	    free(pData->provider_name);
	    pData->provider_name = es_str2cstr(pvals[i].val.d.estr, NULL);
	} else if(!strcmp(actpblk.descr[i].name, "tag")) {
	    free(pData->tag);
	    pData->tag = es_str2cstr(pvals[i].val.d.estr, NULL);
	    if(pData->tag[0] == '$')
	        memmove(pData->tag, pData->tag+1, strlen(pData->tag));
	} else if(!strcmp(actpblk.descr[i].name, "model")) {
	    free(pData->model);
	    pData->model = es_str2cstr(pvals[i].val.d.estr, NULL);
	} else if(!strcmp(actpblk.descr[i].name, "prompt")) {
	    free(pData->prompt);
	    pData->prompt = es_str2cstr(pvals[i].val.d.estr, NULL);
	} else if(!strcmp(actpblk.descr[i].name, "inputproperty")) {
	    char *c = es_str2cstr(pvals[i].val.d.estr, NULL);
	    CHKmalloc(pData->inputProp = malloc(sizeof(msgPropDescr_t)));
	    CHKiRet(msgPropDescrFill(pData->inputProp, (uchar*)c, strlen(c)));
	    free(c);
	} else if(!strcmp(actpblk.descr[i].name, "apikey")) {
	    free(pData->apikey);
	    pData->apikey = es_str2cstr(pvals[i].val.d.estr, NULL);
	} else if(!strcmp(actpblk.descr[i].name, "apikey_file")) {
	    free(pData->apikey_file);
	    pData->apikey_file = es_str2cstr(pvals[i].val.d.estr, NULL);
	}
	}
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

NO_LEGACY_CONF_parseSelectorAct

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit

BEGINmodExit
CODESTARTmodExit
ENDmodExit
