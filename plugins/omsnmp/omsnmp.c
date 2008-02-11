/* omsnmp.c
 *
 * This module sends an snmp trap. More text will come here soon ^^
 *
 * This module will become part of the CVS and the rsyslog project because I think
 * it is a generally useful debugging, testing and development aid for everyone
 * involved with rsyslog.
 *
 * CURRENT SUPPORTED COMMANDS:
 *
 * :omsnmp:sleep <seconds> <milliseconds>
 *
 * Must be specified exactly as above. Keep in mind milliseconds are a millionth
 * of a second!
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <assert.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "cfsysline.h"
#include "module-template.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "omsnmp.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA

/* Default static snmp OID's */
/*unused 
static oid             objid_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
static oid             objid_sysdescr[] = { 1, 3, 6, 1, 2, 1, 1, 1, 0 };
*/
static oid             objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
static oid             objid_sysuptime[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };

static uchar* pszTransport = NULL; /* default transport */
static uchar* pszTarget = NULL;
/* note using an unsigned for a port number is not a good idea from an IPv6 point of view */
static int iPort = 0;
static int iSNMPVersion = 1;	/* 0 Means SNMPv1, 1 Means SNMPv2c */
static uchar* pszCommunity = NULL;
static uchar* pszEnterpriseOID = NULL;
static uchar* pszSyslogMessageOID = NULL;
static int iSpecificType = 0;
static int iTrapType = SNMP_TRAP_ENTERPRISESPECIFIC;/*Default is SNMP_TRAP_ENTERPRISESPECIFIC */
/* 
			Possible Values
	SNMP_TRAP_COLDSTART				(0)
	SNMP_TRAP_WARMSTART				(1)
	SNMP_TRAP_LINKDOWN				(2)
	SNMP_TRAP_LINKUP				(3)
	SNMP_TRAP_AUTHFAIL				(4)
	SNMP_TRAP_EGPNEIGHBORLOSS		(5)
	SNMP_TRAP_ENTERPRISESPECIFIC	(6)
*/

typedef struct _instanceData {
	uchar	szTransport[OMSNMP_MAXTRANSPORLENGTH+1];	/* Transport - Can be udp, tcp, udp6, tcp6 and other types supported by NET-SNMP */ 
	uchar	szTarget[MAXHOSTNAMELEN+1];					/* IP/hostname of Snmp Target*/ 
	uchar	szTargetAndPort[MAXHOSTNAMELEN+1];			/* IP/hostname + Port,needed format for SNMP LIB */ 
	uchar	szCommunity[OMSNMP_MAXCOMMUNITYLENGHT+1];	/* Snmp Community */ 
	uchar	szEnterpriseOID[OMSNMP_MAXOIDLENGHT+1];		/* Snmp Enterprise OID - default is (1.3.6.1.4.1.3.1.1 = enterprises.cmu.1.1) */ 
	uchar	szSyslogMessageOID[OMSNMP_MAXOIDLENGHT+1];	/* Snmp OID used for the Syslog Message - default is 1.3.6.1.4.1 - .iso.org.dod.internet.private.enterprises */ 
	int iPort;											/* Target Port */
	int iSNMPVersion;									/* SNMP Version to use */
	int iTrapType;										/* Snmp TrapType or GenericType */
	int iSpecificType;									/* Snmp Specific Type */
} instanceData;

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("SNMPTransport: %s\n", pData->szTransport);
	dbgprintf("SNMPTarget: %s\n", pData->szTarget);
	dbgprintf("SNMPPort: %d\n", pData->iPort);
	dbgprintf("SNMPTarget+PortStr: %s\n", pData->szTargetAndPort);
	dbgprintf("SNMPVersion (0=v1, 1=v2c): %d\n", pData->iSNMPVersion);
	dbgprintf("Community: %s\n", pData->szCommunity);
	dbgprintf("EnterpriseOID: %s\n", pData->szEnterpriseOID);
	dbgprintf("SyslogMessageOID: %s\n", pData->szSyslogMessageOID);
	dbgprintf("TrapType: %d\n", pData->iTrapType);
	dbgprintf("SpecificType: %d\n", pData->iSpecificType);
ENDdbgPrintInstInfo


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* we are not compatible with repeated msg reduction feature, so do not allow it */
ENDisCompatibleWithFeature


BEGINtryResume
CODESTARTtryResume
ENDtryResume


/* 
*	Helper function - not used yet!
*/
/* NOT NEEDED AT ALL 
static int snmp_input_sender(int operation, netsnmp_session * session, int reqid, netsnmp_pdu *pdu, void *magic)
{
   return 1;
}
*/

static rsRetVal omsnmp_sendsnmp(instanceData *pData, uchar *psz)
{
	DEFiRet;

	netsnmp_session session, *ss;
	netsnmp_pdu    *pdu = NULL;
	oid             enterpriseoid[MAX_OID_LEN];
	size_t          enterpriseoidlen = MAX_OID_LEN;
	oid				oidSyslogMessage[MAX_OID_LEN];
	size_t			oLen = MAX_OID_LEN;
	int             status;
	char           *trap = NULL; /* obselte , *specific = NULL, *description = NULL, *agent = NULL; */
	char		   *strErr = NULL;

	assert(psz != NULL);
	dbgprintf( "omsnmp_sendsnmp: ENTER - Target = '%s' on Port = '%d' syslogmessage = '%s'\n", pData->szTarget, pData->iPort, (char*)psz);

	putenv(strdup("POSIXLY_CORRECT=1"));

	snmp_sess_init(&session);
	session.version = pData->iSNMPVersion; /* Sample SNMP_VERSION_1; */
	session.callback = NULL; /* NOT NEEDED */
	session.callback_magic = NULL;
	session.peername = (char*) pData->szTargetAndPort;
	
	/* Set SNMP Community */
	if (session.version == SNMP_VERSION_1 || session.version == SNMP_VERSION_2c)
	{	
		session.community = (unsigned char *) pData->szCommunity;
		session.community_len = strlen((char*) pData->szCommunity);
	}

	ss = snmp_open(&session);
	if (ss == NULL) 
	{
		/*TODO diagnose snmp_open errors with the input netsnmp_session pointer */
		dbgprintf("omsnmp_sendsnmp: snmp_open to host '%s' on Port '%d' failed\n", pData->szTarget, pData->iPort);
        return RS_RET_FALSE;
	}
	
	/* If SNMP Version1 is configured !*/
	if (session.version == SNMP_VERSION_1) 
	{
		pdu = snmp_pdu_create(SNMP_MSG_TRAP);

		/* Set enterprise */
		if (!snmp_parse_oid( (char*) pData->szEnterpriseOID, &enterpriseoid, &enterpriseoidlen ))
		{
			strErr = snmp_api_errstring(snmp_errno);
			dbgprintf("omsnmp_sendsnmp: Parsing EnterpriseOID failed '%s' with error '%s' \n", pData->szSyslogMessageOID, strErr);
			
			/* CLEANUP */
			snmp_free_pdu(pdu);

			return RS_RET_FALSE;
		}
		pdu->enterprise = (oid *) malloc(enterpriseoidlen * sizeof(oid));
		memcpy(pdu->enterprise, enterpriseoid, enterpriseoidlen * sizeof(oid));
		pdu->enterprise_length = enterpriseoidlen;

		/* Set Traptype */
		pdu->trap_type = pData->iTrapType; 
		
		/* Set SpecificType */
		pdu->specific_type = pData->iSpecificType;

		/* Set Updtime */
		pdu->time = get_uptime();
	}
	/* If SNMP Version2c is configured !*/
	else if (session.version == SNMP_VERSION_2c) 
	{
		long sysuptime;
		char csysuptime[20];
		
		/* Create PDU */
		pdu = snmp_pdu_create(SNMP_MSG_TRAP2);
		
		/* Set uptime */
		sysuptime = get_uptime();
		sprintf(csysuptime, "%ld", sysuptime);
		trap = csysuptime;
		snmp_add_var(pdu, objid_sysuptime, sizeof(objid_sysuptime) / sizeof(oid), 't', trap);

		/* Now set the SyslogMessage Trap OID */
		if ( snmp_add_var(pdu, objid_snmptrap, sizeof(objid_snmptrap) / sizeof(oid), 'o', (char*) pData->szSyslogMessageOID ) != 0)
		{
			strErr = snmp_api_errstring(snmp_errno);
			dbgprintf("omsnmp_sendsnmp: Adding trap OID failed '%s' with error '%s' \n", pData->szSyslogMessageOID, strErr);
			
			/* CLEANUP */
			snmp_free_pdu(pdu);

			return RS_RET_FALSE;
		}
	}

	/* SET TRAP PARAMETER for SyslogMessage! */
/*	dbgprintf( "omsnmp_sendsnmp: SyslogMessage '%s'\n", psz );*/

	/* First create new OID object */
	if (snmp_parse_oid( (char*) pData->szSyslogMessageOID, &oidSyslogMessage, &oLen))
	{
		int iErrCode = snmp_add_var(pdu, &oidSyslogMessage, oLen, 's', (char*) psz);
		if (iErrCode)
		{
			const char *str = snmp_api_errstring(iErrCode);
			dbgprintf( "omsnmp_sendsnmp: Invalid SyslogMessage OID, error code '%d' - '%s'\n", iErrCode, str );

			/* CLEANUP */
			snmp_free_pdu(pdu);

			return RS_RET_FALSE;
		}
	}
	else
	{
		strErr = snmp_api_errstring(snmp_errno);
		dbgprintf("omsnmp_sendsnmp: Parsing SyslogMessageOID failed '%s' with error '%s' \n", pData->szSyslogMessageOID, strErr);

		/* CLEANUP */
		snmp_free_pdu(pdu);

		return RS_RET_FALSE;
	}

	/* Send the TRAP */
	status = snmp_send(ss, pdu) == 0;
	if (status)
	{
		int iErrorCode = 0;
		if (ss->s_snmp_errno != 0)
			iErrorCode = ss->s_snmp_errno;
		else
			iErrorCode = session.s_snmp_errno;
		
		/* Debug Output! */
		dbgprintf( "omsnmp_sendsnmp: snmp_send failed error '%d', Description='%s'\n", iErrorCode*(-1), api_errors[iErrorCode*(-1)]);

		/* Important we must free the PDU! */
		snmp_free_pdu(pdu);

		/* Close SNMP Session */
		snmp_close(ss);

		/* TODO! CLEANUP */
		return RS_RET_FALSE;
	}
	else
	{
		/* Just Close SNMP Session */
		snmp_close(ss);
	}

	dbgprintf( "omsnmp_sendsnmp: LEAVE\n");
	RETiRet;
}


BEGINdoAction
CODESTARTdoAction
	/* Abort if the STRING is not set, should never happen */
	if (ppString[0] == NULL)
	{
		ABORT_FINALIZE(RS_RET_TRUE);
	}

/*	dbgprintf("omsnmp: Sending SNMP Trap to '%s' on Port '%d'\n", pData->szTarget, pData->iPort); */
	iRet = omsnmp_sendsnmp(pData, ppString[0]);
	if (iRet == RS_RET_FALSE)
	{
		/* TODO: CLEANUP! */
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
finalize_it:
ENDdoAction

BEGINfreeInstance
CODESTARTfreeInstance
	/* we do not have instance data, so we do not need to
	 * do anything here. -- rgerhards, 2007-07-25
	 */
ENDfreeInstance


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* code here is quick and dirty - if you like, clean it up. But keep
	 * in mind it is just a testing aid ;) -- rgerhards, 2007-12-31
	 */
	if(!strncmp((char*) p, ":omsnmp:", sizeof(":omsnmp:") - 1)) {
		p += sizeof(":omsnmp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;

	/* Check Transport */
	if (pszTransport == NULL) {
		/* 
		* Default transport is UDP. Other values supported by NETSNMP are possible as well
		 */
		strncpy( (char*) pData->szTransport, "udp", sizeof("udp") );
	} else {
	/* Copy Transport */
	strncpy( (char*) pData->szTransport, (char*) pszTransport, strlen((char*) pszTransport) );
	}

	/* Check Target */
	if (pszTarget == NULL) {
		/* due to a problem in the framework, we can not return an error code
		 * right now, so we need to use a (useless) default.
		 */
		/* TODO: re-enable when rsyslogd supports it: ABORT_FINALIZE( RS_RET_PARAM_ERROR ); */
		strncpy( (char*) pData->szTarget, "127.0.0.1", sizeof("127.0.0.1") );
	} else {
	/* Copy Target */
	strncpy( (char*) pData->szTarget, (char*) pszTarget, strlen((char*) pszTarget) );
	}

	/* Copy Community */
	if (pszCommunity == NULL)	/* Failsave */
		strncpy( (char*) pData->szCommunity, "public", sizeof("public") );
	else						/* Copy Target */
		strncpy( (char*) pData->szCommunity, (char*) pszCommunity, strlen((char*) pszCommunity) );

	/* Copy Enterprise OID */
	if (pszEnterpriseOID == NULL)	/* Failsave */
		strncpy( (char*) pData->szEnterpriseOID, "1.3.6.1.4.1.3.1.1", sizeof("1.3.6.1.4.1.3.1.1") );
	else						/* Copy Target */
		strncpy( (char*) pData->szEnterpriseOID, (char*) pszEnterpriseOID, strlen((char*) pszEnterpriseOID) );

	/* Copy SyslogMessage OID */
	if (pszSyslogMessageOID == NULL)	/* Failsave */
		strncpy( (char*) pData->szSyslogMessageOID, "1.3.6.1.6.3.1.1.4.3", sizeof("1.3.6.1.6.3.1.1.4.3") );
	else						/* Copy Target */
		strncpy( (char*) pData->szSyslogMessageOID, (char*) pszSyslogMessageOID, strlen((char*) pszSyslogMessageOID) );

	/* Copy Port */
	if ( iPort == 0)		/* If no Port is set we use the default Port 162 */
		pData->iPort = 162;
	else
		pData->iPort = iPort;
	
	/* Set SNMPVersion */
	if ( iSNMPVersion < 0 || iSNMPVersion > 1)		/* Set default to 1 if out of range */
		pData->iSNMPVersion = 1;
	else
		pData->iSNMPVersion = iSNMPVersion;

	/* Copy SpecificType */
	if ( iSpecificType == 0)		/* If no iSpecificType is set, we use the default 0 */
		pData->iSpecificType = 0;
	else
		pData->iSpecificType = iSpecificType;

	/* Copy TrapType */
	if ( iTrapType < 0 && iTrapType >= 6)		/* Only allow values from 0 to 6 !*/
		pData->iTrapType = SNMP_TRAP_ENTERPRISESPECIFIC;
	else
		pData->iTrapType = iTrapType;

	/* Create string for session peername! */
	sprintf( (char*) pData->szTargetAndPort, "%s:%s:%d", pData->szTransport, pData->szTarget, pData->iPort );
	
	/* Print Debug info */
	dbgprintf("SNMPTransport: %s\n", pData->szTransport);
	dbgprintf("SNMPTarget: %s\n", pData->szTarget);
	dbgprintf("SNMPPort: %d\n", pData->iPort);
	dbgprintf("SNMPTarget+PortStr: %s\n", pData->szTargetAndPort);
	dbgprintf("SNMPVersion (0=v1, 1=v2c): %d\n", pData->iSNMPVersion);
	dbgprintf("Community: %s\n", pData->szCommunity);
	dbgprintf("EnterpriseOID: %s\n", pData->szEnterpriseOID);
	dbgprintf("SyslogMessageOID: %s\n", pData->szSyslogMessageOID);
	dbgprintf("TrapType: %d\n", pData->iTrapType);
	dbgprintf("SpecificType: %d\n", pData->iSpecificType);

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " StdFwdFmt"));

	/* Init NetSNMP library and read in MIB database */
	init_snmp("rsyslog");

	/* Set some defaults in the NetSNMP library */
	netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DEFAULT_PORT, pData->iPort );

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
ENDneedUDPSocket

/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if (pszTarget != NULL)
		free(pszTarget);
	pszTarget = NULL;
	iPort = 0;

	return RS_RET_OK;
}


BEGINmodExit
CODESTARTmodExit
if (pszTarget != NULL)
	free(pszTarget);	
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr

	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmptransport", 0, eCmdHdlrGetWord, NULL, &pszTransport, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmptarget", 0, eCmdHdlrGetWord, NULL, &pszTarget, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmptargetport", 0, eCmdHdlrInt, NULL, &iPort, NULL));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmpversion", 0, eCmdHdlrInt, NULL, &iSNMPVersion, NULL));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpcommunity", 0, eCmdHdlrGetWord, NULL, &pszCommunity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpenterpriseoid", 0, eCmdHdlrGetWord, NULL, &pszEnterpriseOID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpsyslogmessageoid", 0, eCmdHdlrGetWord, NULL, &pszSyslogMessageOID, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmpspecifictype", 0, eCmdHdlrInt, NULL, &iSpecificType, NULL));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmptraptype", 0, eCmdHdlrInt, NULL, &iTrapType, NULL));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
