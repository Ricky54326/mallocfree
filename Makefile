#Hello world from Riccardo and Mitchell

CFLAGS = -Wall -Werror -ggdb -O3 -ffast-math

all: main

main: main.c libmem1.so libmem2.so libmem3.so libcontest1.so libcontest2.so
	gcc -o main main.c -lcontest1 -lpthread -L . $(CFLAGS)

#contest: main.c contest1.so contest2.so

#contest1.so: contest1.o
#	gcc -shared -o libcontest1.so contest1.so $(CFLAGS)

#contest2.so: contest2.o
#	gcc -shared -o libcontest2.so contest2.so $(CFLAGS)

libmem1.so: mem1.o
	gcc -shared -o libmem1.so mem1.o $(CFLAGS)

libmem2.so: mem2.o
	gcc -shared -o libmem2.so mem2.o $(CFLAGS)

libmem3.so: mem3.o
	gcc -shared -o libmem3.so mem3.o $(CFLAGS)

libcontest1.so: contest1.o
	gcc -shared -o libcontest1.so contest1.o $(CFLAGS)

libcontest2.so: contest2.o
	gcc -shared -o libcontest2.so contest2.o $(CFLAGS)

mem1.o: mem1.c
	gcc -c -fpic mem1.c -o mem1.o $(CFLAGS)

mem2.o: mem2.c
	gcc -c -fpic mem2.c -o mem2.o $(CFLAGS)

mem3.o: mem3.c
	gcc -c -fpic mem3.c -o mem3.o $(CFLAGS)

contest1.o: contest1.c
	gcc -c -fpic contest1.c -o contest1.o $(CFLAGS)

contest2.o: contest2.c
	gcc -c -fpic contest2.c -o contest2.o $(CFLAGS)

clean:
	rm -rf *.so *.o main
