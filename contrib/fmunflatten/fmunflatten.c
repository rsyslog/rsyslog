/*
 * This is a function module providing ability to unflatten a JSON tree.
 *
 * Copyright 2020 Julien Thomas < jthomas @ zenetys.com >
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

#include <stdint.h>
#include <stddef.h>
#ifndef _AIX
#include <typedefs.h>
#endif
#include <sys/types.h>
#include <string.h>

#include "rsyslog.h"

#include "errmsg.h"
#include "msg.h"
#include "parserif.h"
#include "module-template.h"
#include "rainerscript.h"
#include "wti.h"

#define FMUNFLATTEN_KBUFLEN 256
#define _jso_type(x) json_type_to_name(json_object_get_type(x))

MODULE_TYPE_FUNCTION
MODULE_TYPE_NOKEEP
DEF_FMOD_STATIC_DATA

struct unflatten_ctx {
	char *kbuf;
	size_t kbuf_len;
	char delim;
};

/* forward declarations */
void unflatten_add(struct unflatten_ctx *ctx, struct json_object *dst, const char *key, struct json_object *value);
void unflatten(struct unflatten_ctx *ctx, struct json_object *src, struct json_object *dst);

void unflatten_add(struct unflatten_ctx *ctx,
	struct json_object *dst,
	const char *key,
	struct json_object *value)
{
	const char *p = key;
	const char *q = p;
	int depth = 0;
	size_t klen;
	struct json_object *o;
	json_bool exists_already;
	int create;

	while (1) {
		while (*q != ctx->delim && *q != '\0')
			q++;
		klen = q - p;
		if (klen + 1 > ctx->kbuf_len) {
			DBGPRINTF("warning: flat key \"%.20s...\" truncated at depth #%d, buffer too "
				"small (max %zd)\n", key, depth, ctx->kbuf_len);
			klen = ctx->kbuf_len - 1;
		}
		memcpy(ctx->kbuf, p, klen);
		ctx->kbuf[klen] = '\0';
		exists_already = json_object_object_get_ex(dst, ctx->kbuf, &o);

		if (*q == ctx->delim) {
			if (!exists_already)
				create = 1;
			else if (json_object_is_type(o, json_type_object))
				create = 0;
			else {
				DBGPRINTF("warning: while processing flat key \"%s\" at depth #%d (intermediate "
					"node), overriding existing value of type %s by an object\n", key, depth,
					_jso_type(o));
				json_object_object_del(dst, ctx->kbuf);
				create = 1;
			}
			if (create) {
				o = json_object_new_object();
				json_object_object_add(dst, ctx->kbuf, o);
			}
			dst = o;
			p = q + 1;
			q = p;
			depth++;
		}
		else if (*q == '\0') {
			if (json_object_is_type(value, json_type_object)) {
				if (exists_already) {
					if (!json_object_is_type(o, json_type_object)) {
						DBGPRINTF("warning: while processing flat key \"%s\" at depth #%d "
							"(final node), overriding existing value of type %s by an "
							"object\n", key, depth, _jso_type(o));
						json_object_object_del(dst, ctx->kbuf);
						o = json_object_new_object();
						json_object_object_add(dst, ctx->kbuf, o);
					}
				}
				else {
					o = json_object_new_object();
					json_object_object_add(dst, ctx->kbuf, o);
				}
				unflatten(ctx, value, o);
			}
			else {
				if (exists_already) {
					DBGPRINTF("warning: while processing flat key \"%s\" at depth #%d "
						"(final node), overriding existing value\n", key, depth);
					json_object_object_del(dst, ctx->kbuf);
				}
				o = jsonDeepCopy(value);
				json_object_object_add(dst, ctx->kbuf, o);
			}
			break;
		}
	}
}

void unflatten(struct unflatten_ctx *ctx,
	struct json_object *src,
	struct json_object *dst)
{
	struct json_object_iterator it = json_object_iter_begin(src);
	struct json_object_iterator itEnd = json_object_iter_end(src);
	const char *key;
	struct json_object *value;

	while (!json_object_iter_equal(&it, &itEnd)) {
		key = json_object_iter_peek_name(&it);
		value = json_object_iter_peek_value(&it);
		unflatten_add(ctx, dst, key, value);
		json_object_iter_next(&it);
	}
}

static void ATTR_NONNULL()
doFunc_unflatten(struct cnffunc *__restrict__ const func,
	struct svar *__restrict__ const ret,
	void *__restrict__ const usrptr,
	wti_t *__restrict__ const pWti)
{
	struct svar src_var;
	struct svar delim_var;

	char kbuf[FMUNFLATTEN_KBUFLEN];
	struct unflatten_ctx ctx = {
		.kbuf = kbuf,
		.kbuf_len = sizeof(kbuf),
		.delim = 0
	};

	/* A dummy value of 0 (number) is returned by default. Callers should also
	 * call script_error() to check if this script function succeeded before
	 * using the value it returns. */
	ret->datatype = 'N';
	ret->d.n = 0;
	wtiSetScriptErrno(pWti, RS_SCRIPT_EINVAL);

	cnfexprEval(func->expr[0], &src_var, usrptr, pWti);
	cnfexprEval(func->expr[1], &delim_var, usrptr, pWti);

	/* Check argument 2 (delimiter character). */
	if (delim_var.datatype == 'S' && es_strlen(delim_var.d.estr) == 1)
		ctx.delim = *es_getBufAddr(delim_var.d.estr);
	else if (delim_var.datatype == 'N')
		ctx.delim = delim_var.d.n;
	if (ctx.delim == 0) {
		LogError(0, RS_RET_INVALID_PARAMS, "unflatten: invalid argument 2 (delim), single character "
			"required (as string or decimal charcode)");
		FINALIZE;
	}

	/* Check argument 1 (source). Not logging an error avoids emitting logs for
	 * messages when $! was not touched. */
	if (src_var.datatype != 'J') {
		DBGPRINTF("unsupported argument 1 (src) datatype %c\n", src_var.datatype);
		FINALIZE;
	}

	ret->datatype = 'J';
	if (json_object_is_type(src_var.d.json, json_type_object)) {
		ret->d.json = json_object_new_object();
		unflatten(&ctx, src_var.d.json, ret->d.json);
	}
	else
		ret->d.json = jsonDeepCopy(src_var.d.json);
	wtiSetScriptErrno(pWti, RS_SCRIPT_EOK);

finalize_it:
	varFreeMembers(&src_var);
	varFreeMembers(&delim_var);
}

static rsRetVal ATTR_NONNULL(1)
initFunc_unflatten(struct cnffunc *const func)
{
	DEFiRet;
	func->destructable_funcdata = 0;
	RETiRet;
}

static struct scriptFunct functions[] = {
	{ "unflatten", 2, 2, doFunc_unflatten, initFunc_unflatten, NULL },
	{ NULL, 0, 0, NULL, NULL, NULL } /* last element to check end of array */
};

BEGINgetFunctArray
CODESTARTgetFunctArray
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
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	dbgprintf("rsyslog fmunflatten init called, compiled with version %s\n", VERSION);
ENDmodInit
