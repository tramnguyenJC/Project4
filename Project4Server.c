#include "Project4Header.h"
#include <pthread.h>


// tracks whether a thread is currently writing the log file
// that contains information about each client
boolean log_file_open = false;


/* Sends the given client a list
 * of .mp3 files in the server's
 * directory.
 */
void list( int client_sock );

/* Sends the given client the files that correspond to the file names
 * specified in the packet. The packet will contain one or more file_name
 * structures, the number of which is specified in length.
 */
void send_files( int client_sock, int length, unsigned char* request );

/* Saves the files in the packet to the server's current directory.
 * The packet will contain length push_file structures, each followed
 * by the data of the file to be written.
 */
void write_files( int client_sock, int length, unsigned char* packet );

/* Handles requests from a client and keeps log information. Once the
 * client ends the connection, the log information is written to the
 * server's log file.
 */
void* threadMain( void* thread_arg );



int main( int argc, char* argv[] )
{
    unsigned short port;

    // parse arguments to get port
    if ( argc != 2 )
    {
        fprintf( stderr, "Usage: ./Project4Server -p [port]\n" );
        exit(1);
    }
    else
    {
        if ( argv[1][0] == '-' && argv[1][1] == 'p' )
        {
            port = htons( atoi( argv[2] ) );
        }
        else
        {
            fprintf( stderr, "Usage: ./Project4Server -p [port]\n" );
            exit(1);
        }
    }


    // create socket
    int sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );

    if ( sock < 0 )
    {
        fprintf( stderr, "creation of socket failed\n" );
        exit(1);
    }

    // server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    server_addr.sin_port = port;


    // bind socket
    if ( bind( sock, (struct sockaddr*) &server_addr, sizeof( server_addr ) ) < 0 )
    {
        fprintf( stderr, "binding socket failed\n" );
        exit(1);
    }


    // listen for client connections
    if ( listen( sock, 5 ) < 0 )
    {
        fprintf( stderr, "listen() failed\n" );
        exit(1);
    }


    // infinite loop waiting for connections
    while ( true )
    {
        // client address
        struct sockaddr_in client_addr;
        unsigned int len = sizeof( client_addr );

        // get client connection
        int client = accept( sock, (struct sockaddr*) &client_addr, &len );

        if ( client < 0 )
        {
            fprintf( stderr, "accepting client connection failed\n" );
            exit(1);
        }

        // create thread for client
        pthread_t threadID;


        if ( pthread_create( &threadID, NULL, threadMain, (void*) &client ) != 0 )
        {
            fprintf( stderr, "creating thread failed\n" );
            exit(1);
        }

    }
}


void list( int client_sock )
{
    // print current operation
    printf( "processing LIST command\n" );


    // get number of music files in current directory
    FILE* pipe = popen( "find -maxdepth 1 -iname '*.mp3' | wc -l", "r" );
    char num[ 10 ];

    fgets( num, 10, pipe );
    int num_files = atoi( num );

    pclose( pipe );


    size_t file_count = 0;
    struct file_name files[ num_files ];

    // use ls to get all music files in current directory
    pipe = popen( "find -maxdepth 1 -iname '*.mp3'", "r" );

    // get first filename
    char file[ FILE_NAME_MAX ];
    char* success = fgets( file, FILE_NAME_MAX, pipe );

    while( success != 0 )
    {
        // zero out struct
        memset( &files[ file_count ], 0, sizeof( struct file_name ) );

        // copy all but leading "./" and ending newline character into struct
        strncpy( files[ file_count ].filename, &file[2], strlen( file ) - 3 );


        // add hash of file to the struct
        char* hash = computeHash( files[ file_count ].filename );
        memcpy( files[ file_count ].hash, hash, 64 );
        free( hash );


        ++file_count;

        // get next file name
        success = fgets( file, FILE_NAME_MAX, pipe );
    }

    pclose( pipe );



    // create a packet from the list of music files
    struct header response_header;
    response_header.length = num_files;

    char* msg_type = "LIST";
    memcpy( &(response_header.type), msg_type, 4 );

    struct file_name files_list[ file_count ];
    memset( files_list, 0, sizeof( struct file_name ) * file_count );


    // copy header and file names into buffer
    size_t header_len = sizeof( struct header );
    size_t data_len = sizeof( struct file_name ) * num_files;

    size_t packet_len = header_len + data_len;

    unsigned char packet[ packet_len ];

    // copy header
    memcpy( packet, &response_header, header_len );

    // copy file names
    memcpy( &packet[ header_len ], files_list, data_len );


    // send response to client
    if( send( client_sock, packet, packet_len, 0 ) != packet_len )
    {
        fprintf( stderr, "send() failed\n" );
        close( client_sock );
        exit(1);
    }
}


void* threadMain( void* thread_arg )
{
    int client_sock = *( (int*) thread_arg );

    pthread_t threadID = pthread_self();

    // variables for keeping track of client activity
    // need to keep client IP address
    // commands sent from client
    // a list of the files the server receives
    // and a list of any files the client requests

    // deallocates thread resources
    pthread_detach( threadID );

    // wait for client message

    // determine type of message and send to corresponding function
    // print thread ID (to precede message printed from called function)
    printf( "thread %lu ", (unsigned long int) threadID );


    // check if log file is open in another thread
    if ( !log_file_open )
    {
        // if not, open and add activity information for this client
    }

    return NULL;
}