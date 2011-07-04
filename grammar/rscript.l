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
  * Copyright (C) 2011 by Rainer Gerhards and Adiscon GmbH
  * Released under the GNU GPL v3. For details see LICENSE file.
  */

%option noyywrap nodefault case-insensitive yylineno
 /*%option noyywrap nodefault case-insensitive */

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
#include <libestr.h>
#include "utils.h"
#include "rscript.tab.h"
static int preCommentState;	/* save for lex state before a comment */

struct bufstack {
	struct bufstack *prev;
	YY_BUFFER_STATE bs;
	int lineno;
	char *fn;
} *currbs = NULL;

char *currfn;			/* name of currently processed file */
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
<EXPR>\$[$!]{0,1}[a-z][a-z0-9\-_\.]*	{ yylval.s = strdup(yytext); return VAR; }
<EXPR>\'([^'\\]|\\['])*\'	 { yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				   return STRING; }
<EXPR>\"([^"\\]|\\["])*\"	 { yylval.estr = es_newStrFromBuf(yytext+1, yyleng-2);
				   return STRING; }
<EXPR>[ \t\n]
<EXPR>.				{ printf("invalid char in expr: %s\n", yytext); }
"&"				{ return '&'; }
"{"				{ return '{'; }
"}"				{ return '}'; }
"ruleset"			{ printf("RULESET\n"); }
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
<INCL>[^ \t\n]+			{ if(cnfSetLexFile(yytext) != 0)
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
^[ \t]*[,\*a-z]+\.[,!=;\.\*a-z]+ { yylval.s = strdup(yytext); return PRIFILT; }
"*" |
\-\/[^*][^\n]* |
\/[^*][^\n]* |
:[a-z0-9]+:[^\n]* |
[\|\.\-\@~][^\n]+ |
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
/* set a new buffers. Returns 0 on success, something else otherwise. */
int cnfSetLexFile(char *fname)
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
	bs->fn = strdup(fname);
	bs->bs = yy_scan_buffer(es_getBufAddr(str), es_strlen(str));
	currbs = bs;
	currfn = bs->fn;
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

	/* switch back to previous */
	currbs = bs->prev;
	free(bs);

	if(currbs == NULL)
		return 1; /* all processed */
	
	yy_switch_to_buffer(currbs->bs);
	yylineno = currbs->lineno;
	currfn = currbs->fn;
	return 0;
}
