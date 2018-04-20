all: server client
	
server: Project4Server.c
	g++ -Wall -std=c++11 -pthread Project4Server.c -o Project4Server

client: Project4Client.c
	g++ -Wall -std=c++11 Project4Client.c -o Project4Client
