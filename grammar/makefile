rscript: lex.yy.c utils.o
	gcc -o rscript lex.yy.c utils.o -lestr -lfl

lex.yy.c: rscript.l
	flex rscript.l

utils.o: utils.c
	gcc -c utils.c
