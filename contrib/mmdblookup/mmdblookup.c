/* mmdblookup.c
 * Parse ipaddress field of the message into structured data using
 * MaxMindDB.
 *
 * Copyright 2013 Rao Chenlin.
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

#include "maxminddb.h"

#define JSON_IPLOOKUP_NAME "!iplocation"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmdblookup")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */

typedef struct _instanceData {
     char *pszKey;
     char *pszMmdbFile;
     struct {
          int nmemb;
          uchar **name;
     } fieldList;
} instanceData;

typedef struct wrkrInstanceData {
     instanceData *pData;
     MMDB_s mmdb;
} wrkrInstanceData_t;

struct modConfData_s {
     rsconf_t *pConf;     /* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
     { "key", eCmdHdlrGetWord, 0 },
     { "mmdbfile", eCmdHdlrGetWord, 0 },
     { "fields", eCmdHdlrArray, 0 },
};
static struct cnfparamblk actpblk =
     { CNFPARAMBLK_VERSION,
       sizeof(actpdescr)/sizeof(struct cnfparamdescr),
       actpdescr
     };

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
ENDfreeCnf


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
         int status = MMDB_open(pData->pszMmdbFile, MMDB_MODE_MMAP, &pWrkrData->mmdb);
         if(MMDB_SUCCESS != status) {
             dbgprintf("Can't open %s - %s\n", pData->pszMmdbFile, MMDB_strerror(status));
             if(MMDB_IO_ERROR == status) {
                 dbgprintf("  IO error: %s\n", strerror(errno));
             }
             errmsg.LogError(0, RS_RET_SUSPENDED, "can not initialize maxminddb");
//             ABORT_FINALIZE(RS_RET_SUSPENDED);
         }
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
     MMDB_close(&pWrkrData->mmdb);
ENDfreeWrkrInstance

static inline void
setInstParamDefaults(instanceData *pData)
{
     pData->pszKey = NULL;
     pData->pszMmdbFile = NULL;
     pData->fieldList.nmemb = 0;
}

BEGINnewActInst
     struct cnfparamvals *pvals;
     int i;
CODESTARTnewActInst
     dbgprintf("newActInst (mmdblookup)\n");
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
          if(!strcmp(actpblk.descr[i].name, "key")) {
               pData->pszKey = es_str2cstr(pvals[i].val.d.estr, NULL);
               continue;
          }
          if(!strcmp(actpblk.descr[i].name, "mmdbfile")) {
               pData->pszMmdbFile = es_str2cstr(pvals[i].val.d.estr, NULL);
               continue;
          }
          if(!strcmp(actpblk.descr[i].name, "fields")) {
               pData->fieldList.nmemb = pvals[i].val.d.ar->nmemb;
               CHKmalloc(pData->fieldList.name = malloc(sizeof(uchar*) * pData->fieldList.nmemb));
               for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
                    pData->fieldList.name[j] = (uchar*)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
               }
          }
          dbgprintf("mmdblookup: program error, non-handled "
                 "param '%s'\n", actpblk.descr[i].name);
     }

     if(pData->pszKey == NULL) {
          dbgprintf("mmdblookup: action requires a key");
          ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
     }

     if(pData->pszMmdbFile == NULL) {
          dbgprintf("mmdblookup: action requires a mmdbfile");
          ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
     }

CODE_STD_FINALIZERnewActInst
     cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume
void str_split(char **membuf){
	char *buf  = *membuf;
	char tempbuf[strlen(buf)]  ;
	memset(tempbuf, 0, strlen(buf))	;

	while(*buf++ != '\0'){
		if (*buf == '\n' || *buf == '\t' || *buf == ' '){
			continue;
		}
		else {
			if (*buf == '<'){
				char *p = strchr(buf, '>');
				buf = buf + (int)(p - buf);
				strcat(tempbuf, ",");
			}
			else if( *buf == '}'){
				strcat(tempbuf, "},");
			}
			else{
				strncat(tempbuf, buf, 1);
			}
		}
	}

	tempbuf[strlen(tempbuf) +1 ] = '\n';
	memset(*membuf, 0, strlen(*membuf))	;
	memcpy(*membuf, tempbuf, strlen(tempbuf));
}



BEGINdoAction
     msg_t *pMsg;
     struct json_object *json = NULL;
     struct json_object *keyjson = NULL;
     char *pszValue;
     instanceData *const pData = pWrkrData->pData;
CODESTARTdoAction
     pMsg = (msg_t*) ppString[0];

     json = json_object_new_object(); 

     /* key is given, so get the property json */
     msgPropDescr_t pProp;
     msgPropDescrFill(&pProp, (uchar*)pData->pszKey, strlen(pData->pszKey));
     rsRetVal localRet = msgGetJSONPropJSON(pMsg, &pProp, &keyjson);
     msgPropDescrDestruct(&pProp);
	
     if(localRet != RS_RET_OK) {
	  /* key not found in the message. nothing to do */
	  ABORT_FINALIZE(RS_RET_OK);
     }
     /* key found, so get the value */
     pszValue = (char*)json_object_get_string(keyjson);


     int gai_err, mmdb_err;
     MMDB_lookup_result_s result = MMDB_lookup_string(&pWrkrData->mmdb, pszValue, &gai_err, &mmdb_err);

     if(0 != gai_err) {
		dbgprintf("Error from call to getaddrinfo for %s - %s\n", pszValue, gai_strerror(gai_err));
		dbgprintf("aaaaa\n");
		ABORT_FINALIZE(RS_RET_OK);
     }
     if(MMDB_SUCCESS != mmdb_err) {
		dbgprintf("Got an error from the maxminddb library: %s\n", MMDB_strerror(mmdb_err));
		dbgprintf("bbbbb\n");
		ABORT_FINALIZE(RS_RET_OK);
     }

	MMDB_entry_data_list_s *entry_data_list = NULL;	
	int status  = MMDB_get_entry_data_list(&result.entry, &entry_data_list);
	
	if (MMDB_SUCCESS != status){
		dbgprintf("Got an error looking up the entry data - %s\n", MMDB_strerror(status));
		ABORT_FINALIZE(RS_RET_OK);
	}

	FILE *memstream;
	char *membuf;
	size_t memlen;
	memstream = open_memstream(&membuf, &memlen);

	if (entry_data_list != NULL && memstream != NULL){
		MMDB_dump_entry_data_list(memstream, entry_data_list, 2);
		fflush(memstream);
		str_split(&membuf);
	}

	json_object *total_json = json_tokener_parse(membuf);
	fclose(memstream);
	
	if (pData->fieldList.nmemb < 1){
		dbgprintf("fieldList.name is empty!...\n");
		ABORT_FINALIZE(RS_RET_OK);
	}

	for (int i = 0 ; i <  pData->fieldList.nmemb ; ++i){
		char buf[(strlen((char *)(pData->fieldList.name[i])))+1];
		memset(buf, 0, sizeof(buf));
		strcpy(buf, (char *)pData->fieldList.name[i]);

		struct json_object *json1[5] = {NULL};
		json_object *temp_json = total_json;
		int j = 0;
		char *path[10] = {NULL};	
		char *sep = "!";
		
		char *s = strtok(buf, sep);
		for (; s != NULL; j++){
			path[j] = s;
			s = strtok(NULL, sep);
			
			json_object *sub_obj = json_object_object_get(temp_json, path[j]);
			temp_json = sub_obj;
		}

		j--;
		for (;j >= 0 ;j--){
			if (j > 0){			
     			json1[j] = json_object_new_object(); 
				json_object_object_add(json1[j], path[j], temp_json);
				temp_json = json1[j];
			}

			else {
				json_object_object_add(json, path[j], temp_json);
			}
		}

	}

finalize_it:

     if(json) {
          msgAddJSON(pMsg, (uchar *)JSON_IPLOOKUP_NAME, json, 0, 0);
     }
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
     if(strncmp((char*) p, ":mmdblookup:", sizeof(":mmdblookup:") - 1)) {
          errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
               "mmdblookup supports only v6+ config format, use: "
               "action(type=\"mmdblookup\" ...)");
     }
     ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
     objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt



BEGINmodInit()
CODESTARTmodInit
     *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
     dbgprintf("mmdblookup: module compiled with rsyslog version %s.\n", VERSION);
     CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit

