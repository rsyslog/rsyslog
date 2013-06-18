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
  * Copyright 2011-2012 Rainer Gerhards and Adiscon GmbH.
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
%x INCALL
	/* INCALL: support for the call statement */
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
#include "parserif.h"
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
int fileno(FILE *stream);

%}

%%

 /* keywords */
"if"				{ BEGIN EXPR; return IF; }
<EXPR>"then"			{ BEGIN INITIAL; return THEN; }
<EXPR>";"			{ BEGIN INITIAL; return ';'; }
<EXPR>"or"			{ return OR; }
<EXPR>"and"			{ return AND; }
<EXPR>"not"			{ return NOT; }
<EXPR>"=" |
<EXPR>"," |
<EXPR>"*" |
<EXPR>"/" |
<EXPR>"%" |
<EXPR>"+" |
<EXPR>"&" |
<EXPR>"-" |
<EXPR>"[" |
<EXPR>"]" |
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
<EXPR>\$[$!]{0,1}[a-z][!a-z0-9\-_\.]*	{ yylval.s = strdup(yytext); return VAR; }
<EXPR>\'([^'\\]|\\['"\\$bntr]|\\x[0-9a-f][0-9a-f]|\\[0-7][0-7][0-7])*\'	 {
				   yytext[yyleng-1] = '\0';
				   unescapeStr((uchar*)yytext+1, yyleng-2);
				   yylval.estr = es_newStrFromBuf(yytext+1, strlen(yytext)-1);
				   return STRING; }
<EXPR>\"([^"\\$]|\\["'\\$bntr]|\\x[0-9a-f][0-9a-f]|\\[0-7][0-7][0-7])*\" {
				   yytext[yyleng-1] = '\0';
				   unescapeStr((uchar*)yytext+1, yyleng-2);
				   yylval.estr = es_newStrFromBuf(yytext+1, strlen(yytext)-1);
				   return STRING; }
<EXPR>[ \t\n]
<EXPR>[a-z][a-z0-9_]*		{ yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  return FUNC; }
<EXPR>.				{ parser_errmsg("invalid character '%s' in expression "
					        "- is there an invalid escape sequence somewhere?",
						yytext); }
<INCALL>[ \t\n]
<INCALL>.			{ parser_errmsg("invalid character '%s' in 'call' statement"
					        "- is there an invalid escape sequence somewhere?",
						yytext); }
<INCALL>[a-zA-Z][a-zA-Z0-9_\.]*	{ yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  BEGIN INITIAL;
				  return NAME; }
"&"				{ return '&'; }
"{"				{ return '{'; }
"}"				{ return '}'; }
"stop"				{ return STOP; }
"else"				{ return ELSE; }
"call"				{ BEGIN INCALL; return CALL; }
"set"				{ BEGIN EXPR; return SET; }
"unset"				{ BEGIN EXPR; return UNSET; }
"continue"			{ return CONTINUE; }
 /* line number support because the "preprocessor" combines lines and so needs
  * to tell us the real source line.
  */
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
"template"[ \n\t]*"("		{ yylval.objType = CNFOBJ_TPL;
				  BEGIN INOBJ; return BEGIN_TPL; }
"ruleset"[ \n\t]*"("		{ yylval.objType = CNFOBJ_RULESET;
				  BEGIN INOBJ; return BEGIN_RULESET; }
"property"[ \n\t]*"("		{ yylval.objType = CNFOBJ_PROPERTY;
				  BEGIN INOBJ; return BEGIN_PROPERTY; }
"constant"[ \n\t]*"("		{ yylval.objType = CNFOBJ_CONSTANT;
				  BEGIN INOBJ; return BEGIN_CONSTANT; }
"input"[ \n\t]*"("		{ yylval.objType = CNFOBJ_INPUT;
				  BEGIN INOBJ; return BEGINOBJ; }
"module"[ \n\t]*"("		{ yylval.objType = CNFOBJ_MODULE;
				  BEGIN INOBJ; return BEGINOBJ; }
"action"[ \n\t]*"("		{ BEGIN INOBJ; return BEGIN_ACTION; }
^[ \t]*:\$?[a-z\-]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\"(\\\"|[^\"])*\"	{
				  yylval.s = strdup(rmLeadingSpace(yytext));
				  dbgprintf("lexer: propfilt is '%s'\n", yylval.s);
				  return PROPFILT;
				  }
^[ \t]*[\*a-z][\*a-z]*[0-7]*[\.,][,!=;\.\*a-z0-7]+ { yylval.s = strdup(rmLeadingSpace(yytext)); return PRIFILT; }
"~" |
"*" |
\-\/[^*][^\n]* |
\/[^*][^\n]* |
:[a-z0-9]+:[^\n]* |
[\|\.\-\@\^?~>][^\n]+ |
[a-z0-9_][a-z0-9_\-\+,;]*	{ yylval.s = yytext; return LEGACY_ACTION; }
<INOBJ>")"			{ BEGIN INITIAL; return ENDOBJ; }
<INOBJ>[a-z][a-z0-9_\.]*	{ yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  return NAME; }
<INOBJ>"," |
<INOBJ>"[" |
<INOBJ>"]" |
<INOBJ>"="			{ return(yytext[0]); }
<INOBJ>\"([^"\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3})*\" {
				   yytext[yyleng-1] = '\0';
				   unescapeStr((uchar*)yytext+1, yyleng-2);
				   yylval.estr = es_newStrFromBuf(yytext+1, strlen(yytext)-1);
				   return STRING; }
				  /*yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				  return VALUE; }*/
"/*"				{ preCommentState = YY_START; BEGIN COMMENT; }
<INOBJ>"/*"			{ preCommentState = YY_START; BEGIN COMMENT; }
<EXPR>"/*"			{ preCommentState = YY_START; BEGIN COMMENT; }
<COMMENT>"*/"			{ BEGIN preCommentState; }
<COMMENT>([^*]|\n)+|.
<INOBJ>#.*$	/* skip comments in input */
<INOBJ>[ \n\t]
<INOBJ>.			{ parser_errmsg("invalid character '%s' in object definition "
					        "- is there an invalid escape sequence somewhere?",
						yytext); }
\$[a-z]+.*$			{ /* see comment on $IncludeConfig above */
				  if(!strncasecmp(yytext, "$includeconfig ", 14)) {
					yyless(14);
				  	BEGIN INCL;
				  } else if(!strncasecmp(yytext, "$ruleset ", 9)) {
					yylval.s = strdup(yytext);
					return LEGACY_RULESET;
				  } else {
					  cnfDoCfsysline(strdup(yytext)); 
				  }
				}
![^ \t\n]+[ \t]*$		{ yylval.s = strdup(yytext); return BSD_TAG_SELECTOR; }
[+-]\*[ \t\n]*#.*$		{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
[+-]\*[ \t\n]*$			{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
^[ \t]*[+-][a-z0-9.:-]+[ \t]*$	{ yylval.s = strdup(yytext); return BSD_HOST_SELECTOR; }
\#.*\n	/* skip comments in input */
[\n\t ]	/* drop whitespace */
.				{ parser_errmsg("invalid character '%s' "
					        "- is there an invalid escape sequence somewhere?",
						yytext); }
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
	dbgprintf("config parser: pushed file %s on top of stack\n", fname);

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
	
	/* delete current entry. But we must not free the file name if
	 * this is the top-level file, because then it may still be used
	 * in error messages for other processing steps.
	 * TODO: change this to another method which stores the file
	 * name inside the config objects. In the longer term, this is
	 * necessary, as otherwise we may provide wrong file name information
	 * at the end of include files as well. -- rgerhards, 2011-07-22
	 */
	dbgprintf("config parser: reached end of file %s\n", bs->fn);
	yy_delete_buffer(bs->bs);
	if(bs->prev != NULL)
		free(bs->fn);
	free(bs->estr);

	/* switch back to previous */
	currbs = bs->prev;
	free(bs);

	if(currbs == NULL) {
		dbgprintf("config parser: parsing completed\n");
		return 1; /* all processed */
	}
	
	yy_switch_to_buffer(currbs->bs);
	yylineno = currbs->lineno;
	cnfcurrfn = currbs->fn;
	dbgprintf("config parser: resume parsing of file %s at line %d\n",
		  cnfcurrfn, yylineno);
	return 0;
}

void
tellLexEndParsing(void)
{
	free(cnfcurrfn);
	cnfcurrfn= NULL;
}
