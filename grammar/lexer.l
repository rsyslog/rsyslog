 /* Lex file for rsyslog config format v2 (RainerScript).
  * Please note: this file introduces the new config format, but maintains
  * backward compatibility. In order to do so, the grammar is not 100% clean,
  * but IMHO still sufficiently easy both to understand for programmers
  * maitaining the code as well as users writing the config file. Users are,
  * of course, encouraged to use new constructs only. But it needs to be noted
  * that some of the legacy constructs (specifically the in-front-of-action
  * PRI filter) are very hard to beat in ease of use, at least for simpler
  * cases. So while we hope that cfsysline support can be dropped some time in
  * the future, we will probably keep these useful constructs.
  *
  * Copyright 2011 Rainer Gerhards and Adiscon GmbH.
  *
  * This file is part of the rsyslog runtime library.
  *
  * The rsyslog runtime library is free software: you can redistribute it and/or modify
  * it under the terms of the GNU Lesser General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * The rsyslog runtime library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
  *
  * A copy of the GPL can be found in the file "COPYING" in this distribution.
  * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
  */

%option noyywrap nodefault case-insensitive yylineno
 /*%option noyywrap nodefault case-insensitive */

/* avoid compiler warning: `yyunput' defined but not used */
%option nounput noinput


%x INOBJ
	/* INOBJ is selected if we are inside an object (name/value pairs!) */
%x COMMENT
	/* COMMENT is "the usual trick" to handle C-style comments */
%x INCL
	/* INCL is in $IncludeConfig processing (skip to include file) */
%x LINENO
	/* LINENO: support for setting the linenumber */
%x EXPR
	/* EXPR is a bit ugly, but we need it to support pre v6-syntax. The problem
	 * is that cfsysline statement start with $..., the same like variables in
	 * an expression. However, cfsysline statements can never appear inside an
	 * expression. So we create a specific expr mode, which is turned on after 
	 * we lexed a keyword that needs to be followed by an expression (using 
	 * knowledge from the upper layer...). In expr mode, we strictly do
	 * expression-based parsing. Expr mode is stopped when we reach a token
	 * that can not be part of an expression (currently only "then"). As I
	 * wrote this ugly, but the price needed to pay in order to remain
	 * compatible to the previous format.
	 */
%{
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <libestr.h>
#include "rainerscript.h"
#include "grammar.h"
static int preCommentState;	/* save for lex state before a comment */

struct bufstack {
	struct bufstack *prev;
	YY_BUFFER_STATE bs;
	int lineno;
	char *fn;
	es_str_t *estr;
} *currbs = NULL;

char *cnfcurrfn;			/* name of currently processed file */

int popfile(void);
int cnfSetLexFile(char *fname);

extern int yydebug;

/* somehow, I need these prototype even though the headers are 
 * included. I guess that's some autotools magic I don't understand...
 */
//char *strdup(char*);
int fileno(FILE *stream);

%}

%%

 /* keywords */
"if"				{ BEGIN EXPR; return IF; }
<EXPR>"then"			{ BEGIN INITIAL; return THEN; }
<EXPR>"or"			{ return OR; }
<EXPR>"and"			{ return AND; }
<EXPR>"not"			{ return NOT; }
<EXPR>"," |
<EXPR>"*" |
<EXPR>"/" |
<EXPR>"%" |
<EXPR>"+" |
<EXPR>"-" |
<EXPR>"(" |
<EXPR>")"			{ return yytext[0]; }
<EXPR>"=="			{ return CMP_EQ; }
<EXPR>"<="			{ return CMP_LE; }
<EXPR>">="			{ return CMP_GE; }
<EXPR>"!=" |
<EXPR>"<>"			{ return CMP_NE; }
<EXPR>"<"			{ return CMP_LT; }
<EXPR>">"			{ return CMP_GT; }
<EXPR>"contains"		{ return CMP_CONTAINS; }
<EXPR>"contains_i"		{ return CMP_CONTAINSI; }
<EXPR>"startswith"		{ return CMP_STARTSWITH; }
<EXPR>"startswith_i"		{ return CMP_STARTSWITHI; }
<EXPR>0[0-7]+ |			/* octal number */
<EXPR>0x[0-7a-f] |		/* hex number, following rule is dec; strtoll handles all! */
<EXPR>([1-9][0-9]*|0)		{ yylval.n = strtoll(yytext, NULL, 0); return NUMBER; }
<EXPR>\$[$!]{0,1}[a-z][a-z0-9\-_\.]*	{ yylval.s = strdup(yytext); return VAR; }
<EXPR>\'([^'\\]|\\['])*\'	 { yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				   return STRING; }
<EXPR>\"([^"\\]|\\["])*\"	 { yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				   return STRING; }
<EXPR>[ \t\n]
<EXPR>[a-z][a-z0-9_]*		{ yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  return FUNC; }
<EXPR>.				{ printf("invalid char in expr: %s\n", yytext); }
"&"				{ return '&'; }
"{"				{ return '{'; }
"}"				{ return '}'; }
"ruleset"			{ printf("RULESET\n"); }
 /* line number support because the "preprocessor" combines lines and so needs
  * to tell us the real source line.
  */
"stop"				{ dbgprintf("STOP\n"); return STOP; }
"preprocfilelinenumber("	{ BEGIN LINENO; }
<LINENO>[0-9]+			{ yylineno = atoi(yytext) - 1; }
<LINENO>")"			{ BEGIN INITIAL; }
<LINENO>.|\n
 /* $IncludeConfig must be detected as part of CFSYSLINE, because this is
  * always the longest match :-(
  */
<INCL>.|\n
<INCL>[^ \t\n]+			{ if(cnfDoInclude(yytext) != 0)
					yyterminate();
				  BEGIN INITIAL; }
"global"[ \n\t]*"("		{ yylval.objType = CNFOBJ_GLOBAL;
				  BEGIN INOBJ; return BEGINOBJ; }
"input"[ \n\t]*"("		{ yylval.objType = CNFOBJ_INPUT;
				  BEGIN INOBJ; return BEGINOBJ; }
"module"[ \n\t]*"("		{ yylval.objType = CNFOBJ_MODULE;
				  BEGIN INOBJ; return BEGINOBJ; }
"action"[ \n\t]*"("		{ BEGIN INOBJ; return BEGIN_ACTION; }
^[ \t]*:\$?[a-z]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\".*\"	{
				  yylval.s = strdup(yytext); return PROPFILT; }
^[ \t]*[a-z][,\*a-z]*[0-7]*\.[,!=;\.\*a-z]+ { yylval.s = strdup(yytext); return PRIFILT; }
"*" |
\-\/[^*][^\n]* |
\/[^*][^\n]* |
:[a-z0-9]+:[^\n]* |
[\|\.\-\@\^?~>][^\n]+ |
[a-z0-9_][a-z0-9_\-\+]*		{ yylval.s = strdup(yytext);
				 // printf("lex: LEGA ACT: '%s'\n", yytext);
				  return LEGACY_ACTION; }
<INOBJ>")"			{ BEGIN INITIAL; return ENDOBJ; }
<INOBJ>[a-z][a-z0-9_\.]*	{ yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  return NAME; }
<INOBJ>"="			{ return(yytext[0]); }
<INOBJ>\"([^"\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3})*\" {
				  yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				  return VALUE; }
"/*"				{ preCommentState = YY_START; BEGIN COMMENT; }
<EXPR>"/*"			{ preCommentState = YY_START; BEGIN COMMENT; }
<COMMENT>"*/"			{ BEGIN preCommentState; }
<COMMENT>([^*]|\n)+|.
<INOBJ>#.*$	/* skip comments in input */
<INOBJ>[ \n\t]
<INOBJ>.			{ printf("INOBJ: invalid char '%s'\n", yytext); }
\$[a-z]+.*$			{ /* see common on $IncludeConfig above */
				  if(!strncasecmp(yytext, "$includeconfig ", 14)) {
					yyless(14);
				  	BEGIN INCL;
				  } else {
					  yylval.s = strdup(yytext);
					  return CFSYSLINE;
				  }
				}
![^ \t\n]+[ \t]*$		{ yylval.s = strdup(yytext); return BSD_TAG_SELECTOR; }
[+-]\*[ \t\n]*#.*$		{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
[+-]\*[ \t\n]*$			{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
^[ \t]*[+-][a-z0-9.:-]+[ \t]*$	{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
\#.*\n	/* skip comments in input */
[\n\t ]	/* drop whitespace */
.				{ printf("invalid char: %s\n", yytext);
				}
<<EOF>>				{ if(popfile() != 0) yyterminate(); }

%%
int
cnfParseBuffer(char *buf, unsigned lenBuf)
{
	struct bufstack *bs;
	int r = 0;
	yydebug = 1;
	BEGIN INITIAL;
	/* maintain stack */
	if((bs = malloc(sizeof(struct bufstack))) == NULL) {
		r = 1;
		goto done;
	}

	if(currbs != NULL)
		currbs->lineno = yylineno;
	bs->prev = currbs;
	bs->fn = strdup("*buffer*");
	bs->bs = yy_scan_buffer(buf, lenBuf);
	bs->estr = NULL;
	currbs = bs;
	cnfcurrfn = bs->fn;
	yylineno = 1;
done:	return r;
}

/* set a new buffers. Returns 0 on success, something else otherwise. */
int
cnfSetLexFile(char *fname)
{
	es_str_t *str = NULL;
	FILE *fp;
	int r = 0;
	struct bufstack *bs;

	if(fname == NULL) {
		fp = stdin;
	} else {
		if((fp = fopen(fname, "r")) == NULL) {
			r = 1;
			goto done;
		}
	}
	readConfFile(fp, &str);
	if(fp != stdin)
		fclose(fp);
	
	/* maintain stack */
	if((bs = malloc(sizeof(struct bufstack))) == NULL) {
		r = 1;
		goto done;
	}

	if(currbs != NULL)
		currbs->lineno = yylineno;
	bs->prev = currbs;
	bs->fn = strdup(fname == NULL ? "stdin" : fname);
	bs->bs = yy_scan_buffer((char*)es_getBufAddr(str), es_strlen(str));
	bs->estr = str; /* needed so we can free it later */
	currbs = bs;
	cnfcurrfn = bs->fn;
	yylineno = 1;

done:
	if(r != 0) {
		if(str != NULL)
			es_deleteStr(str);
	}
	return r;
}


/* returns 0 on success, something else otherwise */
int
popfile(void)
{
	struct bufstack *bs = currbs;

	if(bs == NULL)
		return 1;
	
	/* delte current entry */
	yy_delete_buffer(bs->bs);
	free(bs->fn);
	free(bs->estr);

	/* switch back to previous */
	currbs = bs->prev;
	free(bs);

	if(currbs == NULL)
		return 1; /* all processed */
	
	yy_switch_to_buffer(currbs->bs);
	yylineno = currbs->lineno;
	cnfcurrfn = currbs->fn;
	return 0;
}
