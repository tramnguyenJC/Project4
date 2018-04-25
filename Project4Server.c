#include "Project4Header.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>


/* Variables */

// struct for arguments to each thread
struct thread_args
{
    int client_sock; // socket for client connection
    char client_ip[ 16 ]; // string of client's IP address
};


// tracks whether a thread is currently writing the log file
// that contains information about each client
static boolean log_file_open = false;

// name of the file to which log information should be written
static char* log_file_name;


/* Functions */

/* Sends the given client a list of .mp3 files in the server's directory.
 */
void list( int client_sock );

/* Sends the given client the files that correspond to the file names
 * specified by the client. Reads in the entries from the client socket
 * and responds with the specified files. The length describes the number
 * of file requests in the client message (as specified by the header).
 * Returns a list of the names of the files sent to the client.
 */
char** send_files( int client_sock, int length );

/* Handles requests from a client and keeps log information. Once the
 * client ends the connection, the log information is written to the
 * server's log file.
 */
void* threadMain( void* thread_arg );

/* Reads in length push_file structs and corresponding files from the client
 * and writes them to the server's directory. Returns a list of the file names
 * received from the client.
 */
char** write_files( int client_sock, int length );

/* Appends information to the file specified by log_file_name. If the client
 * IP address client_ip is not in the file, an entry for that address is added.
 * The file names specified in client_files are appended to the list of files the
 * server has sent and received from the client. The messages stored in client_activity
 * are appended to the client's activity. The length of the list of files is in
 * num_files, and the length of the activity log is in log_len.
 */
void write_log_file( char** client_files, size_t num_files, char** client_activity, size_t log_len, char* client_ip );



/* main code */

int main( int argc, char* argv[] )
{
    unsigned short port;

    // parse arguments to get port
    if ( argc != 5 )
    {
        fprintf( stderr, "Usage: ./Project4Server -p <port> -l <log file name>\n" );
        exit(1);
    }
    else
    {
        int i;
        for ( i = 0; i < argc; ++i )
        {
            if ( argv[i][0] == '-' )
            {
                if ( argv[i][1] == 'p' )
                {
                    port = htons( atoi( argv[ 2 ] ) );
                }
                else if ( argv[i][1] == 'l' )
                {
                    // copy given file name to log_file_name
                    size_t file_name_len = strlen( argv[ i + 1 ] ) + 1;
                    log_file_name = (char*) malloc( file_name_len );

                    strncpy( log_file_name, argv[ i + 1 ], file_name_len );
                    log_file_name[ file_name_len - 1 ] = '\0';
                }
                else
                {
                    fprintf( stderr, "Usage: ./Project4Server -p <port> -l <log file name>\n" );
                    exit(1);
                }
            }
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
    if ( (bind( sock, (struct sockaddr*) &server_addr, sizeof( server_addr ))) < 0 )
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


        // passes client socket and ip address to the thread
        struct thread_args* args = (struct thread_args*) malloc( sizeof( struct thread_args ) );
        memset( args, 0, sizeof( struct thread_args ) );
        args->client_sock = client;

        // get client IP address and copy into thread_args
        char* ip_addr = inet_ntoa( client_addr.sin_addr );
        strncpy( args->client_ip, ip_addr, strlen( ip_addr ) );

        // create thread for client
        pthread_t threadID;


        if ( pthread_create( &threadID, NULL, threadMain, (void*) args ) != 0 )
        {
            fprintf( stderr, "creating thread failed\n" );
            exit(1);
        }

    }
}


/* Sends the given client a list of .mp3 files in the server's directory.
 */
void list( int client_sock )
{
    // get number of music files in current directory
    FILE* pipe = popen( "find -maxdepth 1 -iname '*.mp3' | wc -l", "r" );
    char num[ 10 ];
    memset( num, 0, 10 );

    // read in number of files
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

    const char* msg_type = "LIST";
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
    if( send( client_sock, packet, packet_len, 0) != packet_len )
    {
        fprintf( stderr, "send() failed: %s\n", strerror( errno ) );
        close( client_sock );
        exit(1);
    }
}


/* Sends the given client the files that correspond to the file names
 * specified by the client. Reads in the entries from the client socket
 * and responds with the specified files. The length describes the number
 * of file requests in the client message (as specified by the header).
 * Returns a list of the names of the files sent to the client.
 */
char** send_files( int client_sock, int length )
{
    // get file names
    struct file_name files[ length ];

    // create return array of strings
    char** added_files = (char**) malloc( length * sizeof( char* ) );

    // packet should consist of length file_name structs
    int i;
    for ( i = 0; i < length; ++i )
    {
        unsigned char buffer[ sizeof( struct file_name ) ];

        // for each struct, read remaining bytes until all bytes received
        // may need to make multiple calls to get all bytes
        size_t bytes_received = 0;
        size_t bytes_expected = length * sizeof( struct file_name );

        while( bytes_received < bytes_expected )
        {
            int new_bytes = recv( client_sock, &buffer[ bytes_received ], bytes_expected - bytes_received, 0 );

            if ( new_bytes < 0 )
            {
                fprintf( stderr, "recv() failed: %s\n", strerror( errno ) );
                close( client_sock );
                exit(1);
            }

            // add new bytes read in to overall count
            bytes_received += new_bytes;
        }

        // once all bytes are read, copy into struct
        memcpy( &files[i], buffer, sizeof( struct file_name ) );
    }

    // struct for each file
    struct push_file file_sizes[ length ];

    // keeps track of total size of packet
    size_t packet_size = sizeof( struct header );

    // get the size of each requested file that is found
    int file_found_count = 0;


    for ( i = 0; i < length; ++i )
    {
        // get the size of this file (in bytes)
        struct stat file_size;
        int success = stat( files[file_found_count].filename, &file_size );


        unsigned int size = file_size.st_size;


        // stat() returns 0 to signal it successfully found the file
        if ( success == 0 )
        {
            size_t name_len = strlen( files[ file_found_count ].filename );

            // copy file name and size
            strncpy( file_sizes[file_found_count].name, files[file_found_count].filename, name_len );
            file_sizes[ file_found_count ].size = size;


            // update packet size to account for this file
            packet_size += sizeof( struct push_file );
            packet_size += file_sizes[file_found_count].size;


            // add file to list to be returned
            added_files[ file_found_count ] = malloc( name_len + 1 );
            strncpy( added_files[ file_found_count ], files[ file_found_count ].filename, name_len );
            added_files[ file_found_count ][ name_len ] =  '\0';



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

        unsigned char* buffer = (unsigned char*) malloc(file_sizes[i].size ) ;

        fread( buffer, 1, file_sizes[i].size, file );

        fclose( file );

        // copy contents of file to message
        memcpy( &packet[ current_index ], buffer, file_sizes[i].size );
        current_index += file_sizes[i].size;
        free(buffer);
    }


    // send message to client
    if( send( client_sock, packet, packet_size, 0 ) != packet_size )
    {
        fprintf( stderr, "send() failed\n" );
        close( client_sock );
        exit(1);
    }


    return added_files;
}


/* Handles requests from a client and keeps log information. Once the
 * client ends the connection, the log information is written to the
 * server's log file.
 */
void* threadMain( void* thread_arg )
{
    // get client socket and IP address from arg structure
    struct thread_args* args = ( struct thread_args* ) thread_arg;
    int client_sock = args->client_sock;
    char* ip_addr = args->client_ip;

    pthread_t threadID = pthread_self();

    printf( "thread %lu: connection established\n", threadID );


    // variables that keep info on client activity
    // start with 100 strings each, expand later if necessary
    int files_cap = 100;
    int log_cap = 100;

    char** client_files = (char**) malloc( files_cap * sizeof( char* ) );  // list of files sent to/received from client
    char** activity_log = (char**) malloc( log_cap * sizeof( char* ) );  // list of messages received from client

    size_t num_files = 0;  // number of files sent to/received from client
    size_t num_messages = 0;  // total number of messages received and logged


    // get time of connection beginning
    time_t t;
    struct tm* time_struct;

    time( &t );
    time_struct = localtime( &t );

    char* time_str = asctime( time_struct );


    // add information for client connection to activity log string
    size_t time_len = strlen( time_str );
    size_t str_len = strlen( "\tConnection opened " );

    size_t start_len = time_len + str_len + 1; // add enough room for \n and \0
    activity_log[0] = (char*) malloc( start_len );

    // copy connection start time
    strncpy( activity_log[0], "\tConnection opened ", str_len );
    strncpy( &activity_log[0][ str_len ], time_str, time_len );

    activity_log[0][ start_len - 1 ] = '\0';

    ++num_messages;


    // deallocates thread resources
    pthread_detach( threadID );


    // pointer to any new info about client files from messages
    char** new_files = NULL;


    // ends the loop when the client sends message indicating end of connection
    boolean connection_open = true;

    // infinite loop waiting for client message
    while ( connection_open )
    {
        size_t header_len = sizeof( struct header );
        unsigned char buffer[ header_len ];

        // first just read in message header to check message type
        size_t bytes_received = 0;
        size_t bytes_expected = sizeof( struct header );

        // make sure all bytes of header get read in correctly
        while( bytes_received < bytes_expected )
        {
            // read in up to the full header
            int new_bytes = recv( client_sock, buffer, bytes_expected - bytes_received, 0 );

            // if there's an error, print and exit
            if ( new_bytes < 0 )
            {
                fprintf( stderr, "recv() failed: %s\n", strerror( errno ) );
                close( client_sock );
                exit(1);
            }

            // update received count based on the last call
            bytes_received += new_bytes;
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

            // add request to client activity information
            char* log_msg = "\tLIST request\n";
            activity_log[ num_messages ] = (char*) malloc( strlen( log_msg ) + 1 );
            strncpy( activity_log[ num_messages ], log_msg, strlen( log_msg ) );
            activity_log[ num_messages ][ strlen( log_msg ) ] = '\0';

            // add the new message to the count
            ++num_messages;
        }
        else if ( strncmp( request_header.type, "PULL", 4 ) == 0 )
        {
            // request for the server to transmit specified files
            printf( "processing PULL request\n" );
            new_files = send_files( client_sock, request_header.length );

            char* log_msg = "\tSent files to client:\n";

            size_t total_len = strlen( log_msg );

            // get total length of log message string including file names
            int i;
            for ( i = 0; i < request_header.length; ++i )
            {
                // include length of string, adding two tabs preceding it and newline at the end
                total_len += strlen( new_files[i] ) + 3;
            }

            // account for null terminator
            ++total_len;


            // allocate overall string and copy everything to it
            activity_log[ num_messages ] = (char*) malloc( total_len );

            // copy log message
            strncpy( activity_log[ num_messages ], log_msg, strlen( log_msg ) );


            // keeps the index of where to copy each subsequent part into the string
            size_t index = strlen( log_msg );

            // copy each file name, preceded by two tabs and followed by a newline
            for ( i = 0; i < request_header.length; ++i )
            {
                // copy tabs
                strncpy( &activity_log[ num_messages ][ index ], "\t\t", 2 );
                index += 2;

                // copy filename
                strncpy( &activity_log[ num_messages ][ index ], new_files[i], strlen( new_files[i] ) );
                index += strlen( new_files[i] );

                // add newline
                activity_log[ num_messages ][ index ] = '\n';
                ++index;
            }

            activity_log[ num_messages ][ total_len - 1 ] = '\0';


            // add the new message to the count
            ++num_messages;
        }
        else if ( strncmp( request_header.type, "PUSH", 4 ) == 0 )
        {
            // packet contains files to write to server's directory
            printf( "receiving files from client\n" );

            new_files = write_files( client_sock, request_header.length );

            char* log_msg = "\tReceived files from client:\n";

            // get total length of message, including file names
            size_t total_len = strlen( log_msg );

            int i;
            for ( i = 0; i < request_header.length; ++i )
            {
                // include length of string, prepending two tabs and a newline
                total_len += strlen( new_files[i] ) + 3;
            }

            // account for null terminator
            ++total_len;


            // allocate overall string and copy everything to it
            activity_log[ num_messages ] = (char*) malloc( total_len );

            // copy log message
            strncpy( activity_log[ num_messages ], log_msg, strlen( log_msg ) );


            // index of where to copy each part into the string
            size_t index = strlen( log_msg );

            // copy each file name, preceded by two tabs and followed by a newline
            for ( i = 0; i < request_header.length; ++i )
            {
                // copy tabs
                strncpy( &activity_log[ num_messages ][ index ], "\t\t", 2 );
                index += 2;

                // copy filename
                strncpy( &activity_log[ num_messages ][ index ], new_files[i], strlen( new_files[i] ) );
                index += strlen( new_files[i] );

                // add newline
                activity_log[ num_messages ][ index ] = '\n';
                ++index;
            }

            activity_log[ num_messages ][ total_len - 1 ] = '\0';


            // add the new message to the count
            ++num_messages;
        }
        else if ( strncmp( request_header.type, "BYE!", 4 ) == 0 )
        {
            // request to close connection
            printf( "closing connection\n" );
            close( client_sock );

            // get time
            time( &t );
            time_struct = localtime( &t );
            time_str = asctime( time_struct );

            // create log message with time that connection was closed

            size_t initial_len = strlen( "\tConnection closed " );
            size_t log_msg_len = initial_len + strlen( time_str ) + 2; // account for newline and \0

            activity_log[ num_messages ] = (char*) malloc( log_msg_len );

            // copy first part of message
            strncpy( activity_log[ num_messages ], "\tConnection closed ", initial_len );

            // copy time
            strncpy( &activity_log[ num_messages ][ initial_len ], time_str, strlen( time_str ) );

            activity_log[ num_messages ][ log_msg_len - 2 ] = '\n';
            activity_log[ num_messages ][ log_msg_len - 1 ] = '\0';


            // add the new message to the count
            ++num_messages;


            connection_open = false;
        }


        // add any new files to/from the client into the list
        if ( new_files != NULL )
        {
            // check if the list is large enough and enlarge if needed
            if ( num_files + request_header.length >= files_cap )
            {
                // double capacity, allocate new memory, and copy old contents
                files_cap *= 2;
                char** new_list = (char**) malloc( files_cap * sizeof( char* ) );
                memcpy( client_files, new_list, num_files * sizeof( char* ) );

                client_files = new_list;
            }


            // add new files to list
            int i;
            for ( i = 0; i < request_header.length; ++i )
            {
                // copy each file name into client_files and free memory
                size_t file_name_len = strlen( new_files[i] );

                client_files[ num_files ] = (char*) malloc( file_name_len );
                strncpy( client_files[ num_files ], new_files[i], file_name_len );

                free( new_files[i] );

                ++num_files;
            }

            free( new_files );


            // reset pointer
            new_files = NULL;
        }


        // check whether the activity log needs to be expanded
        if ( num_messages == log_cap )
        {
            // increase size of array and copy previous data
            log_cap *= 2;

            char** new_log = (char**) malloc( log_cap * sizeof( char* ) );
            memcpy( new_log, activity_log, num_messages * sizeof( char* ) );

            activity_log = new_log;
        }
    }



    printf( "thread %lu: writing log information...", threadID );
    fflush( stdout );

    // write information collected about client to log file
    write_log_file( client_files, num_files, activity_log, num_messages, ip_addr );

    printf("complete\n");


    // free variables
    int i;
    for ( i = 0; i < num_files; ++i )
    {
        free( client_files[i] );
    }

    for ( i = 0; i < num_messages; ++i )
    {
        free( activity_log[i] );
    }

    free( client_files );
    free( activity_log );
    free( thread_arg );


    return NULL;
}


/* Reads in length push_file structs and corresponding files from the client
 * and writes them to the server's directory. Returns a list of the file names
 * received from the client.
 */
char** write_files( int client_sock, int length )
{
    // array of file names to return
    char** added_files = (char**) malloc( sizeof(char*) * length );

    // get files from client
    int i;
    for ( i = 0; i < length; ++i )
    {
        struct push_file prefix;

        // read in file prefix that contains name and size

        size_t bytes_received = 0;
        size_t bytes_expected = sizeof( struct push_file );

        // make sure all bytes get received correctly
        // get all bytes into temporary buffer first
        unsigned char buffer[ sizeof( struct push_file ) ];

        while( bytes_received < bytes_expected )
        {
            // get up to the entire packet
            ssize_t new_bytes = recv( client_sock, &buffer[ bytes_received ], bytes_expected - bytes_received, 0 );

            if ( new_bytes < 0 )
            {
                fprintf( stderr, "recv() failed\n" );
                close( client_sock );
                exit(1);
            }

            // update based on the number of bytes read in
            bytes_received += new_bytes;
        }

        // once the entire struct has been received, copy into prefix
        memcpy( &prefix, buffer, sizeof( struct push_file ) );


        printf("Saving file: %s...", prefix.name);
 
        // read in contents of file

        bytes_received = 0;
        bytes_expected = prefix.size;

        unsigned char file[ bytes_expected ];

        // read in bytes until the entire file has been received
        while ( bytes_received < bytes_expected )
        {
            // read in more data, up to the amount remaining in the file
            ssize_t new_bytes = recv( client_sock, &file[ bytes_received ], bytes_expected - bytes_received, 0 );

            // the loop will continue until all bytes of the file have been read in (but no more)
            bytes_received += new_bytes;
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


        // for the edge case of two clients syncing the same file at the same time
        FILE* open_file = fopen( new_file_name, "r" );

        if ( open_file != NULL )
        {
            // don't write the file if another client has already sent it
            fclose( open_file );
            continue;
        }


        // create file
        FILE* new_file = fopen( new_file_name, "w" );

        // write bytes to file
        fwrite( file, sizeof(char), prefix.size, new_file );
        fclose( new_file );
		
        // rename to remove .part once the entire file is written
        rename( new_file_name, prefix.name );

        printf( "complete\n" );


        // add file name to list to be returned
        added_files[i] = (char*) malloc( file_name_len + 1 );
        strncpy( added_files[i], prefix.name, file_name_len );
        added_files[i][ file_name_len ] = '\0';
    }

    return added_files;
}


/* Appends information to the file specified by log_file_name. If the client
 * IP address client_ip is not in the file, an entry for that address is added.
 * The file names specified in client_files are appended to the list of files the
 * server has sent and received from the client. The messages stored in client_activity
 * are appended to the client's activity. The length of the list of files is in
 * num_files, and the length of the activity log is in log_len.
 */
void write_log_file( char** client_files, size_t num_files, char** client_activity, size_t log_len, char* client_ip )
{
    // check if log file is open in another thread
    while ( log_file_open )
    {
        // if so, wait 1s and try again until it is free
        sleep( 1 );
    }


    // once the log file is free, open and add activity info for this client
    log_file_open = true;

    // string "Client: [IPaddress]\n" is the beginning of that client's info
    size_t line_len = strlen( "Client: " ) + strlen( client_ip ) + 2;

    char begin_str[ line_len ];
    strncpy( begin_str, "Client: ", strlen( "Client: " ) );
    strncpy( &begin_str[ strlen( "Client: " ) ], client_ip, strlen( client_ip ) );
    begin_str[ line_len - 2 ] = '\n';
    begin_str[ line_len - 1 ] = '\0';



    // first open in read mode to search for client
    FILE* log_file = fopen( log_file_name, "r" );

    // search file for beginning of client's information
    boolean found = false;

    // a line will be an absolute maximum of 258 chars -- at most filename + two tabs and newline
    int line_max = 258;

    long client_pos; // position of this client's entry, if found

    // iterate until line is found or at the end of file
    while( !found && !feof( log_file ) )
    {
        char line[ line_max ];
        memset( line, 0, line_max );

        fgets( line, line_max, log_file );

        // the beginning line will always be line_len characters long
        if ( strncmp( line, begin_str, line_len ) == 0 )
        {
            found = true;
            client_pos = ftell( log_file );
        }
    }
    fclose( log_file );


    // if the client was not found in the file, append a new entry to the end of the file
    if ( !found )
    {
        // open file to append to end
        log_file = fopen( log_file_name, "a" );

        // add entry for this client, preceded by newlines to separate entries
        fputs( "\n\n", log_file );
        fputs( begin_str, log_file );

        // add entry for this client's files
        fputs( "Files:\n", log_file );


        // add each file from the list
        int i;
        for ( i = 0; i < num_files; ++i )
        {
            // write file name to list
            fprintf( log_file, "\t%s\n", client_files[i] );
        }


        // add entry for this client's activity
        fputs( "\nActivity:\n", log_file );

        // add client activity from the last session
        for ( i = 0; i < log_len; ++i )
        {
            // write each message to the activity list
            fputs( client_activity[i], log_file );
        }


        fclose( log_file );
    }
    else
    {
        log_file = fopen( log_file_name, "r" );

        // get size of log file
        struct stat file_size;
        stat( log_file_name, &file_size );


        // get total number of bytes to write to the file
        size_t new_info_len = 0;

        int i;
        for ( i = 0; i < num_files; ++i )
        {
            new_info_len += strlen( client_files[i] );
        }

        for( i = 0; i < log_len; ++i )
        {
            new_info_len += strlen( client_activity[i] );
        }


        // allocate a buffer big enough for the file and new information
        // add space for \0 and to add an extra line between client sessions
        size_t buff_size = new_info_len + file_size.st_size + 3;
        char buffer[ buff_size ];
        memset( buffer, 0, buff_size );

        size_t buffer_index = 0;

        // copy file to buffer up to this client's entry
        fread( buffer, 1, client_pos, log_file );

        buffer_index += client_pos;

        // the next line should be "Files:\n"
        fgets( &buffer[ buffer_index ], line_max, log_file );
        buffer_index += strlen( "Files:\n" );

        // append the list of files after this
        for ( i = 0; i < num_files; ++i )
        {
            size_t file_len = strlen( client_files[i] );
            strncpy( &buffer[ buffer_index ], client_files[i], file_len );
            buffer_index += file_len;
        }


        // navigate until the beginning of the activity log
        // starts with the line "Activity:\n"
        char line[ line_max ];
        memset( line, 0, line_max );
        fgets( line, line_max, log_file );

        while( strncmp( line, "Activity:\n", strlen( "Activity:\n" ) ) != 0 )
        {
            // copy next line of file to buffer
            strncpy( &buffer[ buffer_index ], line, strlen( line ) );
            buffer_index += strlen( line );

            // erase old data and copy in the next line
            memset( line, 0, line_max );
            fgets( line, line_max, log_file );
        }


        // navigate to the end of the activity log
        // if not EOF, will be a line with just \n-- this should be added after writing the new data
        while( strncmp( line, "\n", 1 ) != 0 && feof( log_file ) == 0 )
        {
            // copy next line to buffer
            strncpy( &buffer[ buffer_index ], line, strlen( line ) );
            buffer_index += strlen( line );

            // erase old line and get the next one
            memset( line, 0, line_max );
            fgets( line, line_max, log_file );
        }


        // add to the client's activity log

        // first add a line separating this session from past ones
        strncpy( &buffer[ buffer_index ], "\t\n", 2 );
        buffer_index += 2;

        for ( i = 0; i < log_len; ++i )
        {
            // copy each message to the buffer
            size_t msg_len = strlen( client_activity[i] );

            strncpy( &buffer[ buffer_index ], client_activity[i], msg_len );
            buffer_index += msg_len;
        }

        // add the \n at the new end of the log if not at the end of the file
        if ( feof( log_file ) == 0 )
        {
            buffer[ buffer_index ] = '\n';
            ++buffer_index;
        }


        // read the rest of the file into the buffer
        size_t remaining_bytes = file_size.st_size - buffer_index;
        fread( &buffer[ buffer_index ], 1, remaining_bytes, log_file );

        buffer[ buff_size - 1 ] = '\0';


        // rewrite everything to the log file and close it
        log_file = freopen( 0, "w", log_file );
        fputs( buffer, log_file );
        fclose( log_file );
    }


    // indicate that the log file is free and return
    log_file_open = false;
}
