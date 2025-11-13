# Noah Hathout
# nhathout
# makefile
# reference: https://stackoverflow.com/questions/1484817/how-do-i-make-a-simple-makefile-for-gcc-on-linux

CC=gcc

all: MyBitApp

MyBitApp: bits.o mylist.o main.o
	gcc -o MyBitApp bits.o mylist.o main.o

bits.o: bits.c bits.h

mylist.o: mylist.c mylist.h

main.o: main.c


clean:
	rm -f MyBitApp *.o

run: MyBitApp
	./MyBitApp input.txt output.txt
	
