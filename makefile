all: s-talk.o list.o
	gcc -o s-talk s-talk.o list.o -lpthread
	
s-talk.o: s-talk.c
	gcc -c s-talk.c

clean:
	rm -f s-talk s-talk.o 
