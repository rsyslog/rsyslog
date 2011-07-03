rscript: lex.yy.c utils.o rscript.tab.h utils.h
	gcc -g -o rscript lex.yy.c rscript.tab.c utils.o -lestr

lex.yy.c: rscript.l rscript.tab.h
	flex rscript.l

rscript.tab.h: rscript.y
	bison -d rscript.y

utils.o: utils.c utils.h
	gcc -g -Wall -c utils.c

clean:
	rm *.o
