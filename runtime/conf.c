/* The config file handler (not yet a real object)
 *
 * This file is based on an excerpt from syslogd.c, which dates back
 * much later. I began the file on 2008-02-19 as part of the modularization
 * effort. Over time, a clean abstration will become even more important
 * because the config file handler will by dynamically be loaded and be
 * kept in memory only as long as the config file is actually being 
 * processed. Thereafter, it shall be unloaded. -- rgerhards
 * Please note that the original syslogd.c source was under BSD license
 * at the time of the rsyslog fork from sysklogd.
 *
 * Copyright 2008-2012 Rainer Gerhards and Adiscon GmbH.
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
#define CFGLNSIZ 64*1024 /* the maximum size of a configuraton file line, after re-combination */
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <dirent.h>
#include <glob.h>
#include <sys/types.h>
#ifdef HAVE_LIBGEN_H
#	ifndef OS_SOLARIS
#		include <libgen.h>
#	endif
#endif

#include "rsyslog.h"
#include "dirty.h"
#include "parse.h"
#include "action.h"
#include "template.h"
#include "cfsysline.h"
#include "modules.h"
#include "outchannel.h"
#include "stringbuf.h"
#include "conf.h"
#include "stringbuf.h"
#include "srUtils.h"
#include "errmsg.h"
#include "net.h"
#include "expr.h"
#include "ctok.h"
#include "ctok_token.h"
#include "rule.h"
#include "ruleset.h"
#include "unicode-helper.h"

#ifdef OS_SOLARIS
#	define NAME_MAX MAXNAMELEN
#endif

/* forward definitions */
static rsRetVal cfline(uchar *line, rule_t **pfCurr);
static rsRetVal processConfFile(uchar *pConfFile);


/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(expr)
DEFobjCurrIf(ctok)
DEFobjCurrIf(ctok_token)
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(net)
DEFobjCurrIf(rule)
DEFobjCurrIf(ruleset)

ecslConfObjType currConfObj = eConfObjGlobal; /* to support scoping - which config object is currently active? */
int bConfStrictScoping = 0;	/* force strict scoping during config processing? */


static int iNbrActions = 0; /* number of currently defined actions */

/* The following module-global variables are used for building
 * tag and host selector lines during startup and config reload.
 * This is stored as a global variable pool because of its ease. It is
 * also fairly compatible with multi-threading as the stratup code must
 * be run in a single thread anyways. So there can be no race conditions.
 * rgerhards 2005-10-18
 */
static EHostnameCmpMode eDfltHostnameCmpMode = HN_NO_COMP;
static cstr_t *pDfltHostnameCmp = NULL;
static cstr_t *pDfltProgNameCmp = NULL;


/* process a directory and include all of its files into
 * the current config file. There is no specific order of inclusion,
 * files are included in the order they are read from the directory.
 * The caller must have make sure that the provided parameter is
 * indeed a directory.
 * rgerhards, 2007-08-01
 */
static rsRetVal doIncludeDirectory(uchar *pDirName)
{
	DEFiRet;
	int iEntriesDone = 0;
	DIR *pDir;
	union {
              struct dirent d;
              char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
	} u;
	struct dirent *res;
	size_t iDirNameLen;
	size_t iFileNameLen;
	uchar szFullFileName[MAXFNAME];

	ASSERT(pDirName != NULL);

	if((pDir = opendir((char*) pDirName)) == NULL) {
		errmsg.LogError(errno, RS_RET_FOPEN_FAILURE, "error opening include directory");
		ABORT_FINALIZE(RS_RET_FOPEN_FAILURE);
	}

	/* prepare file name buffer */
	iDirNameLen = strlen((char*) pDirName);
	memcpy(szFullFileName, pDirName, iDirNameLen);

	/* now read the directory */
	iEntriesDone = 0;
	while(readdir_r(pDir, &u.d, &res) == 0) {
		if(res == NULL)
			break; /* this also indicates end of directory */
#		ifdef DT_REG
		/* TODO: find an alternate way to checking for special files if this is
		 * not defined. This is currently a known problem on HP UX, but the work-
		 * around is simple: do not create special files in that directory. So 
		 * fixing this is actually not the most important thing on earth...
		 * rgerhards, 2008-03-04
		 */
		if(res->d_type != DT_REG)
			continue; /* we are not interested in special files */
#		endif
		if(res->d_name[0] == '.')
			continue; /* these files we are also not interested in */
		++iEntriesDone;
		/* construct filename */
		iFileNameLen = strlen(res->d_name);
		if (iFileNameLen > NAME_MAX)
			iFileNameLen = NAME_MAX;
		memcpy(szFullFileName + iDirNameLen, res->d_name, iFileNameLen);
		*(szFullFileName + iDirNameLen + iFileNameLen) = '\0';
		dbgprintf("including file '%s'\n", szFullFileName);
		processConfFile(szFullFileName);
		/* we deliberately ignore the iRet of processConfFile() - this is because
		 * failure to process one file does not mean all files will fail. By ignoring,
		 * we retry with the next file, which is the best thing we can do. -- rgerhards, 2007-08-01
		 */
	}

	if(iEntriesDone == 0) {
		/* I just make it a debug output, because I can think of a lot of cases where it
		 * makes sense not to have any files. E.g. a system maintainer may place a $Include
		 * into the config file just in case, when additional modules be installed. When none
		 * are installed, the directory will be empty, which is fine. -- rgerhards 2007-08-01
		 */
		dbgprintf("warning: the include directory contained no files - this may be ok.\n");
	}

finalize_it:
	if(pDir != NULL)
		closedir(pDir);

	RETiRet;
}


/* process a $include config line. That type of line requires
 * inclusion of another file.
 * rgerhards, 2007-08-01
 */
rsRetVal
doIncludeLine(uchar **pp, __attribute__((unused)) void* pVal)
{
	DEFiRet;
	char pattern[MAXFNAME];
	uchar *cfgFile;
	glob_t cfgFiles;
	int result;
	size_t i = 0;
	struct stat fileInfo;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);

	if(getSubString(pp, (char*) pattern, sizeof(pattern) / sizeof(char), ' ')  != 0) {
		errmsg.LogError(0, RS_RET_NOT_FOUND, "could not parse config file name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* Use GLOB_MARK to append a trailing slash for directories.
	 * Required by doIncludeDirectory().
	 */
	result = glob(pattern, GLOB_MARK, NULL, &cfgFiles);
	if(result == GLOB_NOSPACE || result == GLOB_ABORTED) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND, "error accessing config file or directory '%s': %s",
				pattern, errStr);
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
	}

	for(i = 0; i < cfgFiles.gl_pathc; i++) {
		cfgFile = (uchar*) cfgFiles.gl_pathv[i];

		if(stat((char*) cfgFile, &fileInfo) != 0) 
			continue; /* continue with the next file if we can't stat() the file */

		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			dbgprintf("requested to include config file '%s'\n", cfgFile);
			iRet = processConfFile(cfgFile);
		} else if(S_ISDIR(fileInfo.st_mode)) { /* config directory */
			dbgprintf("requested to include directory '%s'\n", cfgFile);
			iRet = doIncludeDirectory(cfgFile);
		} else { /* TODO: shall we handle symlinks or not? */
			dbgprintf("warning: unable to process IncludeConfig directive '%s'\n", cfgFile);
		}
	}

	globfree(&cfgFiles);

finalize_it:
	RETiRet;
}


/* process a $ModLoad config line.
 */
rsRetVal
doModLoad(uchar **pp, __attribute__((unused)) void* pVal)
{
	DEFiRet;
	uchar szName[512];
	uchar *pModName;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);

	skipWhiteSpace(pp); /* skip over any whitespace */
	if(getSubString(pp, (char*) szName, sizeof(szName) / sizeof(uchar), ' ')  != 0) {
		errmsg.LogError(0, RS_RET_NOT_FOUND, "could not extract module name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}
	skipWhiteSpace(pp); /* skip over any whitespace */

	/* this below is a quick and dirty hack to provide compatibility with the
	 * $ModLoad MySQL forward compatibility statement. TODO: clean this up
	 * For the time being, it is clean enough, it just needs to be done
	 * differently when we have a full design for loadable plug-ins. For the
	 * time being, we just mangle the names a bit.
	 * rgerhards, 2007-08-14
	 */
	if(!strcmp((char*) szName, "MySQL"))
		pModName = (uchar*) "ommysql.so";
	else
		pModName = szName;

	CHKiRet(module.Load(pModName));

finalize_it:
	RETiRet;
}


/* parse and interpret a $-config line that starts with
 * a name (this is common code). It is parsed to the name
 * and then the proper sub-function is called to handle
 * the actual directive.
 * rgerhards 2004-11-17
 * rgerhards 2005-06-21: previously only for templates, now 
 *    generalized.
 */
rsRetVal
doNameLine(uchar **pp, void* pVal)
{
	DEFiRet;
	uchar *p;
	enum eDirective eDir;
	char szName[128];

	ASSERT(pp != NULL);
	p = *pp;
	ASSERT(p != NULL);

	eDir = (enum eDirective) pVal;	/* this time, it actually is NOT a pointer! */

	if(getSubString(&p, szName, sizeof(szName) / sizeof(char), ',')  != 0) {
		errmsg.LogError(0, RS_RET_NOT_FOUND, "Invalid config line: could not extract name - line ignored");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}
	if(*p == ',')
		++p; /* comma was eaten */
	
	/* we got the name - now we pass name & the rest of the string
	 * to the subfunction. It makes no sense to do further
	 * parsing here, as this is in close interaction with the
	 * respective subsystem. rgerhards 2004-11-17
	 */
	
	switch(eDir) {
		case DIR_TEMPLATE: 
			tplAddLine(szName, &p);
			break;
		case DIR_OUTCHANNEL: 
			ochAddLine(szName, &p);
			break;
		case DIR_ALLOWEDSENDER: 
			net.addAllowedSenderLine(szName, &p);
			break;
		default:/* we do this to avoid compiler warning - not all
			 * enum values call this function, so an incomplete list
			 * is quite ok (but then we should not run into this code,
			 * so at least we log a debug warning).
			 */
			dbgprintf("INTERNAL ERROR: doNameLine() called with invalid eDir %d.\n",
				eDir);
			break;
	}

	*pp = p;

finalize_it:
	RETiRet;
}


/* Parse and interpret a system-directive in the config line
 * A system directive is one that starts with a "$" sign. It offers
 * extended configuration parameters.
 * 2004-11-17 rgerhards
 */
rsRetVal
cfsysline(uchar *p)
{
	DEFiRet;
	uchar szCmd[64];

	ASSERT(p != NULL);
	errno = 0;
	if(getSubString(&p, (char*) szCmd, sizeof(szCmd) / sizeof(uchar), ' ')  != 0) {
		errmsg.LogError(0, RS_RET_NOT_FOUND, "Invalid $-configline - could not extract command - line ignored\n");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* we now try and see if we can find the command in the registered
	 * list of cfsysline handlers. -- rgerhards, 2007-07-31
	 */
	CHKiRet(processCfSysLineCommand(szCmd, &p));

	/* now check if we have some extra characters left on the line - that
	 * should not be the case. Whitespace is OK, but everything else should
	 * trigger a warning (that may be an indication of undesired behaviour).
	 * An exception, of course, are comments (starting with '#').
	 * rgerhards, 2007-07-04
	 */
	skipWhiteSpace(&p);

	if(*p && *p != '#') { /* we have a non-whitespace, so let's complain */
		errmsg.LogError(0, NO_ERRCODE, 
		         "error: extra characters in config line ignored: '%s'", p);
	}

finalize_it:
	RETiRet;
}




/* process a configuration file
 * started with code from init() by rgerhards on 2007-07-31
 */
static rsRetVal
processConfFile(uchar *pConfFile)
{
	int iLnNbr = 0;
	FILE *cf;
	rule_t *pCurrRule = NULL;
	uchar *p;
	uchar cbuf[CFGLNSIZ];
	uchar *cline;
	int i;
	int bHadAnError = 0;
	uchar *pszOrgLine = NULL;
	size_t lenLine;
	DEFiRet;
	ASSERT(pConfFile != NULL);

	if((cf = fopen((char*)pConfFile, "r")) == NULL) {
		ABORT_FINALIZE(RS_RET_FOPEN_FAILURE);
	}

	/* Now process the file.
	 */
	cline = cbuf;
	while (fgets((char*)cline, sizeof(cbuf) - (cline - cbuf), cf) != NULL) {
		++iLnNbr;
		/* drop LF - TODO: make it better, replace fgets(), but its clean as it is */
		lenLine = ustrlen(cline);
		if(cline[lenLine-1] == '\n') {
			cline[lenLine-1] = '\0';
		}
		free(pszOrgLine);
		pszOrgLine = ustrdup(cline); /* save if needed for errmsg, NULL ptr is OK */
		/* check for end-of-section, comments, strip off trailing
		 * spaces and newline character.
		 */
		p = cline;
		skipWhiteSpace(&p);
		if (*p == '\0' || *p == '#')
			continue;

		/* we now need to copy the characters to the begin of line. As this overlaps,
		 * we can not use strcpy(). -- rgerhards, 2008-03-20
		 * TODO: review the code at whole - this is highly suspect (but will go away
		 * once we do the rest of RainerScript).
		 */
		for( i = 0 ; p[i] != '\0' ; ++i) {
			cline[i] = p[i];
		}
		cline[i] = '\0';

		for (p = (uchar*) strchr((char*)cline, '\0'); isspace((int) *--p);)
			/*EMPTY*/;
		if (*p == '\\') {
			if ((p - cbuf) > CFGLNSIZ - 30) {
				/* Oops the buffer is full - what now? */
				cline = cbuf;
				dbgprintf("buffer overflow extending config file\n");
				errmsg.LogError(0, RS_RET_CONFIG_ERROR,
					"error: config file line %d too long", iLnNbr);
			} else {
				*p = 0;
				cline = p;
				continue;
			}
		}  else
			cline = cbuf;
		*++p = '\0'; /* TODO: check this */

		/* we now have the complete line, and are positioned at the first non-whitespace
		 * character. So let's process it
		 */
		if(cfline(cbuf, &pCurrRule) != RS_RET_OK) {
			/* we log a message, but otherwise ignore the error. After all, the next
			 * line can be correct.  -- rgerhards, 2007-08-02
			 */
			uchar szErrLoc[MAXFNAME + 64];
			dbgprintf("config line NOT successfully processed\n");
			snprintf((char*)szErrLoc, sizeof(szErrLoc) / sizeof(uchar),
				 "%s, line %d", pConfFile, iLnNbr);
			errmsg.LogError(0, NO_ERRCODE, "the last error occured in %s:\"%s\"", (char*)szErrLoc, (char*)pszOrgLine);
			bHadAnError = 1;
		}
	}

	/* we probably have one selector left to be added - so let's do that now */
	if(pCurrRule != NULL) {
		CHKiRet(ruleset.AddRule(rule.GetAssRuleset(pCurrRule), &pCurrRule));
	}

	/* close the configuration file */
	fclose(cf);

finalize_it:
	if(iRet != RS_RET_OK) {
		char errStr[1024];
		if(pCurrRule != NULL)
			rule.Destruct(&pCurrRule);

		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("error %d processing config file '%s'; os error (if any): %s\n",
			iRet, pConfFile, errStr);
	}

	free(pszOrgLine);

	if(bHadAnError && (iRet == RS_RET_OK)) { /* a bit dirty, enhance in future releases */
		iRet = RS_RET_NONFATAL_CONFIG_ERR;
	}
	RETiRet;
}


/* Helper to cfline() and its helpers. Parses a template name
 * from an "action" line. Must be called with the Line pointer
 * pointing to the first character after the semicolon.
 * rgerhards 2004-11-19
 * changed function to work with OMSR. -- rgerhards, 2007-07-27
 * the default template is to be used when no template is specified.
 */
rsRetVal cflineParseTemplateName(uchar** pp, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts, uchar *dfltTplName)
{
	uchar *p;
	uchar *tplName = NULL;
	cstr_t *pStrB;
	DEFiRet;

	ASSERT(pp != NULL);
	ASSERT(*pp != NULL);
	ASSERT(pOMSR != NULL);

	p =*pp;
	/* a template must follow - search it and complain, if not found */
	skipWhiteSpace(&p);
	if(*p == ';')
		++p; /* eat it */
	else if(*p != '\0' && *p != '#') {
		errmsg.LogError(0, RS_RET_ERR, "invalid character in selector line - ';template' expected");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	skipWhiteSpace(&p); /* go to begin of template name */

	if(*p == '\0' || *p == '#') {
		/* no template specified, use the default */
		/* TODO: check NULL ptr */
		tplName = (uchar*) strdup((char*)dfltTplName);
	} else {
		/* template specified, pick it up */
		CHKiRet(cstrConstruct(&pStrB));

		/* now copy the string */
		while(*p && *p != '#' && !isspace((int) *p)) {
			CHKiRet(cstrAppendChar(pStrB, *p));
			++p;
		}
		CHKiRet(cstrFinalize(pStrB));
		CHKiRet(cstrConvSzStrAndDestruct(pStrB, &tplName, 0));
	}

	CHKiRet(OMSRsetEntry(pOMSR, iEntry, tplName, iTplOpts));

finalize_it:
	if(iRet != RS_RET_OK)
		free(tplName);

	*pp = p;

	RETiRet;
}

/* Helper to cfline(). Parses a file name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template.
 * rgerhards 2004-11-18
 * parameter pFileName must point to a buffer large enough
 * to hold the largest possible filename.
 * rgerhards, 2007-07-25
 * updated to include OMSR pointer -- rgerhards, 2007-07-27
 * updated to include template name -- rgerhards, 2008-03-28
 * rgerhards, 2010-01-19: file names end at the first space
 */
rsRetVal
cflineParseFileName(uchar* p, uchar *pFileName, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts, uchar *pszTpl)
{
	register uchar *pName;
	int i;
	DEFiRet;

	ASSERT(pOMSR != NULL);

	pName = pFileName;
	i = 1; /* we start at 1 so that we reseve space for the '\0'! */
	while(*p && *p != ';' && *p != ' ' && i < MAXFNAME) {
		*pName++ = *p++;
		++i;
	}
	*pName = '\0';

	iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts, pszTpl);

	RETiRet;
}


/* Helper to cfline(). This function takes the filter part of a traditional, PRI
 * based line and decodes the PRIs given in the selector line. It processed the
 * line up to the beginning of the action part. A pointer to that beginnig is
 * passed back to the caller.
 * rgerhards 2005-09-15
 */
/* GPLv3 - stems back to sysklogd */
static rsRetVal cflineProcessTradPRIFilter(uchar **pline, register rule_t *pRule)
{
	uchar *p;
	register uchar *q;
	register int i, i2;
	uchar *bp;
	int pri;
	int singlpri = 0;
	int ignorepri = 0;
	uchar buf[2048]; /* buffer for facility and priority names */
	uchar xbuf[200];
	DEFiRet;

	ASSERT(pline != NULL);
	ASSERT(*pline != NULL);
	ISOBJ_TYPE_assert(pRule, rule);

	dbgprintf(" - traditional PRI filter\n");
	errno = 0;	/* keep strerror_r() stuff out of logerror messages */

	pRule->f_filter_type = FILTER_PRI;
	/* Note: file structure is pre-initialized to zero because it was
	 * created with calloc()!
	 */
	for (i = 0; i <= LOG_NFACILITIES; i++) {
		pRule->f_filterData.f_pmask[i] = TABLE_NOPRI;
	}

	/* scan through the list of selectors */
	for (p = *pline; *p && *p != '\t' && *p != ' ';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t ,;", *q) && bp < buf+sizeof(buf)-1 ; )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(",;", *q))
			q++;

		/* decode priority name */
		if ( *buf == '!' ) {
			ignorepri = 1;
			/* copy below is ok, we can NOT go off the allocated area */
			for (bp=buf; *(bp+1); bp++)
				*bp=*(bp+1);
			*bp='\0';
		}
		else {
			ignorepri = 0;
		}
		if ( *buf == '=' )
		{
			singlpri = 1;
			pri = decodeSyslogName(&buf[1], syslogPriNames);
		}
		else {
		        singlpri = 0;
			pri = decodeSyslogName(buf, syslogPriNames);
		}

		if (pri < 0) {
			snprintf((char*) xbuf, sizeof(xbuf), "unknown priority name \"%s\"", buf);
			errmsg.LogError(0, RS_RET_ERR, "%s", xbuf);
			return RS_RET_ERR;
		}

		/* scan facilities */
		while (*p && !strchr("\t .;", *p)) {
			for (bp = buf; *p && !strchr("\t ,;.", *p) && bp < buf+sizeof(buf)-1 ; )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*') {
				for (i = 0; i <= LOG_NFACILITIES; i++) {
					if ( pri == INTERNAL_NOPRI ) {
						if ( ignorepri )
							pRule->f_filterData.f_pmask[i] = TABLE_ALLPRI;
						else
							pRule->f_filterData.f_pmask[i] = TABLE_NOPRI;
					}
					else if ( singlpri ) {
						if ( ignorepri )
				  			pRule->f_filterData.f_pmask[i] &= ~(1<<pri);
						else
				  			pRule->f_filterData.f_pmask[i] |= (1<<pri);
					}
					else
					{
						if ( pri == TABLE_ALLPRI ) {
							if ( ignorepri )
								pRule->f_filterData.f_pmask[i] = TABLE_NOPRI;
							else
								pRule->f_filterData.f_pmask[i] = TABLE_ALLPRI;
						}
						else
						{
							if ( ignorepri )
								for (i2= 0; i2 <= pri; ++i2)
									pRule->f_filterData.f_pmask[i] &= ~(1<<i2);
							else
								for (i2= 0; i2 <= pri; ++i2)
									pRule->f_filterData.f_pmask[i] |= (1<<i2);
						}
					}
				}
			} else {
				i = decodeSyslogName(buf, syslogFacNames);
				if (i < 0) {

					snprintf((char*) xbuf, sizeof(xbuf), "unknown facility name \"%s\"", buf);
					errmsg.LogError(0, RS_RET_ERR, "%s", xbuf);
					return RS_RET_ERR;
				}

				if ( pri == INTERNAL_NOPRI ) {
					if ( ignorepri )
						pRule->f_filterData.f_pmask[i >> 3] = TABLE_ALLPRI;
					else
						pRule->f_filterData.f_pmask[i >> 3] = TABLE_NOPRI;
				} else if ( singlpri ) {
					if ( ignorepri )
						pRule->f_filterData.f_pmask[i >> 3] &= ~(1<<pri);
					else
						pRule->f_filterData.f_pmask[i >> 3] |= (1<<pri);
				} else {
					if ( pri == TABLE_ALLPRI ) {
						if ( ignorepri )
							pRule->f_filterData.f_pmask[i >> 3] = TABLE_NOPRI;
						else
							pRule->f_filterData.f_pmask[i >> 3] = TABLE_ALLPRI;
					} else {
						if ( ignorepri )
							for (i2= 0; i2 <= pri; ++i2)
								pRule->f_filterData.f_pmask[i >> 3] &= ~(1<<i2);
						else
							for (i2= 0; i2 <= pri; ++i2)
								pRule->f_filterData.f_pmask[i >> 3] |= (1<<i2);
					}
				}
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	*pline = p;
	RETiRet;
}


/* Helper to cfline(). This function processes an "if" type of filter,
 * what essentially means it parses an expression. As usual, 
 * It processes the line up to the beginning of the action part.
 * A pointer to that beginnig is passed back to the caller.
 * rgerhards 2008-01-19
 */
static rsRetVal cflineProcessIfFilter(uchar **pline, register rule_t *f)
{
	DEFiRet;
	ctok_t *tok;
	ctok_token_t *pToken;

	ASSERT(pline != NULL);
	ASSERT(*pline != NULL);
	ASSERT(f != NULL);

	dbgprintf(" - general expression-based filter\n");
	errno = 0;	/* keep strerror_r() stuff out of logerror messages */

	f->f_filter_type = FILTER_EXPR;

	/* if we come to over here, pline starts with "if ". We just skip that part. */
	(*pline) += 3;

	/* we first need a tokenizer... */
	CHKiRet(ctok.Construct(&tok));
	CHKiRet(ctok.Setpp(tok, *pline));
	CHKiRet(ctok.ConstructFinalize(tok));

	/* now construct our expression */
	CHKiRet(expr.Construct(&f->f_filterData.f_expr));
	CHKiRet(expr.ConstructFinalize(f->f_filterData.f_expr));

	/* ready to go... */
	CHKiRet(expr.Parse(f->f_filterData.f_expr, tok));

	/* we now need to parse off the "then" - and note an error if it is
	 * missing...
	 */
	CHKiRet(ctok.GetToken(tok, &pToken));
	if(pToken->tok != ctok_THEN) {
		ctok_token.Destruct(&pToken);
		ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
	}

	ctok_token.Destruct(&pToken); /* no longer needed */

	/* we are done, so we now need to restore things */
	CHKiRet(ctok.Getpp(tok, pline));
	CHKiRet(ctok.Destruct(&tok));

	/* debug support - print vmprg after construction (uncomment to use) */
	/* vmprgDebugPrint(f->f_filterData.f_expr->pVmprg); */

	/* we now need to skip whitespace to the action part, else we confuse
	 * the legacy rsyslog conf parser. -- rgerhards, 2008-02-25
	 */
	while(isspace(**pline))
		++(*pline);

finalize_it:
	if(iRet == RS_RET_SYNTAX_ERROR) {
		errmsg.LogError(0, RS_RET_SYNTAX_ERROR, "syntax error in expression");
	}

	RETiRet;
}


/* Helper to cfline(). This function takes the filter part of a property
 * based filter and decodes it. It processes the line up to the beginning
 * of the action part. A pointer to that beginnig is passed back to the caller.
 * rgerhards 2005-09-15
 */
static rsRetVal cflineProcessPropFilter(uchar **pline, register rule_t *f)
{
	rsParsObj *pPars;
	cstr_t *pCSCompOp;
	cstr_t *pCSPropName;
	rsRetVal iRet;
	int iOffset; /* for compare operations */

	ASSERT(pline != NULL);
	ASSERT(*pline != NULL);
	ASSERT(f != NULL);

	dbgprintf(" - property-based filter\n");
	errno = 0;	/* keep strerror_r() stuff out of logerror messages */

	f->f_filter_type = FILTER_PROP;

	/* create parser object starting with line string without leading colon */
	if((iRet = rsParsConstructFromSz(&pPars, (*pline)+1)) != RS_RET_OK) {
		errmsg.LogError(0, iRet, "Error %d constructing parser object - ignoring selector", iRet);
		return(iRet);
	}

	/* read property */
	iRet = parsDelimCStr(pPars, &pCSPropName, ',', 1, 1, 1);
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error %d parsing filter property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}
	iRet = propNameToID(pCSPropName, &f->f_filterData.prop.propID);
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error %d parsing filter property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}
	if(f->f_filterData.prop.propID == PROP_CEE) {
		/* in CEE case, we need to preserve the actual property name */
		if((f->f_filterData.prop.propName =
		     es_newStrFromBuf((char*)cstrGetSzStrNoNULL(pCSPropName)+2, cstrLen(pCSPropName)-2)) == NULL) {
			cstrDestruct(&pCSPropName);
			return(RS_RET_ERR);
		}
	}
	cstrDestruct(&pCSPropName);

	/* read operation */
	iRet = parsDelimCStr(pPars, &pCSCompOp, ',', 1, 1, 1);
	if(iRet != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error %d compare operation property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* we now first check if the condition is to be negated. To do so, we first
	 * must make sure we have at least one char in the param and then check the
	 * first one.
	 * rgerhards, 2005-09-26
	 */
	if(rsCStrLen(pCSCompOp) > 0) {
		if(*rsCStrGetBufBeg(pCSCompOp) == '!') {
			f->f_filterData.prop.isNegated = 1;
			iOffset = 1; /* ignore '!' */
		} else {
			f->f_filterData.prop.isNegated = 0;
			iOffset = 0;
		}
	} else {
		f->f_filterData.prop.isNegated = 0;
		iOffset = 0;
	}

dbgprintf("XXX: offset is %d, string '%s'\n", iOffset, rsCStrGetSzStrNoNULL(pCSCompOp));
	if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "contains", 8)) {
		f->f_filterData.prop.operation = FIOP_CONTAINS;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "isequal", 7)) {
		f->f_filterData.prop.operation = FIOP_ISEQUAL;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "isempty", 7)) {
		f->f_filterData.prop.operation = FIOP_ISEMPTY;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "startswith", 10)) {
		f->f_filterData.prop.operation = FIOP_STARTSWITH;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (unsigned char*) "regex", 5)) {
		f->f_filterData.prop.operation = FIOP_REGEX;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (unsigned char*) "ereregex", 8)) {
		f->f_filterData.prop.operation = FIOP_EREREGEX;
	} else {
		errmsg.LogError(0, NO_ERRCODE, "error: invalid compare operation '%s' - ignoring selector",
		           (char*) rsCStrGetSzStrNoNULL(pCSCompOp));
	}
	rsCStrDestruct(&pCSCompOp); /* no longer needed */

dbgprintf("XXX: fiop is %u\n", (unsigned) f->f_filterData.prop.operation);
	if(f->f_filterData.prop.operation != FIOP_ISEMPTY) {
		/* read compare value */
		iRet = parsQuotedCStr(pPars, &f->f_filterData.prop.pCSCompValue);
		if(iRet != RS_RET_OK) {
			errmsg.LogError(0, iRet, "error %d compare value property - ignoring selector", iRet);
			rsParsDestruct(pPars);
			return(iRet);
		}
	}

	/* skip to action part */
	if((iRet = parsSkipWhitespace(pPars, 1)) != RS_RET_OK) {
		errmsg.LogError(0, iRet, "error %d skipping to action part - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* cleanup */
	*pline = *pline + rsParsGetParsePointer(pPars) + 1;
		/* we are adding one for the skipped initial ":" */

	return rsParsDestruct(pPars);
}


/*
 * Helper to cfline(). This function interprets a BSD host selector line
 * from the config file ("+/-hostname"). It stores it for further reference.
 * rgerhards 2005-10-19
 */
static rsRetVal cflineProcessHostSelector(uchar **pline)
{
	DEFiRet;

	ASSERT(pline != NULL);
	ASSERT(*pline != NULL);
	ASSERT(**pline == '-' || **pline == '+');

	dbgprintf(" - host selector line\n");

	/* check include/exclude setting */
	if(**pline == '+') {
		eDfltHostnameCmpMode = HN_COMP_MATCH;
	} else { /* we do not check for '-', it must be, else we wouldn't be here */
		eDfltHostnameCmpMode = HN_COMP_NOMATCH;
	}
	(*pline)++;	/* eat + or - */

	/* the below is somewhat of a quick hack, but it is efficient (this is
	 * why it is in here. "+*" resets the tag selector with BSD syslog. We mimic
	 * this, too. As it is easy to check that condition, we do not fire up a
	 * parser process, just make sure we do not address beyond our space.
	 * Order of conditions in the if-statement is vital! rgerhards 2005-10-18
	 */
	if(**pline != '\0' && **pline == '*' && *(*pline+1) == '\0') {
		dbgprintf("resetting BSD-like hostname filter\n");
		eDfltHostnameCmpMode = HN_NO_COMP;
		if(pDfltHostnameCmp != NULL) {
			CHKiRet(rsCStrSetSzStr(pDfltHostnameCmp, NULL));
		}
	} else {
		dbgprintf("setting BSD-like hostname filter to '%s'\n", *pline);
		if(pDfltHostnameCmp == NULL) {
			/* create string for parser */
			CHKiRet(rsCStrConstructFromszStr(&pDfltHostnameCmp, *pline));
		} else { /* string objects exists, just update... */
			CHKiRet(rsCStrSetSzStr(pDfltHostnameCmp, *pline));
		}
	}

finalize_it:
	RETiRet;
}


/*
 * Helper to cfline(). This function interprets a BSD tag selector line
 * from the config file ("!tagname"). It stores it for further reference.
 * rgerhards 2005-10-18
 */
static rsRetVal cflineProcessTagSelector(uchar **pline)
{
	DEFiRet;

	ASSERT(pline != NULL);
	ASSERT(*pline != NULL);
	ASSERT(**pline == '!');

	dbgprintf(" - programname selector line\n");

	(*pline)++;	/* eat '!' */

	/* the below is somewhat of a quick hack, but it is efficient (this is
	 * why it is in here. "!*" resets the tag selector with BSD syslog. We mimic
	 * this, too. As it is easy to check that condition, we do not fire up a
	 * parser process, just make sure we do not address beyond our space.
	 * Order of conditions in the if-statement is vital! rgerhards 2005-10-18
	 */
	if(**pline != '\0' && **pline == '*' && *(*pline+1) == '\0') {
		dbgprintf("resetting programname filter\n");
		if(pDfltProgNameCmp != NULL) {
			rsCStrDestruct(&pDfltProgNameCmp);
		}
	} else {
		dbgprintf("setting programname filter to '%s'\n", *pline);
		if(pDfltProgNameCmp == NULL) {
			/* create string for parser */
			CHKiRet(rsCStrConstructFromszStr(&pDfltProgNameCmp, *pline));
		} else { /* string objects exists, just update... */
			CHKiRet(rsCStrSetSzStr(pDfltProgNameCmp, *pline));
		}
	}

finalize_it:
	RETiRet;
}


/* read the filter part of a configuration line and store the filter
 * in the supplied rule_t
 * rgerhards, 2007-08-01
 */
static rsRetVal cflineDoFilter(uchar **pp, rule_t *f)
{
	DEFiRet;

	ASSERT(pp != NULL);
	ISOBJ_TYPE_assert(f, rule);

	/* check which filter we need to pull... */
	switch(**pp) {
		case ':':
			CHKiRet(cflineProcessPropFilter(pp, f));
			break;
		case 'i': /* "if" filter? */
			if(*(*pp+1) && (*(*pp+1) == 'f') && isspace(*(*pp+2))) {
				CHKiRet(cflineProcessIfFilter(pp, f));
				break;
				}
			/*FALLTHROUGH*/
		default:
			CHKiRet(cflineProcessTradPRIFilter(pp, f));
			break;
	}

	/* we now check if there are some global (BSD-style) filter conditions
	 * and, if so, we copy them over. rgerhards, 2005-10-18
	 */
	if(pDfltProgNameCmp != NULL) {
		CHKiRet(rsCStrConstructFromCStr(&(f->pCSProgNameComp), pDfltProgNameCmp));
	}

	if(eDfltHostnameCmpMode != HN_NO_COMP) {
		f->eHostnameCmpMode = eDfltHostnameCmpMode;
		CHKiRet(rsCStrConstructFromCStr(&(f->pCSHostnameComp), pDfltHostnameCmp));
	}

finalize_it:
	RETiRet;
}


/* process the action part of a selector line
 * rgerhards, 2007-08-01
 */
static rsRetVal cflineDoAction(uchar **p, action_t **ppAction)
{
	DEFiRet;
	modInfo_t *pMod;
	omodStringRequest_t *pOMSR;
	action_t *pAction = NULL;
	void *pModData;

	ASSERT(p != NULL);
	ASSERT(ppAction != NULL);

	/* loop through all modules and see if one picks up the line */
	pMod = module.GetNxtType(NULL, eMOD_OUT);
	/* Note: clang static analyzer reports that pMod mybe == NULL. However, this is
	 * not possible, because we have the built-in output modules which are always
	 * present. Anyhow, we guard this by an assert. -- rgerhards, 2010-12-16
	 */
	assert(pMod != NULL);
	while(pMod != NULL) {
		pOMSR = NULL;
		iRet = pMod->mod.om.parseSelectorAct(p, &pModData, &pOMSR);
		dbgprintf("tried selector action for %s: %d\n", module.GetName(pMod), iRet);
		if(iRet == RS_RET_OK || iRet == RS_RET_SUSPENDED) {
			/* advance our config parser state: we now only accept an $End as valid,
			 * no more action statments.
			 */
			if(currConfObj == eConfObjAction)
				currConfObj = eConfObjActionWaitEnd;
			if((iRet = addAction(&pAction, pMod, pModData, pOMSR, (iRet == RS_RET_SUSPENDED)? 1 : 0)) == RS_RET_OK) {
				/* now check if the module is compatible with select features */
				if(pMod->isCompatibleWithFeature(sFEATURERepeatedMsgReduction) == RS_RET_OK)
					pAction->f_ReduceRepeated = bReduceRepeatMsgs;
				else {
					dbgprintf("module is incompatible with RepeatedMsgReduction - turned off\n");
					pAction->f_ReduceRepeated = 0;
				}
				pAction->eState = ACT_STATE_RDY; /* action is enabled */
				iNbrActions++;	/* one more active action! */
			}
			break;
		}
		else if(iRet != RS_RET_CONFLINE_UNPROCESSED) {
			/* In this case, the module would have handled the config
			 * line, but some error occured while doing so. This error should
			 * already by reported by the module. We do not try any other
			 * modules on this line, because we found the right one.
			 * rgerhards, 2007-07-24
			 */
			dbgprintf("error %d parsing config line\n", (int) iRet);
			break;
		}
		pMod = module.GetNxtType(pMod, eMOD_OUT);
	}

	*ppAction = pAction;
	RETiRet;
}


/* Process a configuration file line in traditional "filter selector" format
 * or one that builds upon this format. Note that ppRule may be a NULL pointer,
 * which is valid and happens if there is no previous line (right at the start
 * of the master config file!).
 */
static rsRetVal
cflineClassic(uchar *p, rule_t **ppRule)
{
	DEFiRet;
	action_t *pAction;

	/* lines starting with '&' have no new filters and just add
	 * new actions to the currently processed selector.
	 */
	if(*p == '&') {
		++p; /* eat '&' */
		skipWhiteSpace(&p); /* on to command */
	} else {
		/* we are finished with the current selector (on previous line).
		 * So we now need to check
		 * if it has any actions associated and, if so, link it to the linked
		 * list. If it has nothing associated with it, we can simply discard
		 * it. In any case, we create a fresh selector for our new filter.
		 * We have one special case during initialization: then, the current
		 * selector is NULL, which means we do not need to care about it at
		 * all.  -- rgerhards, 2007-08-01
		 */
		if(*ppRule != NULL) {
			CHKiRet(ruleset.AddRule(rule.GetAssRuleset(*ppRule), ppRule));
		}
		CHKiRet(rule.Construct(ppRule)); /* create "fresh" selector */
		CHKiRet(rule.SetAssRuleset(*ppRule, ruleset.GetCurrent())); /* create "fresh" selector */
		CHKiRet(rule.ConstructFinalize(*ppRule)); /* create "fresh" selector */
		CHKiRet(cflineDoFilter(&p, *ppRule)); /* pull filters */
	}

	CHKiRet(cflineDoAction(&p, &pAction));
	CHKiRet(llAppend(&(*ppRule)->llActList,  NULL, (void*) pAction));

finalize_it:
	RETiRet;
}


/* process a configuration line
 * I re-did this functon because it was desperately time to do so
 * rgerhards, 2007-08-01
 */
static rsRetVal
cfline(uchar *line, rule_t **pfCurr)
{
	DEFiRet;

	ASSERT(line != NULL);

	dbgprintf("cfline: '%s'\n", line);

	/* check type of line and call respective processing */
	switch(*line) {
		case '!':
			iRet = cflineProcessTagSelector(&line);
			break;
		case '+':
		case '-':
			iRet = cflineProcessHostSelector(&line);
			break;
		case '$':
			++line; /* eat '$' */
			iRet = cfsysline(line);
			break;
		default:
			iRet = cflineClassic(line, pfCurr);
			break;
	}

	RETiRet;
}


/* return the current number of active actions
 * rgerhards, 2008-07-28
 */
static rsRetVal
GetNbrActActions(int *piNbrActions)
{
	DEFiRet;
	assert(piNbrActions != NULL);
	*piNbrActions = iNbrActions;
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-29
 */
BEGINobjQueryInterface(conf)
CODESTARTobjQueryInterface(conf)
	if(pIf->ifVersion != confCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->doNameLine = doNameLine;
	pIf->cfsysline = cfsysline;
	pIf->doModLoad = doModLoad;
	pIf->doIncludeLine = doIncludeLine;
	pIf->cfline = cfline;
	pIf->processConfFile = processConfFile;
	pIf->GetNbrActActions = GetNbrActActions;

finalize_it:
ENDobjQueryInterface(conf)


/* switch to a new action scope. This means that we switch the current 
 * mode to action, but it also means we need to clear all scope variables,
 * so that we have a new environment.
 * rgerhards, 2010-07-23
 */
static inline rsRetVal
setActionScope(void)
{
	DEFiRet;
	modInfo_t *pMod;

	currConfObj = eConfObjAction;
	DBGPRINTF("entering action scope\n");
	CHKiRet(actionNewScope());

	/* now tell each action to start the scope */
	pMod = NULL;
	while((pMod = module.GetNxtType(pMod, eMOD_OUT)) != NULL) {
		DBGPRINTF("NO LONGER SUPPORTED beginning scope on module %s\n", pMod->pszName);
	}

finalize_it:
	RETiRet;
}


/* switch back from action scope.
 * rgerhards, 2010-07-27
 */
static inline rsRetVal
unsetActionScope(void)
{
	DEFiRet;
	modInfo_t *pMod;

	currConfObj = eConfObjAction;
	DBGPRINTF("exiting action scope\n");
	CHKiRet(actionRestoreScope());

	/* now tell each action to restore the scope */
	pMod = NULL;
	while((pMod = module.GetNxtType(pMod, eMOD_OUT)) != NULL) {
		DBGPRINTF("NO LONGER SUPPORTED exiting scope on module %s\n", pMod->pszName);
	}

finalize_it:
	RETiRet;
}


/* This method is called by our own handlers to begin a new config
 * object ($Begin statement). This also implies a new scope.
 * rgerhards, 2010-07-23
 */
static rsRetVal
beginConfObj(void __attribute__((unused)) *pVal, uchar *pszName)
{
	DEFiRet;

	if(currConfObj != eConfObjGlobal) {
		errmsg.LogError(0, RS_RET_CONF_NOT_GLBL, "not in global scope - can not nest $Begin");
		ABORT_FINALIZE(RS_RET_CONF_NOT_GLBL);
	}

	if(!strcasecmp((char*)pszName, "action")) {
		setActionScope();
	} else {
		errmsg.LogError(0, RS_RET_INVLD_CONF_OBJ, "invalid config object \"%s\" in $Begin", pszName);
		ABORT_FINALIZE(RS_RET_INVLD_CONF_OBJ);
	}

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* This method is called to end a config scope and switch
 * back to global scope.
 * rgerhards, 2010-07-23
 */
static rsRetVal
endConfObj(void __attribute__((unused)) *pVal, uchar *pszName)
{
	DEFiRet;

	if(currConfObj == eConfObjGlobal) {
		errmsg.LogError(0, RS_RET_CONF_NOT_GLBL, "already in global scope - dangling $End");
		ABORT_FINALIZE(RS_RET_CONF_IN_GLBL);
	}

	if(!strcasecmp((char*)pszName, "action")) {
		if(currConfObj == eConfObjAction) {
			errmsg.LogError(0, RS_RET_CONF_END_NO_ACT, "$End action but not action specified");
			/* this is a warning, we continue processing in that case (unscope) */
		} else if(currConfObj != eConfObjActionWaitEnd) {
			errmsg.LogError(0, RS_RET_CONF_INVLD_END, "$End not for active config object - "
							          "nesting error?");
			ABORT_FINALIZE(RS_RET_CONF_INVLD_END);
		}
		currConfObj = eConfObjGlobal;
		CHKiRet(unsetActionScope());
	} else {
		errmsg.LogError(0, RS_RET_INVLD_CONF_OBJ, "invalid config object \"%s\" in $End", pszName);
		ABORT_FINALIZE(RS_RET_INVLD_CONF_OBJ);
	}

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* Reset config variables to default values. Note that
 * when we are inside an scope, we simply reset this to global.
 * However, $ResetConfigVariables is a global directive, and as such
 * will not be honored inside a scope!
 * rgerhards, 2010-07-23
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	currConfObj = eConfObjGlobal;
	bConfStrictScoping = 0;
	return RS_RET_OK;
}


/* exit our class
 * rgerhards, 2008-03-11
 */
BEGINObjClassExit(conf, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(conf)
	/* free no-longer needed module-global variables */
	if(pDfltHostnameCmp != NULL) {
		rsCStrDestruct(&pDfltHostnameCmp);
	}

	if(pDfltProgNameCmp != NULL) {
		rsCStrDestruct(&pDfltProgNameCmp);
	}

	/* release objects we no longer need */
	objRelease(expr, CORE_COMPONENT);
	objRelease(ctok, CORE_COMPONENT);
	objRelease(ctok_token, CORE_COMPONENT);
	objRelease(module, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(rule, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
ENDObjClassExit(conf)


/* Initialize our class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-29
 */
BEGINAbstractObjClassInit(conf, 1, OBJ_IS_CORE_MODULE) /* class, version - CHANGE class also in END MACRO! */
	/* request objects we use */
	CHKiRet(objUse(expr, CORE_COMPONENT));
	CHKiRet(objUse(ctok, CORE_COMPONENT));
	CHKiRet(objUse(ctok_token, CORE_COMPONENT));
	CHKiRet(objUse(module, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME)); /* TODO: make this dependcy go away! */
	CHKiRet(objUse(rule, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));

 	/* These commands will NOT be supported -- the new v6.3 config system provides
	 * far better methods. We will remove the related code soon. -- rgerhards, 2012-01-09
	 */
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));
ENDObjClassInit(conf)

/* vi:set ai:
 */
