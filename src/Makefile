all: ../bin/server

debug: server.c utils.h
	gcc -g -Wall -o ../bin/server server.c

../bin/server: server.c utils.h
	gcc -Ofast -Wall -o ../bin/server server.c

clean:
	rm -f ../bin/server
