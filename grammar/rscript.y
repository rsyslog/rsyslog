
%{
#include <stdio.h>
#include <libestr.h>
#include "utils.h"
#define YYDEBUG 1
%}

%union {
	char *s;
	es_str_t *estr;
	enum cnfobjType objType;
	struct cnfobj *obj;
	struct nvlst *nvlst;
}

%token <estr> NAME
%token <estr> VALUE
%token <objType> BEGINOBJ
%token ENDOBJ
%token <s> CFSYSLINE

%type <nvlst> nv nvlst
%type <obj> obj

%%
 /* conf:  | conf global | conf action*/
conf:
	| obj conf
	| cfsysline conf

obj: BEGINOBJ nvlst ENDOBJ 		{ printf("XXXX: global processed\n");
					  $$ = cnfobjNew($1, $2);
					  cnfobjPrint($$);
					  cnfobjDestruct($$);
					}
cfsysline: CFSYSLINE			{ printf("XXXX: processing CFSYSLINE: %s\n", $1);
					}
nvlst:					{ $$ = NULL; }
	| nvlst nv 			{ printf("XXXX: nvlst $1: %p, $2 %p\n", $1,$2);
					  $2->next = $1;
					  $$ = $2;
					}
nv: NAME '=' VALUE 			{ $$ = nvlstNew($1, $3); }

%%
int yyerror(char *s)
{
	printf("yyerror called: %s\n", s);
}

int main()
{
	yydebug = 0;
	return yyparse();
}

