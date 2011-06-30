%{
#include <libestr.h>
%}

%%


.	{ printf("%s", yytext); }

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
