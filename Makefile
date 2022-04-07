all: affinity.c 
	gcc -g -Wall -lnuma -lpthread -o aff.exe affinity.c

clean: 
	$(RM) aff
