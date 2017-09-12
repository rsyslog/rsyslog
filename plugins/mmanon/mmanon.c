/* mmanon.c
 * anonnymize IP addresses inside the syslog message part
 *
 * Copyright 2013 Adiscon GmbH.
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
#include "parserif.h"
#include "hashtable.h"



MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmanon")


DEFobjCurrIf(errmsg);
DEF_OMOD_STATIC_DATA

/* config variables */

// enumerator for the mode
enum mode {ZERO, RANDOMINT, SIMPLE};

union node {
	struct {
		union node* more;
		union node* less;
	} pointer;
	struct {
		char ip_high[16];
		char ip_low[16];
	} ips;
};

struct ipv6_int {
	unsigned long long high;
	unsigned long long low;
	};
/* define operation modes we have */
#define SIMPLE_MODE 0	 /* just overwrite */
#define REWRITE_MODE 1	 /* rewrite IP address, canoninized */
typedef struct _instanceData {
	struct {
		sbool enable;
		int8_t bits;
		union node* Root;
		int randConsis;
		enum mode mode;
		uchar replaceChar;
	} ipv4;

	struct {
		sbool enable;
		uint8_t bits;
		enum mode anonmode;
		int randConsis;
		struct hashtable* hash;
	} ipv6;
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	unsigned randstatus;
} wrkrInstanceData_t;

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};
static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "ipv4.enable", eCmdHdlrBinary, 0 },
	{ "ipv4.mode", eCmdHdlrGetWord, 0 },
	{ "mode", eCmdHdlrGetWord, 0 },
	{ "ipv4.bits", eCmdHdlrPositiveInt, 0 },
	{ "ipv4.replacechar", eCmdHdlrGetChar, 0},
	{ "replacementchar", eCmdHdlrGetChar, 0},
	{ "ipv6.enable", eCmdHdlrBinary, 0 },
	{ "ipv6.anonmode", eCmdHdlrGetWord, 0 },
	{ "ipv6.bits", eCmdHdlrPositiveInt, 0 }
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
	pWrkrData->randstatus = time(NULL);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


static void
delTree(union node* node, const int layer)
{
	if(node == NULL){
		return;
	}
	if(layer == 31){
		free(node);
	} else {
		delTree(node->pointer.more, layer + 1);
		delTree(node->pointer.less, layer + 1);
		free(node);
	}
}


BEGINfreeInstance
CODESTARTfreeInstance
	delTree(pData->ipv4.Root, 0);
	if(pData->ipv6.hash != NULL) {
		hashtable_destroy(pData->ipv6.hash, 1);
	}
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


static inline void
setInstParamDefaults(instanceData *pData)
{
		pData->ipv4.enable = 1;
		pData->ipv4.bits = 16;
		pData->ipv4.Root = NULL;
		pData->ipv4.randConsis = 0;
		pData->ipv4.mode = ZERO;
		pData->ipv4.replaceChar = 'x';

		pData->ipv6.enable = 1;
		pData->ipv6.bits = 96;
		pData->ipv6.anonmode = ZERO;
		pData->ipv6.randConsis = 0;
		pData->ipv6.hash = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	DBGPRINTF("newActInst (mmanon)\n");
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
		if(!strcmp(actpblk.descr[i].name, "ipv4.mode") || !strcmp(actpblk.descr[i].name, "mode")) {
			if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"zero",
					 sizeof("zero")-1)) {
				pData->ipv4.mode = ZERO;
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"random",
					 sizeof("random")-1)) {
				pData->ipv4.mode = RANDOMINT;
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"simple",
					 sizeof("simple")-1) ||
					!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"rewrite",
					 sizeof("rewrite")-1)) {
				pData->ipv4.mode = SIMPLE;
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"random-consistent",
					 sizeof("random-consistent")-1)) {
				pData->ipv4.mode = RANDOMINT;
				pData->ipv4.randConsis = 1;
			} else {
				parser_errmsg("mmanon: configuration error, unknown option for ipv4.mode, will use \"zero\"\n");
			}
		} else if(!strcmp(actpblk.descr[i].name, "ipv4.bits")) {
			if((int8_t) pvals[i].val.d.n <= 32) {
				pData->ipv4.bits = (int8_t) pvals[i].val.d.n;
			} else {
				pData->ipv4.bits = 32;
				parser_errmsg("warning: invalid number of ipv4.bits (%d), corrected to 32", (int) pvals[i].val.d.n);
			}
		} else if(!strcmp(actpblk.descr[i].name, "ipv4.enable")) {
			pData->ipv4.enable = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "ipv4.replacechar") || !strcmp(actpblk.descr[i].name, "replacementchar")) {
			uchar* tmp = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
			pData->ipv4.replaceChar = tmp[0];
			free(tmp);
		} else if(!strcmp(actpblk.descr[i].name, "ipv6.enable")) {
			pData->ipv6.enable = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "ipv6.bits")) {
			if((uint8_t) pvals[i].val.d.n <= 128) {
				pData->ipv6.bits = (uint8_t) pvals[i].val.d.n;
			} else {
				pData->ipv6.bits = 128;
				parser_errmsg("warning: invalid number of ipv6.bits (%d), corrected to 128", (int) pvals[i].val.d.n);
			}
		} else if(!strcmp(actpblk.descr[i].name, "ipv6.anonmode")) {
			if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"zero",
					 sizeof("zero")-1)) {
				pData->ipv6.anonmode = ZERO;
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"random",
					 sizeof("random")-1)) {
				pData->ipv6.anonmode = RANDOMINT;
			} else if(!es_strbufcmp(pvals[i].val.d.estr, (uchar*)"random-consistent",
					 sizeof("random-consistent")-1)) {
				pData->ipv6.anonmode = RANDOMINT;
				pData->ipv6.randConsis = 1;
			} else {
				parser_errmsg("mmanon: configuration error, unknown option for ipv6.anonmode, will use \"zero\"\n");
			}
		} else {
			parser_errmsg("mmanon: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	int bHadBitsErr = 0;
	if(pData->ipv4.mode == SIMPLE) {
		if(pData->ipv4.bits < 8 && pData->ipv4.bits > -1) {
			pData->ipv4.bits = 8;
			bHadBitsErr = 1;
		} else if(pData->ipv4.bits < 16 && pData->ipv4.bits > 8) {
			pData->ipv4.bits = 16;
			bHadBitsErr = 1;
		} else if(pData->ipv4.bits < 24 && pData->ipv4.bits > 16) {
			pData->ipv4.bits = 24;
			bHadBitsErr = 1;
		} else if((pData->ipv4.bits != 32 && pData->ipv4.bits > 24) || pData->ipv4.bits < 0) {
			pData->ipv4.bits = 32;
			bHadBitsErr = 1;
		}
		if(bHadBitsErr) {
			LogError(0, RS_RET_INVLD_ANON_BITS,
				"mmanon: invalid number of ipv4 bits "
				"in simple mode, corrected to %d",
				pData->ipv4.bits);
		}
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


static int
getHexVal(char c)
{
	if('0' <= c && c <= '9') {
		return c - '0';
	} else if('a' <= c && c <= 'f') {
		return (c - 'a') + 10;
	} else if('A' <= c && c <= 'F') {
		return (c - 'A') + 10;
	} else {
		return -1;
	}
}


/* returns -1 if no integer found, else integer */
static int64_t
getPosInt(const uchar *const __restrict__ buf,
	const size_t buflen,
	size_t *const __restrict__ nprocessed)
{
	int64_t val = 0;
	size_t i;
	for(i = 0 ; i < buflen ; i++) {
		if('0' <= buf[i] && buf[i] <= '9')
			val = val*10 + buf[i]-'0';
		else
			break;
	}
	*nprocessed = i;
	if(i == 0)
		val = -1;
	return val;
}

/* 1 - is IPv4, 0 not */

static int
syntax_ipv4(const uchar *const __restrict__ buf,
	const size_t buflen,
	size_t *const __restrict__ nprocessed)
{
	int64_t val;
	size_t nproc;
	size_t i;
	int r = 0;

	val = getPosInt(buf, buflen, &i);
	if(val < 0 || val > 255)
		goto done;

	if(i >= buflen || buf[i] != '.') {
		goto done;
	}
	i++;
	val = getPosInt(buf+i, buflen-i, &nproc);
	if(val < 0 || val > 255)
		goto done;
	i += nproc;

	if(i >= buflen || buf[i] != '.') {
		goto done;
	}
	i++;
	val = getPosInt(buf+i, buflen-i, &nproc);
	if(val < 0 || val > 255)
		goto done;
	i += nproc;

	if(i >= buflen || buf[i] != '.') {
		goto done;
	}
	i++;
	val = getPosInt(buf+i, buflen-i, &nproc);
	if(val < 0 || val > 255)
		goto done;
	i += nproc;

	*nprocessed = i;
	r = 1;

done:
	return r;
}


static int
isValidHexNum(const uchar *const __restrict__ buf,
	const size_t buflen,
	size_t *const __restrict__ nprocessed)
{
	size_t idx = 0;
	int cyc = 0;

	while(idx < buflen) {
		switch(buf[idx]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':

		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':

		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			cyc++;
			if(cyc == 5) {
				return 0;
			}
			(*nprocessed)++;
			break;
		case ':':
			if(cyc == 0) {
				(*nprocessed)++;
				return -1;
			}
		default:
			return cyc;
		}
		idx++;
	}
	return cyc;
}


static int
syntax_ipv6(const uchar *const __restrict__ buf,
	const size_t buflen,
	size_t *const __restrict__ nprocessed)
{
	int lastSep = 0;
	sbool hadAbbrev = 0;
	sbool lastAbbrev = 0;
	int ipParts = 0;
	int numLen;
	int isIP = 0;

	while(*nprocessed < buflen) {
		numLen = isValidHexNum(buf + *nprocessed, buflen - *nprocessed, nprocessed);
		if(numLen > 0) {  //found a valid num
			if((ipParts == 7 && hadAbbrev) || ipParts > 7) {
				isIP = 0;
				goto done;
			}
			if (ipParts == 0 && lastSep && !hadAbbrev) {
				isIP = 0;
				goto done;
			}
			lastSep = 0;
			lastAbbrev = 0;
			ipParts++;
		} else if (numLen < 0) {  //':'
			if(lastSep) {
				if(hadAbbrev) {
					isIP = 0;
					goto done;
				} else {
					hadAbbrev = 1;
					lastAbbrev = 1;
				}
			}
			lastSep = 1;
		} else {  //no valid num
			if(lastSep) {
				if(lastAbbrev && ipParts < 8) {
					isIP = 1;
					goto done;
				}
				isIP = 0;
				goto done;
			}
			if((ipParts == 8 && !hadAbbrev) || (ipParts < 8 && hadAbbrev)) {
				isIP = 1;
				goto done;
			} else {
				isIP = 0;
				goto done;
			}
		}
	}

	if((!lastSep && (ipParts == 8 && !hadAbbrev)) || (ipParts < 8 && hadAbbrev)) {
		isIP = 1;
	}

done:
	return isIP;
}


static unsigned
ipv42num(const char *str)
{
	unsigned num[4] = {0, 0, 0, 0};
	unsigned value = -1;
	size_t len = strlen(str);
	int cyc = 0;
	for(unsigned i = 0 ; i < len ; i++) {
		switch(str[i]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			num[cyc] = num[cyc]*10+(str[i]-'0');
			break;
		case '.':
			cyc++;
			break;
		}
	}

	value = num[0]*256*256*256+num[1]*256*256+num[2]*256+num[3];
	return(value);
}


static unsigned
code_int(unsigned ip, wrkrInstanceData_t *pWrkrData){
	unsigned random;
	unsigned long long shiftIP_subst = ip;
	// variable needed because shift operation of 32nd bit in unsigned does not work
	switch(pWrkrData->pData->ipv4.mode) {
	case ZERO:
		shiftIP_subst = ((shiftIP_subst>>(pWrkrData->pData->ipv4.bits))<<(pWrkrData->pData->ipv4.bits));
		return (unsigned)shiftIP_subst;
	case RANDOMINT:
		shiftIP_subst = ((shiftIP_subst>>(pWrkrData->pData->ipv4.bits))<<(pWrkrData->pData->ipv4.bits));
		// multiply the random number between 0 and 1 with a mask of (2^n)-1:
		random = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*((1ull<<(pWrkrData->pData->ipv4.bits))-1));
		return (unsigned)shiftIP_subst + random;
	case SIMPLE:  //can't happen, since this case is caught at the start of anonipv4()
	default:
		LogError(0, RS_RET_INTERNAL_ERROR, "mmanon: unexpected code path reached in code_int function");
		return 0;
	}
}


static int
num2ipv4(unsigned num, char *str) {
	int numip[4];
	size_t len;
	for(int i = 0 ; i < 4 ; i++){
		numip[i] = num % 256;
		num = num / 256;
	}
	len = snprintf(str, 16, "%d.%d.%d.%d", numip[3], numip[2], numip[1], numip[0]);
	return len;
}


static void
getip(uchar *start, size_t end, char *address)
{
	size_t i;

	for(i = 0; i < end; i++){
		address[i] = *(start+i);
	}
	address[i] = '\0';
}


static char*
findip(char* address, wrkrInstanceData_t *pWrkrData)
{
	int i;
	unsigned num;
	union node* current;
	union node* Last;
	int MoreLess;
	char* CurrentCharPtr;

	current = pWrkrData->pData->ipv4.Root;
	num = ipv42num(address);
	for(i = 0; i < 31; i++){
		if(pWrkrData->pData->ipv4.Root == NULL) {
			current = (union node*)calloc(1, sizeof(union node));
			pWrkrData->pData->ipv4.Root = current;
		}
		Last = current;
		if((num >> (31 - i)) & 1){
			current = current->pointer.more;
			MoreLess = 1;
		} else {
			current = current->pointer.less;
			MoreLess = 0;
		}
		if(current == NULL){
			current = (union node*)calloc(1, sizeof(union node));
			if(MoreLess == 1){
				Last->pointer.more = current;
			} else {
				Last->pointer.less = current;
			}
		}
	}
	if(num & 1){
		CurrentCharPtr = current->ips.ip_high;
	} else {
		CurrentCharPtr = current->ips.ip_low;
	}
	if(CurrentCharPtr[0] != '\0'){
		return CurrentCharPtr;
	} else {
		num = code_int(num, pWrkrData);
		num2ipv4(num, CurrentCharPtr);
		return CurrentCharPtr;
	}
}


static void
process_IPv4 (char* address, wrkrInstanceData_t *pWrkrData)
{
	char* current;
	unsigned num;

	if(pWrkrData->pData->ipv4.randConsis){
		current = findip(address, pWrkrData);
		strcpy(address, current);
	}else {
		num = ipv42num(address);
		num = code_int(num, pWrkrData);
		num2ipv4(num, address);
	}
}


static void
simpleAnon(wrkrInstanceData_t *const pWrkrData, uchar *const msg, int *const hasChanged, int iplen)
{
	int maxidx = iplen - 1;

	int j = -1;
	for(int i = (pWrkrData->pData->ipv4.bits / 8); i > 0; i--) {
		j++;
		while('0' <= msg[maxidx - j] && msg[maxidx - j] <= '9') {
			if(msg[maxidx - j] != pWrkrData->pData->ipv4.replaceChar) {
				msg[maxidx - j] = pWrkrData->pData->ipv4.replaceChar;
				*hasChanged = 1;
			}
			j++;
		}
	}
}


static void
anonipv4(wrkrInstanceData_t *pWrkrData, uchar **msg, int *pLenMsg, int *idx, int *hasChanged)
{
	char address[16];
	char caddress[16];
	int offset = *idx;
	uchar* msgcpy = *msg;
	size_t iplen;
	size_t caddresslen;
	int oldLen = *pLenMsg;

	if(syntax_ipv4((*msg) + offset, *pLenMsg - offset, &iplen)) {
		if(pWrkrData->pData->ipv4.mode == SIMPLE) {
			simpleAnon(pWrkrData, *msg + *idx, hasChanged, iplen);
			*idx += iplen;
			return;
		}

		assert(iplen < sizeof(address));
		getip(*msg + offset, iplen, address);
		offset += iplen;
		strcpy(caddress, address);
		process_IPv4(caddress, pWrkrData);
		caddresslen = strlen(caddress);
		*hasChanged = 1;

		if(caddresslen != strlen(address)) {
			*pLenMsg = *pLenMsg + (caddresslen - strlen(address));
			*msg = (uchar*) malloc(*pLenMsg);
			memcpy(*msg, msgcpy, *idx);
		}
		memcpy(*msg + *idx, caddress, caddresslen);
		*idx = *idx + caddresslen;
		if(*idx < *pLenMsg) {
			memcpy(*msg + *idx, msgcpy + offset, oldLen - offset);
		}
		if(msgcpy != *msg) {
			free(msgcpy);
		}
	}
}


static void
code_ipv6_int(struct ipv6_int* ip, wrkrInstanceData_t *pWrkrData)
{
	unsigned long long randlow = 0;
	unsigned long long randhigh = 0;
	unsigned tmpRand;
	int fullbits;
	int bits = pWrkrData->pData->ipv6.bits;

	if(bits == 128) { //has to be handled separately, since shift
						 //128 bits doesn't work on unsigned long long
		ip->high = 0;
		ip->low = 0;
	} else if(bits > 64) {
		ip->low = 0;
		ip->high = (ip->high >> (bits - 64)) <<  (bits - 64);
	} else if(bits == 64) {
		ip->low = 0;			
	} else {
		ip->low = (ip->low >> bits) << bits;
	}
	switch(pWrkrData->pData->ipv6.anonmode) {
	case ZERO:
		break;
	case RANDOMINT:
		if(bits == 128) {
			for(int i = 0; i < 8; i++) {
				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				ip->high <<= 8;
				ip->high |= tmpRand;

				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				ip->low <<= 8;
				ip->low |= tmpRand;
			}
		} else if(bits > 64) {
			for(int i = 0; i < 8; i++) {
				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				ip->low <<= 8;
				ip->low |= tmpRand;
			}

			bits -= 64;
			fullbits = bits / 8;
			bits = bits % 8;
			while(fullbits > 0) {
				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				randhigh <<= 8;
				randhigh |= tmpRand;
				fullbits--;
			}
			tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*((1 << bits) - 1));
			randhigh <<= bits;
			randhigh |= tmpRand;

			ip->high |= randhigh;
		} else if(bits == 64) {
			for(int i = 0; i < 8; i++) {
				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				ip->low <<= 8;
				ip->low |= tmpRand;
			}
		} else {
			fullbits = bits / 8;
			bits = bits % 8;
			while(fullbits > 0) {
				tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*0xff);
				randlow <<= 8;
				randlow |= tmpRand;
				fullbits--;
			}
			tmpRand = (unsigned)((rand_r(&(pWrkrData->randstatus))/(double)RAND_MAX)*((1 << bits) - 1));
			randlow <<= bits;
			randlow |= tmpRand;

			ip->low |= randlow;
		}
		break;
	case SIMPLE:  //can't happen, since this case is caught at the start of anonipv4()
	default:
		LogError(0, RS_RET_INTERNAL_ERROR, "mmanon: unexpected code path reached in code_int function");
	}
}


static struct ipv6_int* //separate function from recognising ipv6, since the recognition might get more
ipv62num(char* address, size_t iplen)  //complex. This function always stays the same, since it
					//always gets an valid ipv6 input
{
	struct ipv6_int* ip = (struct ipv6_int*) calloc(1, sizeof(struct ipv6_int));
	int num[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cyc = 0;
	int dots = 0;
	int val;
	unsigned i;

	for(i = 0; i < iplen && dots < 2; i++) {
		val = getHexVal(address[i]);
		if(val == -1) {
			dots++;
			if(dots < 2) {
				cyc++;
			}
		} else {
			num[cyc] = num[cyc] * 16 + val;
			dots = 0;
		}
	}
	if(dots == 2) {
		if(i < iplen - 1) {
			int shift = 0;
			cyc = 7;
			for(unsigned j = iplen - 1; j >= i; j--) {
				val = getHexVal(address[j]);
				if(val == -1) {
					cyc--;
					shift = 0;
				} else {
					val <<= shift;
					shift += 4;
					num[cyc] += val;
				}
			}
		} else {
			while(cyc < 8) {
				num[cyc] = 0;
				cyc++;
			}
		}
	}

	for(i = 0; i < 4; i++) {
		ip->high <<= 16;
		ip->high |= num[i];
	}
	while(i < 8) {
		ip->low <<= 16;
		ip->low |= num[i];
		i++;
	}

	return ip;
}


static void
num2ipv6 (struct ipv6_int* ip, char* address)
{
	int num[8];
	int i;

	for(i = 7; i > 3; i--) {
		num[i] = ip->low & 0xffff;
		ip->low >>= 16;
	}
	while(i > -1) {
		num[i] = ip->high & 0xffff;
		ip->high >>= 16;
		i--;
	}

	snprintf(address, 40, "%x:%x:%x:%x:%x:%x:%x:%x", num[0], num[1], num[2], num[3], num[4], num[5], num[6], num[7]);
}


static int
keys_equal_fn(void* key1, void* key2)
{
	struct ipv6_int *const k1 = (struct ipv6_int*) key1;
	struct ipv6_int *const k2 = (struct ipv6_int*) key2;

	return((k1->high == k2->high) && (k1->low == k2->low));
}


static unsigned
hash_from_key_fn (void* k)
{
	struct ipv6_int *const key = (struct ipv6_int*) k;
	unsigned hashVal;

	hashVal = (key->high & 0xFFC00000) | (key->low & 0x3FFFFF);
	return hashVal;
}


static void
findIPv6(struct ipv6_int* num, char* address, wrkrInstanceData_t *const pWrkrData)
{
	if(pWrkrData->pData->ipv6.hash == NULL) {
		pWrkrData->pData->ipv6.hash = create_hashtable(512, hash_from_key_fn, keys_equal_fn, NULL);
	}

	char* val = (char*)(hashtable_search(pWrkrData->pData->ipv6.hash, num));

	if(val != NULL) {
		strcpy(address, val);
	} else {
		struct ipv6_int* hashKey = (struct ipv6_int*) malloc(sizeof(struct ipv6_int));
		hashKey->low = num->low;
		hashKey->high = num->high;

		code_ipv6_int(num, pWrkrData);
		num2ipv6(num, address);
		char* hashString = strdup(address);

		hashtable_insert(pWrkrData->pData->ipv6.hash, hashKey, hashString);
	}
}


static void
process_IPv6 (char* address, wrkrInstanceData_t *pWrkrData, size_t iplen)
{
	struct ipv6_int* num;

	num = ipv62num(address, iplen);

	if(pWrkrData->pData->ipv6.randConsis) {
		findIPv6(num, address, pWrkrData);
	} else {
		code_ipv6_int(num, pWrkrData);
		num2ipv6(num, address);
	}
	free(num);
}


static void
anonipv6(wrkrInstanceData_t *pWrkrData, uchar **msg, int *pLenMsg, int *idx, int *hasChanged)
{
	size_t iplen = 0;
	int offset = *idx;
	char address[40];
	uchar* msgcpy = *msg;
	unsigned caddresslen;
	size_t oldLen = *pLenMsg;

	int syn = syntax_ipv6(*msg + offset, *pLenMsg - offset, &iplen);
	if(syn) {
		assert(iplen < sizeof(address));
		getip(*msg + offset, iplen, address);
		offset += iplen;
		process_IPv6(address, pWrkrData, iplen);

		caddresslen = strlen(address);
		*hasChanged = 1;

		if(caddresslen != iplen) {
			*pLenMsg = *pLenMsg + (caddresslen - iplen);
			*msg = (uchar*) malloc(*pLenMsg);
			memcpy(*msg, msgcpy, *idx);
		}
		memcpy(*msg + *idx, address, caddresslen);
		*idx = *idx + caddresslen;
		if(*idx < *pLenMsg) {
			memcpy(*msg + *idx, msgcpy + offset, oldLen - offset);
		}
		if(msgcpy != *msg) {
			free(msgcpy);
		}
	}
}


BEGINdoAction_NoStrings
	smsg_t **ppMsg = (smsg_t **) pMsgData;
	smsg_t *pMsg = ppMsg[0];
	uchar *msg;
	int lenMsg;
	int i;
	int hasChanged = 0;
CODESTARTdoAction
	lenMsg = getMSGLen(pMsg);
	msg = (uchar*)strdup((char*)getMSG(pMsg));

	for(i = 0 ; i <= lenMsg - 2 ; i++) {
		if(pWrkrData->pData->ipv4.enable) {
			anonipv4(pWrkrData, &msg, &lenMsg, &i, &hasChanged);
		}
		if(pWrkrData->pData->ipv6.enable) {
			anonipv6(pWrkrData, &msg, &lenMsg, &i, &hasChanged);
		}
	}
	if(hasChanged) {
		MsgReplaceMSG(pMsg, msg, lenMsg);
	}
	free(msg);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(strncmp((char*) p, ":mmanon:", sizeof(":mmanon:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"mmanon supports only v6+ config format, use: "
			"action(type=\"mmanon\" ...)");
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
	DBGPRINTF("mmanon: module compiled with rsyslog version %s.\n", VERSION);
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit
