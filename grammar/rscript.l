%option noyywrap nodefault case-insensitive
 /*%option noyywrap nodefault case-insensitive */

%x INOBJ
	/* INOBJ is selected if we are inside an object (name/value pairs!) */
%{
#include <libestr.h>
%}

%%

"global"[ \n\t]*"("			{ printf("OBJ GLOBAL begin\n");
					  BEGIN INOBJ;
					}
"input"[ \n\t]*"("			{ printf("OBJ INPUT begin\n");
					  BEGIN INOBJ;
					}
"module"[ \n\t]*"("			{ printf("OBJ MODULE begin\n");
					  BEGIN INOBJ;
					}
"action"[ \n\t]*"("			{ printf("OBJ ACTION begin\n");
					  BEGIN INOBJ;
					}
^[ \t]*:\$?[a-z]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\".*\"	{
					  printf("PROP-FILT: '%s'\n", yytext);
					  /*BEGIN OLDACT;*/
					}

^[ \t]*[,\*a-z]+\.[,!=;\.\*a-z]+	{ printf("PRI-FILT: '%s'\n", yytext);
					  /*BEGIN OLDACT;*/
					}

"*" |
[\|\.\/\-:][^\n]+			{ printf("old style action: '%s'\n", yytext);
					}

<INOBJ>")"				{ printf("OBJ end\n");
					  BEGIN INITIAL;
					}
<INOBJ>[a-z][a-z0-9_]*	{ printf("INOBJ: name '%s'\n", yytext);
			}
<INOBJ>"="		{ printf("INOBJ: equals\n");
			}
<INOBJ>[a-z0-9\.,_\+\-\/]* |
<INOBJ>\"([^"\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3})*\" {
 			  printf("INOBJ: value '%s'\n", yytext);
			  BEGIN INOBJ;
			}
<INOBJ>#.*\n	/* skip comments in input */
<INOBJ>[ \n\t]
<INOBJ>.		{ printf("INOBJ: invalid char '%s'\n", yytext); }

 /* CFSYSLINE is valid in all modes */
\$[a-z]+.*$				{ printf("CFSYSLINE: '%s'\n", yytext); }
<INOBJ>\$[a-z]+.*$			{ printf("CFSYSLINE: '%s'\n", yytext); }

\#.*\n	/* skip comments in input */
[\n\t ]	/* drop whitespace */
.			{ printf("invalid char: %s\n", yytext);
			}

 /*<<EOF>>			{ printf("EOF reached\n"); }*/

%%
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
