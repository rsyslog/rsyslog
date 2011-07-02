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
"if"		{ printf("IF\n");
		  BEGIN EXPR;
		}
<EXPR>"then"		{ printf("THEN\n");
			  BEGIN INITIAL;
			}
<EXPR>"or"		{ printf("OR\n"); }
<EXPR>"and"		{ printf("AND\n"); }
<EXPR>"not"		{ printf("NOT\n"); }
<EXPR>"("		{ printf("LPAREN\n"); }
<EXPR>")"		{ printf("RPAREN\n"); }
<EXPR>"=="		{ printf("==\n"); }
<EXPR>"<="		{ printf("<=\n"); }
<EXPR>">="		{ printf(">=\n"); }
<EXPR>"!=" |
<EXPR>"<>"		{ printf("!=\n"); }
<EXPR>"<"		{ printf("<\n"); }
<EXPR>">"		{ printf(">\n"); }
<EXPR>"contains"	{ printf("CONTAINS\n"); }
<EXPR>"contains_i"	{ printf("CONTAINS_I\n"); }
<EXPR>"startswith"	{ printf("STARTSWITH\n"); }
<EXPR>"startswith_i"	{ printf("STARTSWITH_I\n"); }
<EXPR>-?0[0-7]+		{ printf("NUMBER (oct) %s\n", yytext); }
<EXPR>-?0x[0-7a-f]	{ printf("NUMBER (hex) %s\n", yytext); }
<EXPR>-?([1-9][0-9]*|0)	{ printf("NUMBER (dec) %s\n", yytext); }
<EXPR>\$[$!]{0,1}[a-z][a-z0-9\-_\.]*		{ printf("VARNAME: '%s'\n", yytext); }
<EXPR>\'([^'\\]|\\['])*\' { printf("EXPR string: -%s-\n", yytext); }
<EXPR>[ \t\n]
<EXPR>.			{ printf("invalid char in expr: %s\n", yytext); }
"&"		{ printf("AMPER\n"); }
"ruleset"	{ printf("RULESET\n"); }

"global"[ \n\t]*"("			{ yylval.objType = CNFOBJ_GLOBAL;
					  BEGIN INOBJ; return BEGINOBJ; }
"input"[ \n\t]*"("			{ yylval.objType = CNFOBJ_INPUT;
					  BEGIN INOBJ; return BEGINOBJ; }
"module"[ \n\t]*"("			{ yylval.objType = CNFOBJ_MODULE;
					  BEGIN INOBJ; return BEGINOBJ; }
"action"[ \n\t]*"("			{ yylval.objType = CNFOBJ_ACTION;
					  BEGIN INOBJ; return BEGINOBJ; }
^[ \t]*:\$?[a-z]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\".*\"	{
					  printf("PROP-FILT: '%s'\n", yytext);
					}

^[ \t]*[,\*a-z]+\.[,!=;\.\*a-z]+ { printf("PRI-FILT: '%s'\n", yytext);
				}

"*" |
\/[^*][^\n]* |
[\|\.\-:][^\n]+			{ printf("old style action: '%s'\n", yytext);
				}

[a-z0-9_\-\+]+			{ printf("name: '%s'\n", yytext); }

<INOBJ>")"			{ printf("OBJ end\n");
				  BEGIN INITIAL;
				  return ENDOBJ;
				}
<INOBJ>[a-z][a-z0-9_\.]*	{ printf("INOBJ: name '%s'\n", yytext);
				  yylval.estr = es_newStrFromCStr(yytext, yyleng);
				  return NAME;
				}
<INOBJ>"="			{ printf("INOBJ: equals (%s)\n", yytext);
				  return(yytext[0]);
				}
<INOBJ>\"([^"\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3})*\" {
				  printf("INOBJ: value '%s'\n", yytext);
				  yylval.estr = es_newStrFromCStr(yytext+1, yyleng-2);
				  return VALUE;
				}
"/*"				{ preCommentState = YY_START;
		  		  BEGIN COMMENT;
				}
<EXPR>"/*"			{ preCommentState = YY_START;
				  BEGIN COMMENT;
				}
<COMMENT>"*/"			{ BEGIN preCommentState; }
<COMMENT>([^*]|\n)+|.

<INOBJ>#.*\n	/* skip comments in input */
<INOBJ>[ \n\t]
<INOBJ>.			{ printf("INOBJ: invalid char '%s'\n", yytext); }

 /* CFSYSLINE is valid in all modes */
\$[a-z]+.*$			{ printf("CFSYSLINE: '%s'\n", yytext);
				  yylval.s = yytext;
				  return CFSYSLINE;
				}
<INOBJ>\$[a-z]+.*$		{ printf("CFSYSLINE: '%s'\n", yytext);
				  yylval.s = yytext;
				  return CFSYSLINE;
				}

\#.*\n	/* skip comments in input */
[\n\t ]	/* drop whitespace */
.				{ printf("invalid char: %s\n", yytext);
				}

 /*<<EOF>>			{ printf("EOF reached\n"); }*/

%%
 /*
int
main(int argc, char *argv[])
{
	es_str_t *str;
	YY_BUFFER_STATE bp;
	char ln[10240];

	readConfFile(stdin, &str);
	//printf("buffer: %s\n", es_getBufAddr(str));
	bp = yy_scan_buffer(es_getBufAddr(str), es_strlen(str));
	//yy_switch_to_buffer(bp);
	yylex();
}
*/
