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
 * specified by the client. Reads in the entries from the client socket
 * and responds with the specified files. The length describes the number
 * of file requests in the client message (as specified by the header).
 */
void send_files( int client_sock, int length );

/* Reads in length push_file structs and corresponding files from the client
 * and writes them to the server's directory.
 */
void write_files( int client_sock, int length );

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
    // get number of music files in current directory
    FILE* pipe = popen( "find -maxdepth 1 -iname '*.mp3' | wc -l", "r" );
    char num[ 10 ];
    memset( num, 0, 10 );

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


    // copy header and file names into buffer
    size_t header_len = sizeof( struct header );
    size_t data_len = sizeof( struct file_name ) * num_files;

    size_t packet_len = header_len + data_len;

    unsigned char packet[ packet_len ];

    // copy header
    memcpy( packet, &response_header, header_len );

    // copy file names
    memcpy( &packet[ header_len ], files, data_len );


    // send response to client
    if( send( client_sock, packet, packet_len, 0 ) != packet_len )
    {
        fprintf( stderr, "send() failed\n" );
        close( client_sock );
        exit(1);
    }
}


void send_files( int client_sock, int length )
{
    // get file names
    struct file_name files[ length ];

    if ( recv( client_sock, files, length * sizeof( struct file_name ), 0 ) < 0 )
    {
        fprintf( stderr, "recv() failed\n" );
        close( client_sock );
        exit(1);
    }


    // struct for each file
    struct push_file file_sizes[ length ];

    // keeps track of total size of packet
    size_t packet_size = sizeof( struct header );



    // get the size of each requested file that is found
    int file_found_count = 0;

    int i;
    for ( i = 0; i < length; ++i )
    {
        // get the size of this file
        // command: find -iname '$[filename]' -printf '%b\n'

        size_t name_len = strlen( files[file_found_count].filename );
        size_t command_len = strlen( "find -iname '" );
        size_t options_len = strlen( "' -printf '%s\n'" );

        size_t len = command_len + name_len + options_len;

        // copy command and filename for call
        char command[ len + 1 ];

        strncpy( command, "find -iname '", command_len );
        strncpy( command, files[file_found_count].filename, name_len );
        strncpy( command, "' -printf '%s\n'", options_len );

        command[ len ] = '\0';


        // gets size of file (in bytes)
        FILE* pipe = popen( command, "r" );

        char num[10];
        memset( num, 0, 10 );

        fgets( num, 10, pipe );
        pclose( pipe );


        unsigned int size = strtoul( num, 0, 10 );

        // atoi returns 0 from an empty string, so
        // zero indicates the file wasn't found
        if ( size != 0 )
        {
            // copy file name and size
            strncpy( file_sizes[file_found_count].name, files[file_found_count].filename, name_len );
            file_sizes[ file_found_count ].size = size;


            // update packet size to account for this file
            packet_size += sizeof( struct push_file );
            packet_size += file_sizes[file_found_count].size;

            ++file_found_count;
        }
    }



    // construct response message
    unsigned char packet[ packet_size ];
    memset( packet, 0, packet_size );


    // create header and copy to message
    struct header push_header;
    memcpy( push_header.type, "PUSH", 4 );
    push_header.length = file_found_count;

    memcpy( packet, &push_header, sizeof( struct header ) );


    // pointer to current position in message
    int current_index = sizeof( struct header );

    // copy requested files to message
    for ( i = 0; i < file_found_count; ++i )
    {
        // copy file information first
        memcpy( &packet[ current_index ], &file_sizes[i], sizeof( struct push_file ) );


        current_index += sizeof( struct push_file );

        // then read in contents of file
        FILE* file = fopen( file_sizes[i].name, "r" );

        unsigned char buffer[ file_sizes[i].size ];

        fread( buffer, 1, file_sizes[i].size, file );

        fclose( file );


        // copy contents of file to message
        memcpy( &packet[ current_index ], buffer, file_sizes[i].size );

        current_index += file_sizes[i].size;
    }



    // send message to client
    if( send( client_sock, packet, packet_size, 0 ) != packet_size )
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

    // TODO: variables for keeping track of client activity
    // need to keep client IP address
    // commands sent from client
    // a list of the files the server receives
    // and a list of any files the client requests

    // deallocates thread resources
    pthread_detach( threadID );

    // keeps track of whether the client has requested to close the connection
    boolean connection_closed = false;

    // wait for client message
    while ( ! connection_closed )
    {
        size_t header_len = sizeof( struct header );
        unsigned char buffer[ header_len ];

        // first just read in the header to check message type
        if ( recv( client_sock, buffer, header_len, 0 ) < 0 )
        {
            fprintf( stderr, "recv() failed\n" );
            close( client_sock );
            exit(1);
        }

        // copy header into struct for parsing
        struct header request_header;
        memcpy( &request_header, buffer, header_len );


        // print thread ID (to precede message specifying next action)
        printf( "thread %lu: ", (unsigned long int) threadID );


        // determine type of message and send to corresponding function
        if ( strncmp( request_header.type, "LIST", 4 ) == 0 )
        {
            // request for a list of the server's files
            printf( "processing LIST request\n" );
            list( client_sock );
        }
        else if (  strncmp( request_header.type, "PULL", 4 ) == 0 )
        {
            // request for the server to transmit specified files
            printf( "processing PULL request\n" );
            send_files( client_sock, request_header.length );

            // TODO: add files sent to the list of what the client has
        }
        else if ( strncmp( request_header.type, "PUSH", 4 ) == 0 )
        {
            // packet contains files to write to server's directory
            printf( "receiving files from client\n" );
            write_files( client_sock, request_header.length );

            // TODO: add files read in to the list of what the client has
        }
        else if ( strncmp( request_header.type, "BYE!", 4 ) == 0 )
        {
            // request to close connection
            printf( "closing connection\n" );
            connection_closed = true;

            close( client_sock );
        }
    }


    // TODO: write client data to log file
    // check if log file is open in another thread
    if ( !log_file_open )
    {
        // if not, open and add activity information for this client

        // if so, wait and try again until it is free
    }

    return NULL;
}


void write_files( int client_sock, int length )
{
    // get files from client
    int i;
    for ( i = 0; i < length; ++i )
    {
        struct push_file prefix;

        // read in file prefix with name and size
        if ( recv( client_sock, &prefix, sizeof( struct push_file ), 0 ) < 0 )
        {
            fprintf( stderr, "recv() failed\n" );
            close( client_sock );
            exit(1);
        }


        // read in contents of file
        unsigned char file_bytes[ prefix.size ];
        memset( file_bytes, 0, prefix.size );

        if ( recv( client_sock, file_bytes, prefix.size, 0 ) < 0 )
        {
            fprintf( stderr, "recv() failed\n" );
            close( client_sock );
            exit(1);
        }


        // create file using filename
        // adding .part suffix at first
        // to avoid including incomplete files in a LIST request

        size_t file_name_len = strlen( prefix.name );

        // add 6 to accommodate ".part" suffix and \0
        char new_file_name[ file_name_len + 6 ];

        // copy file name and suffix
        strncpy( new_file_name, prefix.name, file_name_len );
        strncpy( &new_file_name[ file_name_len ], ".part", 5 );

        new_file_name[ file_name_len + 5 ] = '\0';


        // create file
        FILE* new_file = fopen( new_file_name, "w" );

        // write bytes to file
        fwrite( file_bytes, 1, prefix.size, new_file );

        fclose( new_file );

        // rename to remove .part once the entire file is written
        rename( new_file_name, prefix.name );


        // TODO: may want to consider adding a hash to check that the file is correct?
    }
}