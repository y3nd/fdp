all: ../bin/server1-FDP ../bin/server2-FDP ../bin/server3-FDP

../bin/server1-FDP: server.c utils.h config.h
	echo "#define BASE_WINDOW_SIZE 69" > config.h
	echo "#define MAX_WINDOW_SIZE 69" >> config.h
	gcc -o ../bin/server1-FDP server.c

../bin/server2-FDP: server.c utils.h config.h
	echo "#define BASE_WINDOW_SIZE 9" > config.h
	echo "#define MAX_WINDOW_SIZE 9" >> config.h
	gcc -o ../bin/server2-FDP server.c

../bin/server3-FDP: server.c utils.h config.h
	echo "#define BASE_WINDOW_SIZE 69" > config.h
	echo "#define MAX_WINDOW_SIZE 69" >> config.h
	gcc -o ../bin/server3-FDP server.c

clean:
	rm -f ../bin/server?-FDP
