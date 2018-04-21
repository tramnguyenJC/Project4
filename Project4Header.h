#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <netdb.h>
#include <errno.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

#define SERVER_HOST "141.166.207.144"  /* wallis IP address */
#define SERVER_PORT "35001"

#define SA struct sockaddr

#include <iostream>
using namespace std;

/* Miscellaneous constants */
#define	MAXLINE		4096	/* max text line length */
#define	MAXSOCKADDR  128	/* max socket address structure size */
#define	BUFFSIZE	8192	/* buffer size for reads and writes */
#define	LISTENQ		1024	/* 2nd argument to listen() */
#define SHORT_BUFFSIZE  100     /* For messages I know are short */

#define boolean int
#define true 1
#define false 0


int FILE_NAME_MAX = 255;

// structure that forms the beginning of each message
struct header
{
    char type[4];
    int length;  // the number of data items included in the message
};


// structure used for LIST requests -- provides the name of each
// file and its hash to detect duplicate files
struct file_name
{
    char filename[ 255 ];
    char hash[64];
};


// structure for messages that include files
// each file is preceded by this structure, which
// indicates the file's name and size (in bytes)
struct push_file
{
    unsigned int size;
    char name[ 255 ];
};

/* Computes and returns the SHA256 hash of the file in the current
 * directory with name filename.
 * Returns a pointer to a char array that represents the string of
 * the hash. The array is 64 characters long and not null-terminated.
 */
char* computeHash( char* filename )
{
    // make enough room for command "sha256sum", quotes, and filename
    size_t fcn_len = strlen( "sha256sum \"" );
    size_t len = strlen( filename );

    size_t command_len = fcn_len + len + 1;

    // construct string for command to hash specified file
    char command[ command_len + 1 ];
    strncpy( command, "sha256sum \"", fcn_len );
    strncpy( &command[ fcn_len ], filename, len );
    command[ command_len - 1 ] = '\"';
    command[ command_len ] = '\0';

    // hash will always be 64 chars
    char* hash_str = (char*) malloc( 64 );

    // get hash of file i
    FILE* hash_pipe = popen( command, "r" );
    fgets( hash_str, 64, hash_pipe );
    pclose( hash_pipe );

    return hash_str;
}
