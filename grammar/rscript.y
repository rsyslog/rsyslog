 /* Bison file for rsyslog config format v2 (RainerScript).
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

%{
#include <stdio.h>
#include <libestr.h>
#include "utils.h"
#define YYDEBUG 1
extern int yylineno;
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
	struct cnfrule *rule;
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
%token <s> BSD_TAG_SELECTOR
%token <s> BSD_HOST_SELECTOR
%token IF
%token THEN
%token OR
%token AND
%token NOT
%token <s> VAR
%token <estr> STRING
%token <n> NUMBER
%token CMP_EQ
%token CMP_NE
%token CMP_LE
%token CMP_GE
%token CMP_LT
%token CMP_GT
%token CMP_CONTAINS
%token CMP_CONTAINSI
%token CMP_STARTSWITH
%token CMP_STARTSWITHI

%type <nvlst> nv nvlst
%type <obj> obj
%type <actlst> actlst
%type <actlst> act
%type <s> cfsysline
%type <actlst> block
%type <expr> expr
%type <rule> rule
%type <rule> scriptfilt

%left AND OR
%left CMP_EQ CMP_NE CMP_LE CMP_GE CMP_LT CMP_GT CMP_CONTAINS CMP_CONTAINSI CMP_STARTSWITH CMP_STARTSWITHI
%left '+' '-'
%left '*' '/' '%'
%nonassoc UMINUS NOT

%expect 3
/* these shift/reduce conflicts are created by the CFSYSLINE construct, which we
 * unfortunately can not avoid. The problem is that CFSYSLINE can occur both in
 * global context as well as within an action. It's not permitted somewhere else,
 * but this is suficient for conflicts. The "dangling else" built-in resolution
 * works well to solve this issue, so we accept it (it's a wonder that our
 * old style grammar doesn't work at all, so we better do not complain...).
 * Use "bison -v rscript.y" if more conflicts arise and check rscript.out for
 * were exactly these conflicts exits.
 */
%%
/* note: we use left recursion below, because that saves stack space AND
 * offers the right sequence so that we can submit the top-layer objects
 * one by one.
 */
conf:	/* empty (to end recursion) */
	| conf obj			{ printf("global:config: ");
					  cnfobjPrint($2); cnfobjDestruct($2); }
	| conf rule			{ printf("global:rule processed\n");
					  cnfrulePrint($2); }
	| conf cfsysline		{ printf("global:cfsysline: %s\n", $2); }
	| conf BSD_TAG_SELECTOR		{ printf("global:BSD tag '%s'\n", $2); }
	| conf BSD_HOST_SELECTOR	{ printf("global:BSD host '%s'\n", $2); }

obj:	  BEGINOBJ nvlst ENDOBJ 	{ $$ = cnfobjNew($1, $2); }
	| BEGIN_ACTION nvlst ENDOBJ 	{ $$ = cnfobjNew(CNFOBJ_ACTION, $2); }
cfsysline: CFSYSLINE	 		{ $$ = $1 }
nvlst:					{ $$ = NULL; }
	| nvlst nv 			{ $2->next = $1; $$ = $2; }
nv:	NAME '=' VALUE 			{ $$ = nvlstNew($1, $3); }
rule:	  PRIFILT actlst		{ $$ = cnfruleNew(CNFFILT_PRI, $2); $$->filt.s = $1; }
	| PROPFILT actlst		{ $$ = cnfruleNew(CNFFILT_PROP, $2); $$->filt.s = $1; }
	| scriptfilt			{ $$ = $1; }

scriptfilt: IF expr THEN actlst		{ $$ = cnfruleNew(CNFFILT_SCRIPT, $4);
					  $$->filt.expr = $2; }
block:	  actlst			{ $$ = $1; }
	| block actlst			{ $2->next = $1; $$ = $2; }
	/* v7: | actlst
	   v7: | block rule */ /* v7 extensions require new rule engine capabilities! */
actlst:	  act 	 			{ $$=$1; }
	| actlst '&' act 		{ $3->next = $1; $$ = $3; }
	| actlst cfsysline		{ $$ = cnfactlstAddSysline($1, $2); }
	| '{' block '}'			{ $$ = $2; }
act:	  BEGIN_ACTION nvlst ENDOBJ	{ $$ = cnfactlstNew(CNFACT_V2, $2, NULL); }
	| LEGACY_ACTION			{ $$ = cnfactlstNew(CNFACT_LEGACY, NULL, $1); }
expr:	  expr AND expr			{ $$ = cnfexprNew(AND, $1, $3); }
	| expr OR expr			{ $$ = cnfexprNew(OR, $1, $3); }
	| NOT expr			{ $$ = cnfexprNew(NOT, NULL, $2); }
	| expr CMP_EQ expr		{ $$ = cnfexprNew(CMP_EQ, $1, $3); }
	| expr CMP_NE expr		{ $$ = cnfexprNew(CMP_NE, $1, $3); }
	| expr CMP_LE expr		{ $$ = cnfexprNew(CMP_LE, $1, $3); }
	| expr CMP_GE expr		{ $$ = cnfexprNew(CMP_GE, $1, $3); }
	| expr CMP_LT expr		{ $$ = cnfexprNew(CMP_LT, $1, $3); }
	| expr CMP_GT expr		{ $$ = cnfexprNew(CMP_GT, $1, $3); }
	| expr CMP_CONTAINS expr	{ $$ = cnfexprNew(CMP_CONTAINS, $1, $3); }
	| expr CMP_CONTAINSI expr	{ $$ = cnfexprNew(CMP_CONTAINSI, $1, $3); }
	| expr CMP_STARTSWITH expr	{ $$ = cnfexprNew(CMP_STARTSWITH, $1, $3); }
	| expr CMP_STARTSWITHI expr	{ $$ = cnfexprNew(CMP_STARTSWITHI, $1, $3); }
	| expr '+' expr			{ $$ = cnfexprNew('+', $1, $3); }
	| expr '-' expr			{ $$ = cnfexprNew('-', $1, $3); }
	| expr '*' expr			{ $$ = cnfexprNew('*', $1, $3); }
	| expr '/' expr			{ $$ = cnfexprNew('/', $1, $3); }
	| expr '%' expr			{ $$ = cnfexprNew('%', $1, $3); }
	| '(' expr ')'			{ $$ = $2; }
	| '-' expr %prec UMINUS		{ $$ = cnfexprNew('M', NULL, $2); }
	| NUMBER			{ $$ = (struct cnfexpr*) cnfnumvalNew($1); }
	| STRING			{ $$ = (struct cnfexpr*) cnfstringvalNew($1); }
	| VAR				{ $$ = (struct cnfexpr*) cnfvarNew($1); }

%%
int yyerror(char *s)
{
	printf("parse failure on or before line %d: %s\n", yylineno, s);
}
