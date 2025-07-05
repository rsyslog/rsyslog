/*
 * function module to masks a Primary Account Number (PAN) according to PCI DSS standards.
 *
 * Copyright 2025 Arnaud Grandville < agrandville @ gmail.com >
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	   http://www.apache.org/licenses/LICENSE-2.0
 *	   -or-
 *	   see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#ifndef _AIX
#include <typedefs.h>
#endif
#include <sys/types.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "rsyslog.h"
#include "parserif.h"
#include "module-template.h"
#include "rainerscript.h"
#include "glbl.h"
#define MAXLINE 8192

#if defined(__GNUC__) || defined(__clang__)
static const char RCSID[] __attribute__((used)) = "@(#)fmpansanitizer v0.9 2025-06-29";
#else
static const char RCSID[] = "@(#)fmpansanitizer v0.9 2025-06-29";
#endif

MODULE_TYPE_FUNCTION
MODULE_TYPE_NOKEEP
DEF_FMOD_STATIC_DATA

struct maskpan_state
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	pcre2_match_context *match_context;
};

static int ATTR_NONNULL()
	luhn_check_cb(pcre2_substitute_callout_block *cb, void *unused)
{
	(void)unused; // Avoid unused parameter warning
	const unsigned char *number = cb->input + cb->ovector[0];
	int len = (int)(cb->ovector[1] - cb->ovector[0]);
	int sum = 0;
	int alt = 0;

	if (len < 1)
		return 1;

	int i = len - 1;
	do
	{
		if ((unsigned char)number[i] < '0' ||
			(unsigned char)number[i] > '9')
		{
			return 1; // Invalid character
		}

		int digit = number[i] - '0';

		if (alt)
		{
			digit *= 2;
			if (digit > 9)
				digit -= 9;
		}

		sum += digit;
		alt = !alt;
	} while (--i >= 0);

	return (sum % 10) != 0;
}

static rsRetVal ATTR_NONNULL()
	doFunc_maskpan(struct cnffunc *__restrict__ const func,
				   struct svar *__restrict__ const ret,
				   void *__restrict__ const usrptr,
				   wti_t *__restrict__ const pWti)
{
	DEFiRet;
	struct maskpan_state *state = (struct maskpan_state *)func->funcdata;
	struct svar sSubject, sReplacement;
	int bMustFreeSub, bMustFreeReplace;
	char *pszSubject, *pszReplace;
	// PCRE2_SPTR replacement = (PCRE2_SPTR)"$1******$2";

	cnfexprEval(func->expr[0], &sSubject, usrptr, pWti);
	cnfexprEval(func->expr[2], &sReplacement, usrptr, pWti);

	pszSubject = (char *)var2CString(&sSubject, &bMustFreeSub);
	pszReplace = (char *)var2CString(&sReplacement, &bMustFreeReplace);

	char output[MAXLINE] = {0};
	PCRE2_SIZE outlen = MAXLINE;

	int rc = pcre2_substitute(state->re,
							  (PCRE2_SPTR)pszSubject, strlen(pszSubject),
							  0,
							  PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_EXTENDED,
							  state->match_data, state->match_context,
							  (PCRE2_SPTR)pszReplace, strlen(pszReplace),
							  (PCRE2_UCHAR *)output, &outlen);

	if (rc < 0 && rc != PCRE2_ERROR_NOMATCH)
	{
		dbgprintf("fmpansanitizer: pcre2_substitute failed with error %d\n", rc);
		/* On error, output is not guaranteed to be valid. */
		/* We'll proceed with an empty string in 'output'. */
		output[0] = '\0';
	}

	dbgprintf("fmpansanitizer() result '%s'\n", output);

	CHKmalloc(ret->d.estr = es_newStrFromCStr(output, strlen(output)));
	ret->datatype = 'S';

finalize_it:
	if (bMustFreeSub)
	{
		free(pszSubject);
		pszSubject = NULL;
	}
	if (bMustFreeReplace)
	{
		free(pszReplace);
		pszReplace = NULL;
	}

	varFreeMembers(&sSubject);
	varFreeMembers(&sReplacement);

	RETiRet;
}

static rsRetVal ATTR_NONNULL(1)
	initFunc_maskpan(struct cnffunc *const func)
{
	DEFiRet;
	const char *err;
	int erroffset;
	char *regex = NULL;
	struct maskpan_state *state;

	if (func->nParams != 3)
	{
		parser_errmsg("maskpan error : Expected 3 arguments, got %i", func->nParams);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if (func->expr[1]->nodetype != 'S')
	{
		parser_errmsg("param 2 of maskpan() must be a constant string");
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}
	if (func->expr[2]->nodetype != 'S')
	{
		parser_errmsg("param 3 of maskpan() must be a constant string");
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}

	CHKmalloc(state = calloc(1, sizeof(struct maskpan_state)));
	regex = es_str2cstr(((struct cnfstringval *)func->expr[1])->estr, NULL);

	state->re = pcre2_compile((PCRE2_SPTR)regex,
							  PCRE2_ZERO_TERMINATED,
							  0,
							  &err, &erroffset, NULL);

	if (state->re == NULL)
	{
		parser_errmsg("pcre compilation failed at offset %d: %s", erroffset, err);
		free(state);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	state->match_context = pcre2_match_context_create(NULL);
	state->match_data = pcre2_match_data_create_from_pattern(state->re, NULL);

	pcre2_set_substitute_callout(state->match_context, *luhn_check_cb, NULL);

	func->funcdata = state;
	func->destructable_funcdata = 1;

finalize_it:
	free(regex);
	RETiRet;
}

static void ATTR_NONNULL()
	destruct_maskpan(struct cnffunc *const func)
{

	struct maskpan_state *state = (struct maskpan_state *)func->funcdata;
	if (state != NULL)
	{
		if (state->re != NULL)
			pcre2_code_free(state->re);
		if (state->match_data != NULL)
			pcre2_match_data_free(state->match_data);
		if (state->match_context != NULL)
			pcre2_match_context_free(state->match_context);

		free(state);
		func->funcdata = NULL;
	}
}

static struct scriptFunct functions[] = {
	{"maskpan", 3, 3, doFunc_maskpan, initFunc_maskpan, destruct_maskpan},
	{NULL, 0, 0, NULL, NULL, NULL}};

BEGINgetFunctArray CODESTARTgetFunctArray
	*version = 1;
*functArray = functions;
ENDgetFunctArray

BEGINmodExit
CODESTARTmodExit
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_FMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	dbgprintf("rsyslog fmpansanitizer init called, compiled with version %s\n", VERSION);
ENDmodInit
