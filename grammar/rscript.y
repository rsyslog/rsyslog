
%{
#include <stdio.h>
#include <libestr.h>
#include "utils.h"
#define YYDEBUG 1
%}

%union {
	char *s;
	long long n;
	es_str_t *estr;
	enum cnfobjType objType;
	struct cnfobj *obj;
	struct nvlst *nvlst;
	struct cnfactlst *actlst;
	struct cnfexpr *expr;
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
%token IF
%token THEN
%token OR
%token AND
%token NOT
%token VAR
%token <estr> STRING
%token <n> NUMBER

%type <nvlst> nv nvlst
%type <obj> obj
%type <actlst> actlst
%type <actlst> act
%type <s> cfsysline
%type <actlst> block
%type <expr> expr

%left AND OR
%left '+' '-'
%left '*' '/' '%'
%nonassoc UMINUS NOT

%expect 3
/* two shift/reduce conflicts are created by the CFSYSLINE construct, which we
 * unfortunately can not avoid. The problem is that CFSYSLINE can occur both in
 * global context as well as within an action. It's not permitted somewhere else,
 * but this is suficient for conflicts. The "dangling else" built-in resolution
 * works well to solve this issue, so we accept it (it's a wonder that our
 * old style grammar doesn't work at all, so we better do not complain...).
 * Use "bison -v rscript.y" if more conflicts arise and check rscript.out for
 * were exactly these conflicts exits.
 */
%%
conf:	/* empty (to end recursion) */
	| obj conf
	| cfsysline conf
	| rule conf

obj:	  BEGINOBJ nvlst ENDOBJ 	{ $$ = cnfobjNew($1, $2);
					  cnfobjPrint($$);
					  cnfobjDestruct($$);
					}
	| BEGIN_ACTION nvlst ENDOBJ 	{ struct cnfobj *t = cnfobjNew(CNFOBJ_ACTION, $2);
					  cnfobjPrint(t);
					  cnfobjDestruct(t);
					  printf("XXXX: this is an new-style action!\n");
					}
cfsysline: CFSYSLINE	 		{ printf("XXXX: processing CFSYSLINE: %s\n", $1); }

nvlst:					{ $$ = NULL; }
	| nvlst nv 			{ $2->next = $1; $$ = $2; }
nv: NAME '=' VALUE 			{ $$ = nvlstNew($1, $3); }

rule:	  PRIFILT actlst		{ printf("PRIFILT: %s\n", $1); free($1);
					  $2 = cnfactlstReverse($2);
					  cnfactlstPrint($2); }
	| PROPFILT actlst
	| scriptfilt

scriptfilt: IF expr THEN actlst	{ printf("if filter detected, expr:\n"); cnfexprPrint($2,0); }

/* note: we can do some limited block-structuring with the v6 engine. In that case,
 * we must not support additonal filters inside the blocks, so they must consist of
 * "act", only. We can implement that via the "&" actlist logic.
 */
block:	  actlst
	| block actlst
	/* v7: | actlst
	   v7: | block rule */

actlst:	  act 	 			{ printf("action (end actlst)\n");$$=$1; }
	| actlst '&' act 		{ printf("in actionlist \n");
					  $3->next = $1; $$ = $3; }
	| actlst cfsysline		{ printf("in actionlist/CFSYSLINE: %s\n", $2);
					  $$ = cnfactlstAddSysline($1, $2); }
	| '{' block '}'			{ $$ = $2; }
					  
act:	  BEGIN_ACTION nvlst ENDOBJ	{ $$ = cnfactlstNew(CNFACT_V2, $2, NULL); }
	| LEGACY_ACTION			{ printf("legacy action: '%s'\n", $1);
					  $$ = cnfactlstNew(CNFACT_LEGACY, NULL, $1); }

expr:	  expr '+' expr			{ $$ = cnfexprNew('+', $1, $3); }
	| expr '-' expr			{ $$ = cnfexprNew('-', $1, $3); }
	| expr '*' expr			{ $$ = cnfexprNew('*', $1, $3); }
	| expr '/' expr			{ $$ = cnfexprNew('/', $1, $3); }
	| expr '%' expr			{ $$ = cnfexprNew('%', $1, $3); }
	| '(' expr ')'			{ $$ = $2; printf("( expr)\n"); }
	| '-' expr %prec UMINUS		{ printf("uminus\n"); $$ = cnfexprNew('M', NULL, $2); }
	| NUMBER			{ $$ = cnfnumvalNew($1); }
	| STRING			{ $$ = cnfstringvalNew($1); }
	| VAR				{ printf("variables not yet implemented!\n"); }

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

