%option noyywrap nodefault case-insensitive
 /*%option noyywrap nodefault case-insensitive */

%x OLDACT
	/* old-style action expected -- need to parse differently */
%{
#include <libestr.h>
%}

%%


\$[a-z]+.*$				{ printf("CFSYSLINE: '%s'\n", yytext); }

^[ \t]*:\$?[a-z]+[ ]*,[ ]*!?[a-z]+[ ]*,[ ]*\".*\"	{
					  printf("PROP-FILT: '%s'\n", yytext);
					  BEGIN OLDACT;
					}

^[ \t]*[,\*a-z]+\.[,!=;\.\*a-z]+	{ printf("PRI-FILT: '%s'\n", yytext);
					  BEGIN OLDACT;
					}
  
#.*\n	/* skip comments in input */
.|\n	{ if(yytext[0] != '\n') printf("%s", yytext); }

<OLDACT>\* |
<OLDACT>-\/[^ \t\n]+ |
<OLDACT>\|[^ \t\n]+ |
<OLDACT>\/[^ \t\n]+	{ printf("old style action: '%s'\n", yytext);
			  BEGIN INITIAL;
			}
<OLDACT>[ \t\n]
<OLDACT>.|\n		{ printf("invalid sequence in OLDACT mode: %s\n",
				 yytext);
			}



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
