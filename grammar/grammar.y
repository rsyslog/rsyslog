 /* Bison file for rsyslog config format v2 (RainerScript).
  * Please note: this file introduces the new config format, but maintains
  * backward compatibility. In order to do so, the grammar is not 100% clean,
  * but IMHO still sufficiently easy both to understand for programmers
  * maitaining the code as well as users writing the config file. Users are,
  * of course, encouraged to use new constructs only. But it needs to be noted
  * that some of the legacy constructs (specifically the in-front-of-action
  * PRI filter) are very hard to beat in ease of use, at least for simpler
  * cases.
  *
  * Copyright 2011-2012 Rainer Gerhards and Adiscon GmbH.
  *
  * This file is part of the rsyslog runtime library.
  *
  * The rsyslog runtime library is free software: you can redistribute it and/or modify
  * it under the terms of the GNU Lesser General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * The rsyslog runtime library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
  *
  * A copy of the GPL can be found in the file "COPYING" in this distribution.
  * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
  */
%{
#include <stdio.h>
#include <libestr.h>
#include "rainerscript.h"
#include "parserif.h"
#define YYDEBUG 1
extern int yylineno;

/* keep compile rule clean of errors */
extern int yylex(void);
extern int yyerror(char*);
%}

%union {
	char *s;
	long long n;
	es_str_t *estr;
	enum cnfobjType objType;
	struct cnfobj *obj;
	struct cnfstmt *stmt;
	struct nvlst *nvlst;
	struct objlst *objlst;
	struct cnfexpr *expr;
	struct cnfarray *arr;
	struct cnffunc *func;
	struct cnffparamlst *fparams;
}

%token <estr> NAME
%token <estr> FUNC
%token <objType> BEGINOBJ
%token ENDOBJ
%token BEGIN_ACTION
%token BEGIN_PROPERTY
%token BEGIN_CONSTANT
%token BEGIN_TPL
%token BEGIN_RULESET
%token STOP
%token SET
%token UNSET
%token CONTINUE
%token <cnfstmt> CALL
%token <s> LEGACY_ACTION
%token <s> LEGACY_RULESET
%token <s> PRIFILT
%token <s> PROPFILT
%token <s> BSD_TAG_SELECTOR
%token <s> BSD_HOST_SELECTOR
%token IF
%token THEN
%token ELSE
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

%type <nvlst> nv nvlst value
%type <obj> obj property constant
%type <objlst> propconst
%type <expr> expr
%type <stmt> stmt s_act actlst block script
%type <fparams> fparams
%type <arr> array arrayelt

%left AND OR
%left CMP_EQ CMP_NE CMP_LE CMP_GE CMP_LT CMP_GT CMP_CONTAINS CMP_CONTAINSI CMP_STARTSWITH CMP_STARTSWITHI
%left '+' '-' '&'
%left '*' '/' '%'
%nonassoc UMINUS NOT

%expect 1 /* dangling else */
/* If more erors show up, Use "bison -v grammar.y" if more conflicts arise and
 * check grammar.output for were exactly these conflicts exits.
 */
%%
/* note: we use left recursion below, because that saves stack space AND
 * offers the right sequence so that we can submit the top-layer objects
 * one by one.
 */
conf:	/* empty (to end recursion) */
	| conf obj			{ cnfDoObj($2); }
	| conf stmt			{ cnfDoScript($2); }
	| conf LEGACY_RULESET		{ cnfDoCfsysline($2); }
	| conf BSD_TAG_SELECTOR		{ cnfDoBSDTag($2); }
	| conf BSD_HOST_SELECTOR	{ cnfDoBSDHost($2); }
obj:	  BEGINOBJ nvlst ENDOBJ 	{ $$ = cnfobjNew($1, $2); }
        | BEGIN_TPL nvlst ENDOBJ	{ $$ = cnfobjNew(CNFOBJ_TPL, $2); }
        | BEGIN_TPL nvlst ENDOBJ '{' propconst '}'
					{ $$ = cnfobjNew(CNFOBJ_TPL, $2);
					  $$->subobjs = $5;
					}
        | BEGIN_RULESET nvlst ENDOBJ '{' script '}'
					{ $$ = cnfobjNew(CNFOBJ_RULESET, $2);
					  $$->script = $5;
					}
propconst:				{ $$ = NULL; }
	| propconst property		{ $$ = objlstAdd($1, $2); }
	| propconst constant		{ $$ = objlstAdd($1, $2); }
property: BEGIN_PROPERTY nvlst ENDOBJ	{ $$ = cnfobjNew(CNFOBJ_PROPERTY, $2); }
constant: BEGIN_CONSTANT nvlst ENDOBJ	{ $$ = cnfobjNew(CNFOBJ_CONSTANT, $2); }
nvlst:					{ $$ = NULL; }
	| nvlst nv 			{ $2->next = $1; $$ = $2; }
nv:	NAME '=' value 			{ $$ = nvlstSetName($3, $1); }
value:	  STRING			{ $$ = nvlstNewStr($1); }
	| array				{ $$ = nvlstNewArray($1); }
script:	  stmt				{ $$ = $1; }
	| script stmt			{ $$ = scriptAddStmt($1, $2); }
stmt:	  actlst			{ $$ = $1; }
	| IF expr THEN block 		{ $$ = cnfstmtNew(S_IF);
					  $$->d.s_if.expr = $2;
					  $$->d.s_if.t_then = $4;
					  $$->d.s_if.t_else = NULL; }
	| IF expr THEN block ELSE block	{ $$ = cnfstmtNew(S_IF);
					  $$->d.s_if.expr = $2;
					  $$->d.s_if.t_then = $4;
					  $$->d.s_if.t_else = $6; }
	| SET VAR '=' expr ';'		{ $$ = cnfstmtNewSet($2, $4); }
	| UNSET VAR ';'			{ $$ = cnfstmtNewUnset($2); }
	| PRIFILT block			{ $$ = cnfstmtNewPRIFILT($1, $2); }
	| PROPFILT block		{ $$ = cnfstmtNewPROPFILT($1, $2); }
block:    stmt				{ $$ = $1; }
	| '{' script '}'		{ $$ = $2; }
actlst:	  s_act				{ $$ = $1; }
	| actlst '&' s_act 		{ $$ = scriptAddStmt($1, $3); }
/* s_act are actions and action-like statements */
s_act:	  BEGIN_ACTION nvlst ENDOBJ	{ $$ = cnfstmtNewAct($2); }
	| LEGACY_ACTION			{ $$ = cnfstmtNewLegaAct($1); }
	| STOP				{ $$ = cnfstmtNew(S_STOP); }
	| CALL NAME			{ $$ = cnfstmtNewCall($2); }
	| CONTINUE			{ $$ = cnfstmtNewContinue(); }
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
	| expr '&' expr			{ $$ = cnfexprNew('&', $1, $3); }
	| expr '+' expr			{ $$ = cnfexprNew('+', $1, $3); }
	| expr '-' expr			{ $$ = cnfexprNew('-', $1, $3); }
	| expr '*' expr			{ $$ = cnfexprNew('*', $1, $3); }
	| expr '/' expr			{ $$ = cnfexprNew('/', $1, $3); }
	| expr '%' expr			{ $$ = cnfexprNew('%', $1, $3); }
	| '(' expr ')'			{ $$ = $2; }
	| '-' expr %prec UMINUS		{ $$ = cnfexprNew('M', NULL, $2); }
	| FUNC '(' ')'			{ $$ = (struct cnfexpr*) cnffuncNew($1, NULL); }
	| FUNC '(' fparams ')'		{ $$ = (struct cnfexpr*) cnffuncNew($1, $3); }
	| NUMBER			{ $$ = (struct cnfexpr*) cnfnumvalNew($1); }
	| STRING			{ $$ = (struct cnfexpr*) cnfstringvalNew($1); }
	| VAR				{ $$ = (struct cnfexpr*) cnfvarNew($1); }
	| array				{ $$ = (struct cnfexpr*) $1; }
fparams:  expr				{ $$ = cnffparamlstNew($1, NULL); }
	| expr ',' fparams		{ $$ = cnffparamlstNew($1, $3); }
array:	 '[' arrayelt ']'		{ $$ = $2; }
arrayelt: STRING			{ $$ = cnfarrayNew($1); }
	| arrayelt ',' STRING		{ $$ = cnfarrayAdd($1, $3); }

%%
/*
int yyerror(char *s)
{
	printf("parse failure on or before line %d: %s\n", yylineno, s);
	return 0;
}
*/
