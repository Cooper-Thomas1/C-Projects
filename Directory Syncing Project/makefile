PREAMBLE = cc -std=c11 -Wall -Werror

mysync: mysync.o master.o updatedirs.o patterns.o
	$(PREAMBLE) -g -o mysync mysync.o master.o updatedirs.o patterns.o

mysync.o: mysync.c mysync.h
	$(PREAMBLE) -g -c mysync.c mysync.h

master.o: master.c mysync.h
	$(PREAMBLE) -g -c master.c mysync.h

updatedirs.o: updatedirs.c mysync.h
	$(PREAMBLE) -g -c updatedirs.c mysync.h

patterns.o: patterns.c mysync.h
	$(PREAMBLE) -g -c patterns.c mysync.h
