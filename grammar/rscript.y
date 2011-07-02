
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
%token BEGIN_ACTION
%token <s> LEGACY_ACTION
%token <s> PRIFILT
%token <s> PROPFILT

%type <nvlst> nv nvlst
%type <obj> obj
%type <s> actlst
%type <s> act

%%
conf:	/* empty (to end recursion) */
	| obj conf
	| cfsysline conf
	| rule conf

obj: BEGINOBJ nvlst ENDOBJ 		{ $$ = cnfobjNew($1, $2);
					  cnfobjPrint($$);
					  cnfobjDestruct($$);
					}
obj: BEGIN_ACTION nvlst ENDOBJ 		{ struct cnfobj *t = cnfobjNew(CNFOBJ_ACTION, $2);
					  cnfobjPrint(t);
					  cnfobjDestruct(t);
					  printf("XXXX: this is an new-style action!\n");
					}
cfsysline: CFSYSLINE			{ printf("XXXX: processing CFSYSLINE: %s\n", $1);
					}
nvlst:					{ $$ = NULL; }
	| nvlst nv 			{ $2->next = $1; $$ = $2; }
nv: NAME '=' VALUE 			{ $$ = nvlstNew($1, $3); }

rule:	  PRIFILT actlst		{ printf("PRIFILT: %s\n", $1); free($1); }
	| PROPFILT actlst

actlst:	  act 				{ printf("action (end actlst) %s\n", $1);$$=$1; }
	| actlst '&' act		{ printf("in actionlist %s\n", $3); }
act:	  BEGIN_ACTION nvlst ENDOBJ	{ $$ = "obj"; }
	| LEGACY_ACTION			{ printf("legacy action: '%s'\n", $1);
					  /*free($1);*/
					  $$ = $1;}

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

