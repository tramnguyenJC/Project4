#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <netdb.h>
#include <errno.h>

#define SERVER_HOST "141.166.207.144"  /* wallis IP address */
#define SERVER_PORT "35001"

#define SA struct sockaddr

/* Miscellaneous constants */
#define	MAXLINE		4096	/* max text line length */
#define	MAXSOCKADDR  128	/* max socket address structure size */
#define	BUFFSIZE	8192	/* buffer size for reads and writes */
#define	LISTENQ		1024	/* 2nd argument to listen() */
#define SHORT_BUFFSIZE  100     /* For messages I know are short */

struct header
{
    char type[4];
    int length;
}


struct file_name
{
    char filename[255];  // max of 255 chars in file name
    unsigned char hash[32];
}


struct push_file
{
    unsigned int size; // size in bytes of file
    char name[255]; // file name -- max 255 chars
}
