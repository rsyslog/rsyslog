
tester: hashtable.o tester.o hashtable_itr.o
	gcc -g -Wall -O -lm -o tester hashtable.o hashtable_itr.o tester.o

all: tester old_tester

tester.o:	tester.c
	gcc -g -Wall -O -c tester.c -o tester.o

old_tester: hashtable_powers.o tester.o hashtable_itr.o
	gcc -g -Wall -O -o old_tester hashtable_powers.o hashtable_itr.o tester.o

hashtable_powers.o:	hashtable_powers.c
	gcc -g -Wall -O -c hashtable_powers.c -o hashtable_powers.o

hashtable.o:	hashtable.c
	gcc -g -Wall -O -c hashtable.c -o hashtable.o

hashtable_itr.o: hashtable_itr.c
	gcc -g -Wall -O -c hashtable_itr.c -o hashtable_itr.o

tidy:
	rm *.o

clean: tidy
	rm -f tester old_tester
