all: server client
	
server: Project4Server.c
	gcc -Wall -pthread Project4Server.c -o Project4Server

client: Project3Client.c
	gcc -Wall Project4Client.c -o Project4Client
