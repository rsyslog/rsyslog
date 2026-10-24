/* SPDX-License-Identifier: Apache-2.0 */

/* rsyslog parser interface.
 *
 * Copyright 2011-2016 Rainer Gerhards
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
#ifndef PARSERIF_H_DEFINED
#define PARSERIF_H_DEFINED
#include "rainerscript.h"
/**
 * @brief Parse the current scanner input under serialized parser ownership.
 * @return Zero on acceptance, one on syntax failure, or two on exhaustion.
 * The caller must also consume cnfTakeFatalParseError(): scanner failures
 * remain fatal independently of the parser's return code. This declaration
 * is project-owned because supported Bison versions differ in whether their
 * generated headers declare yyparse(). Only the generated grammar calls it.
 */
int cnfParse(void);
int cnfSetLexFile(const char *);
void parser_errmsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void parser_warnmsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tellLexEndParsing(void);
/**
 * @brief Release all lexer buffers and reset start conditions after private parsing.
 * Safe after successful or failed yyparse(); no caller-owned AST is destroyed.
 * Caller must own serialized parser access. No error is returned and no process
 * exit occurs; fatal load errors are retrieved with cnfTakeFatalParseError().
 */
void cnfResetParser(void);
/* Returns non-zero when there are config buffers queued for yyparse() to
 * consume.  Used by rsconf.c to run yyparse() after yamlconf_load() has pushed
 * synthesised RainerScript script: blocks via cnfAddConfigBuffer(). */
int cnfHasPendingBuffers(void);
#ifndef IN_GRAMMAR_Y
extern char *cnfcurrfn;
extern FILE *fp_rs_full_conf_output;
#endif

/* entry points to be called after the parser has processed the
 * element in question. Actual processing must than be done inside
 * these functions.
 */
void cnfDoObj(struct cnfobj *o);
void cnfDoScript(struct cnfstmt *script);
void cnfDoCfsysline(char *ln);
void cnfDoBSDTag(char *ln);
void cnfDoBSDHost(char *ln);
/* Push a pre-built RainerScript text buffer onto the flex buffer stack so that
 * yyparse() will process it after the current input is exhausted (LIFO order).
 * cnfobjname is either the source file name or a descriptive label (e.g. the
 * ruleset name) used in error messages.
 * IMPORTANT: str must have two trailing NUL bytes appended (required by
 * yy_scan_buffer()).  build_ruleset_rainerscript() in yamlconf.c does this.
 * Ownership of str is transferred; it is freed when the buffer is popped. */
int cnfAddConfigBuffer(es_str_t *const str, const char *const cnfobj_name);
#endif
