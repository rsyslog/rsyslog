#ifndef PARSERIF_H_DEFINED
#define PARSERIF_H_DEFINED
int cnfSetLexFile(char*);
int yyparse();
int yydebug;
void dbgprintf(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void parser_errmsg(char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
