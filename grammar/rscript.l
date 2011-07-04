 /* lex file for rsyslog config format v2.
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
  * Copyright (C) 2011 by Rainer Gerhards and Adiscon GmbH
  * Released under the GNU GPL v3. For details see LICENSE file.
  */

%option noyywrap nodefault case-insensitive
 /*%option noyywrap nodefault case-insensitive */

%x INOBJ
	/* INOBJ is selected if we are inside an object (name/value pairs!) */
%x COMMENT
	/* COMMENT is "the usual trick" to handle C-style comments */
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
#include <libestr.h>
#include "utils.h"
#include "rscript.tab.h"
static int preCommentState;
%}

%%

 /* keywords */
"if"				{ BEGIN EXPR; return IF; }
<EXPR>"then"			{ BEGIN INITIAL; return THEN; }
<EXPR>"or"			{ return OR; }
<EXPR>"and"			{ return AND; }
<EXPR>"not"			{ return NOT; }
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
<EXPR>\$[$!]{0,1}[a-z][a-z0-9\-_\.]*	{ printf("VARNAME: '%s'\n", yytext); return VAR; }
<EXPR>\'([^'\\]|\\['])*\'	 { printf("EXPR string: -%s-\n", yytext); return STRING; }
<EXPR>[ \t\n]
<EXPR>.				{ printf("invalid char in expr: %s\n", yytext); }
"&"				{ return '&'; }
"{"				{ return '{'; }
"}"				{ return '}'; }
"ruleset"			{ printf("RULESET\n"); }

"global"[ \n\t]*"("		{ yylval.objType = CNFOBJ_GLOBAL;
				  BEGIN INOBJ; return BEGINOBJ; }
"input"[ \n\t]*"("		{ yylval.objType = CNFOBJ_INPUT;
				  BEGIN INOBJ; return BEGINOBJ; }
"module"[ \n\t]*"("		{ yylval.objType = CNFOBJ_MODULE;
				  BEGIN INOBJ; return BEGINOBJ; }
"action"[ \n\t]*"("		{ BEGIN INOBJ; return BEGIN_ACTION; }
^[ \t]*:\$?[a-z]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\".*\"	{
				  printf("PROP-FILT: '%s'\n", yytext);
				}

^[ \t]*[,\*a-z]+\.[,!=;\.\*a-z]+ { printf("token prifilt '%s'\n", yytext); yylval.s = strdup(yytext); return PRIFILT; }

"*" |
\/[^*][^\n]* |
[\|\.\-\@:~][^\n]+ |
[a-z0-9_\-\+]+			{ yylval.s = strdup(yytext); return LEGACY_ACTION; }
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

<INOBJ>#.*\n	/* skip comments in input */
<INOBJ>[ \n\t]
<INOBJ>.			{ printf("INOBJ: invalid char '%s'\n", yytext); }

 /* CFSYSLINE is valid in all modes */
\$[a-z]+.*$			{ yylval.s = strdup(yytext); return CFSYSLINE; }

\#.*\n	/* skip comments in input */
[\n\t ]	/* drop whitespace */
.				{ printf("invalid char: %s\n", yytext);
				}

 /*<<EOF>>			{ printf("EOF reached\n"); }*/

%%
/* set a new buffers. Returns 0 on success, something else otherwise. */
int cnfSetLexFile(char *fname)
{
	es_str_t *str;
	YY_BUFFER_STATE bp;
	FILE *fp;
	int r = 0;

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
	bp = yy_scan_buffer(es_getBufAddr(str), es_strlen(str));
	yylineno = 1;

done:
	return r;
}
