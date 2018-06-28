all:server client.c
	gcc -o client client.c -lrt -pthread
server:server.c
	gcc -o server server.c -lrt -pthread
