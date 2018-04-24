#include <pthread.h>
#include <unistd.h>
#include "Project4Header.h"


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



/* Sends the given client a list
 * of .mp3 files in the server's
 * directory.
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


int main( int argc, char* argv[] )
{
    unsigned short port;

    // parse arguments to get port
    if ( argc != 5 )
    {
        fprintf( stderr, "Usage: ./Project4Server -p [port] -l [log file name]\n" );
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
                    fprintf( stderr, "Usage: ./Project4Server -p [port] -l [log file name]\n" );
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
        struct thread_args* args = malloc( sizeof( struct thread_args ) );
        args->client_sock = client;

        // get client IP address and copy into thread_args
        char* ip_addr = inet_ntoa( client_addr.sin_addr );
        strncpy( args->client_ip, ip_addr, 15 ); // IP address is 15 characters long
        ip_addr[ 15 ] = '\0'; // terminate string

        // create thread for client
        pthread_t threadID;


        if ( pthread_create( &threadID, NULL, threadMain, (void*) &args ) != 0 )
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
    if((send( client_sock, packet, packet_len, 0)) != packet_len )
    {
        fprintf( stderr, "send() failed\n" );
        close( client_sock );
        exit(1);
    }
}


char** send_files( int client_sock, int length )
{
    // get file names
    struct file_name files[ length ];

    // create return array of strings
    char** added_files = (char**) malloc( length * sizeof( char* ) );


    // may need to make multiple calls to recv to get all bytes of packet
    size_t bytes_received = 0;
    size_t bytes_expected = length * sizeof( struct file_name );

    while( bytes_received < bytes_expected )
    {
        ssize_t new_bytes;

        // print error and exit if recv fails
        if ( ( new_bytes = recv( client_sock, &files[ bytes_received ], bytes_expected - bytes_received, 0 ) ) < 0 )
        {
            fprintf( stderr, "recv() failed\n" );
            close( client_sock );
            exit(1);
        }

        bytes_received += new_bytes;
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
        size_t options_len = strlen( "' -printf '%s'\n" );

        size_t len = command_len + name_len + options_len;

        // copy command and filename for call
        char command[ len + 1 ];

        strncpy( command, "find -iname '", command_len );
        strncpy( &command[command_len], files[file_found_count].filename, name_len );
        strncpy( &command[command_len + name_len], "' -printf '%s'\n", options_len );

        command[len] = '\0';

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


    return added_files;
}


void* threadMain( void* thread_arg )
{
    // get client socket and IP address from arg structure
    struct thread_args* args = ( struct thread_args* ) thread_arg;
    int client_sock = args->client_sock;
    char* ip_addr = args->client_ip;

    pthread_t threadID = pthread_self();


    // variables that keep info on client activity
    // start with 100 strings each, expand later if necessary
    int files_cap = 100;
    int log_cap = 100;

    char** client_files = malloc( files_cap * sizeof( char* ) );  // list of files sent to/received from client
    char** activity_log = malloc( log_cap * sizeof( char* ) );  // list of messages received from client

    size_t num_files = 0;  // number of files sent to/received from client
    size_t num_messages = 0;  // total number of messages received and logged


    // get time of connection beginning
    time_t t;
    struct tm* time_struct;

    time( &t );
    time_struct = localtime( &t );

    char* time_str = asctime( time_struct );


    // add information for client connection
    size_t start_len = strlen( time_str ) + strlen( "\tConnection " ) + 2;
    client_files[0] = malloc( start_len );

    strncpy( client_files[0], "\tConnection ", strlen( "\tConnection " ) );
    client_files[0][ start_len - 2 ] = '\n';
    client_files[0][ start_len - 1 ] = '\0';

    ++num_messages;


    // deallocates thread resources
    pthread_detach( threadID );


    // pointer to any new info about client files from messages
    char** new_files = NULL;


    // infinite loop waiting for client message
    while ( true )
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

            char* log_msg = "\tReceived files from client:\n";

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

            char* log_msg = "\tSent files to client:\n";

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

            size_t initial_len = strlen( "Connection closed " );
            size_t log_msg_len = initial_len + strlen( time_str ) + 2; // account for newline and null terminator

            activity_log[ num_messages ] = (char*) malloc( log_msg_len );

            // copy first part of message
            strncpy( activity_log[ num_messages ], "\tConnection closed ", initial_len );

            // copy time
            strncpy( &activity_log[ num_messages ][ initial_len ], time_str, strlen( time_str ) );

            activity_log[ num_messages ][ log_msg_len - 2 ] = '\n';
            activity_log[ num_messages ][ log_msg_len - 1 ] = '\0';


            // add the new message to the count
            ++num_messages;

            // exit loop to write log file and end
            break;
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
                strncpy( new_files[i], client_files[ num_files ], file_name_len );

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


    write_log_file( client_files, num_files, activity_log, num_messages, ip_addr );


    free( thread_arg );

    return NULL;
}


char** write_files( int client_sock, int length )
{
    // array of file names to return
    char** added_files = (char**) malloc( sizeof(char*) * length );

    // get files from client
    int i;
    for ( i = 0; i < length; ++i )
    {
        struct push_file prefix;
        // read in file prefix with name and size
    
        if (recv( client_sock, &prefix, sizeof( struct push_file ), 0 ) < 0 )
        {
            fprintf( stderr, "recv() failed\n" );
            close( client_sock );
            exit(1);
        }

        printf("Saving file: %s...", prefix.name);
 
        // read in contents of file

        size_t bytes_received = 0;
        size_t bytes_expected = prefix.size;

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


        // need to close file to reopen to write
        fclose( open_file );


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
    size_t line_len = strlen( "Client: " ) + 15 + 2;

    char begin_str[ line_len ];
    strncpy( begin_str, "Client: ", strlen( "Client: " ) );
    strncpy( &begin_str[ strlen( "Client: " ) ], client_ip, 15 );
    begin_str[ line_len - 2 ] = '\n';
    begin_str[ line_len - 1 ] = '\0';



    FILE* log_file = fopen( log_file_name, "a+" );

    // search file for beginning of client's information
    boolean found = false;
    fpos_t start_pos;

    // a line will be an absolute maximum of 258 chars -- at most filename + two tabs and newline
    int line_max = 258;

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
            fgetpos( log_file, &start_pos );
        }
    }


    // if the client was not found in the file, append a new entry to the end of the file
    if ( !found )
    {
        // move to the end of the file
        fseek( log_file, SEEK_END, SEEK_SET );

        // add entry for this client
        fprintf( log_file, "%s", begin_str );

        // add entry for this client's files
        fprintf( log_file, "Files:\n" );


        // add each file from the list
        int i;
        for ( i = 0; i < num_files; ++i )
        {
            // write file name to list and free memory
            fputs( client_files[i], log_file );

            free( client_files[i] );
        }


        // add entry for this client's activity
        fprintf( log_file, "\nActivity:\n" );

        // add client activity from the last session
        for ( i = 0; i < log_len; ++i )
        {
            // write each message to the activity list
            fputs( client_activity[i], log_file );

            free( client_activity[i] );
        }

        // add two extra \n\n chars to separate client entries
        fprintf( log_file, "\n\n" );
    }
    else
    {
        // find position to add files
        // the line "Files:\n" marks the beginning of the list
        // and should come right after the line specifying client name (so no searching required)
        char line[ line_max ];
        memset( line, 0, line_max );

        fgets( line, line_max, log_file );

        // append files to the beginning of the list (for simplicity's sake)
        int i;
        for ( i = 0; i < num_files; ++i )
        {
            // copy each filename to the file
            fputs( client_files[i], log_file );

            free( client_files[i] );
        }



        // find position to add activity
        // the line "Activity:\n" marks the beginning of the client activity log
        while( strncmp( line, "Activity:\n", strlen( "Activity:\n" ) ) != 0 )
        {
            memset( line, 0, line_max );  // erase old data before writing new line
            fgets( line, line_max, log_file );
        }


        // after the beginning of the activity log has been found
        // need to seek until finding a line that is just \n,
        // which indicates the end of the existing list

        // keep track of the previous position to seek back there once the end of the log is found
        fpos_t prev_pos;
        fgetpos( log_file, &prev_pos );

        memset( line, 0, line_max );  // erase old data before writing new line
        fgets( line, line_max, log_file );

        while( strncmp( line, "\n", 1 ) != 0 )
        {
            fgetpos( log_file, &prev_pos );

            memset( line, 0, line_max );  // erase old data before writing new line
            fgets( line, line_max, log_file );
        }

        // append activity

        // reset position to just before log end
        fsetpos( log_file, &prev_pos );

        // add client message strings to log
        for ( i = 0; i < log_len; ++i )
        {
            // write each message to the activity log
            fputs( client_activity[i], log_file );

            free( client_activity[i] );
        }

    }


    // close log file, indicate that it is free, free memory, and return
    fclose( log_file );
    log_file_open = false;

    free( client_activity );
    free( client_files );
}