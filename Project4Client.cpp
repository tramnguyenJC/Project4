#include <unordered_map>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include <iostream>
using namespace std;


#include "Project4Header.h"

//////////////////////////////////////////////////////////////////////////////
// EXAMPLE TO RUN THIS FILE: ./Project4Client -d ./MusicFiles -s mathcs02:31100


//////////////////////////////////////////////////////////////////////////////
// @brief:Take name of host and return its internet address
// @param name the name of the host
// @return the host's internet address
unsigned long ResolveName(const char name[]);

//////////////////////////////////////////////////////////////////////////////
// @brief: Print out a list of valid instructions that the user can enter
void printInstructions();

//////////////////////////////////////////////////////////////////////////////
// @brief: Requests a list of files on the server and prints this for the user,
// with a note for files that have the same content as files in musicDir.
void list(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Displays a list of files that are on the server but not the client
// and files that are on the client but not the server.
void diff(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Sync all files on the client and server side -- sends to the server
// all files on the client that it doesn't have and retrieves from the server
// all files that were included in the LIST that weren't found in musicDir.
void syncFiles(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Sends BYE message to server and terminate the connection.
void leave(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: This function retrieves all the current files on the client side
// and returns a hashmap of  <hash string, vector of file names with same content>
// Choose structure hashmap for easier look up (O(1) instead of O(n))
std::unordered_map<std::string, std::vector<std::string>> getFilesOnClient();

//////////////////////////////////////////////////////////////////////////////
// @brief: Retrieves all file names currently on server
// @return: a char buffer of size[sizeof(file_name)*file_counts + 4], with the 
// first 4 bytes as the number of file names returned.
unsigned char* getFilenamesOnServer(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Construct a "PUSH" message with a list of struct push_file (along with
// the byte content of each file) for files that are on client but not on server
void sendFiles(int sock, std::vector<std::string> filesToSend);

//////////////////////////////////////////////////////////////////////////////
// @brief: Construct a "PULL" message with a list of struct pull_file (along with
// the byte content of each file) for files that are on server but not on client
void getFiles(int sock, std::vector<std::string> filesToRequest);

char *musicDir;									//< directory path that contains music files

int main (int argc, char *argv[]) {

  const char *serverHost = SERVER_HOST;   //< Default server host
  unsigned short serverPort = atoi(SERVER_PORT); // Default server port
  char *serverPortString;                 //< string representing server port
  struct sockaddr_in serverAddr;          //< Echo server address 
 

  // Test for correct number of arguments
  if (argc != 5 && argc != 3) {
    printf("Error: Usage ./Project4Client -d <path to music files> [-s <server IP>[:<port>]] \n");
    exit(1);
  }

  if ( argv[1][0] == '-' && argv[1][1] == 'd' ) {
        musicDir = argv[ 2 ];
  }
  else {
  	fprintf( stderr, "Error: Usage ./Project4Client [-s <server IP>[:<port>]] -d <path to music files>\n" );
      exit(1);
  }
  
  // If user specifies IP Address/name and port number
  if(argc == 5){
    if ( argv[3][0] == '-' && argv[3][1] == 's' ){
      serverHost = strtok(argv[4],":");
      if ((serverPortString = strtok(NULL, ":")) != NULL) {
        serverPort = atoi(serverPortString);
      }
    }
    else{
      fprintf( stderr, "Error: Usage ./Project4Client [-s <server IP>[:<port>]] -d <path to music files>\n" );
      exit(1);
    }
  }

  // Create a stream socket using TCP 
  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(sock < 0){
    fprintf( stderr, "creation of socket failed\n" );
    exit(1);
  }

  // Construct the server address structure 
  memset(&serverAddr, 0, sizeof(serverAddr));           // Zero out structure 
  serverAddr.sin_family = AF_INET;                      // Internet address family
  serverAddr.sin_addr.s_addr = ResolveName(serverHost); // Get host
  serverAddr.sin_port = htons(serverPort);              // Server port 
  
  //Establish the connection to the echo server 
  if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
    fprintf( stderr, "connect() failed\n" );
    exit(1);
  }

  printf("Welcome Frosty! Please select any functions below:\n");
  printInstructions();

  for(;;){
    // allocates space for the std::string user enters
    const int size = 100;
    char input[size];

    // reads in user input and calls appropriate functions
    fgets(input, size, stdin);

    if(strncmp(input, "list", 4) == 0) {
      list(sock);
    }
    else if(strncmp(input, "diff", 4) == 0){
      diff(sock);
    }
    else if(strncmp(input, "sync", 4) == 0){
      syncFiles(sock);
    }
    else if(strncmp(input, "bye!", 4) == 0){
      leave(sock);
      break;
    }
    else {
      printf("You entered an invalid command. Here are the instructions:\n");
      printInstructions();
    }
  }
}


//////////////////////////////////////////////////////////////////////////////
// @brief:Take name of host and return its internet address
// @param name the name of the host
// @return the host's internet address
unsigned long ResolveName(const char name[]) {
  struct hostent *host; // Structure containing host information
  if((host = gethostbyname(name)) == NULL){
    fprintf(stderr, "gethostbyname() failed");
    exit(1);
  }
  // Return the binary, network-byte-ordered address
  return *((unsigned long *) host->h_addr_list[0]);
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Retrieves all file names currently on server
// @return: a char buffer of size[sizeof(file_name)*file_counts + 4], with the 
// first 4 bytes as the number of file names returned.
unsigned char* getFilenamesOnServer(int sock){

  // construct header
	struct header request_header;
  request_header.length = 0;
  const char* msg_type = "LIST";
  memcpy( &(request_header.type), msg_type, 4 );

  // Send a request with no data, only header
  int header_len = sizeof(struct header);
  unsigned char packet[ header_len ];
  memcpy( packet, &request_header, header_len);

  if (send(sock, packet, header_len, 0) != header_len){
    fprintf( stderr, "send() failed. \n" );
    close(sock);
    exit(1);
  }

  // Receives the header from the server and stores into a buffer
  unsigned char headerBuffer[header_len];
  if((recv(sock, headerBuffer, header_len, 0)) <= 0) {
    printf("recv() failed or connection closed prematurely.\n");
    exit(1);
  }

  // Retrieve the number of file names received from the server
  struct header response_header;
  memcpy( &response_header, headerBuffer, header_len);
  int num_files = response_header.length;

  // Continue receiving data from the server ad store data into dataBuffer
  size_t data_len = sizeof( struct file_name ) * num_files;
  unsigned char* dataBuffer = (unsigned char*) malloc(data_len + 4);

  memset(dataBuffer, 0, data_len + 4);
  memcpy(dataBuffer, &num_files, 4);

  int bytes_expected = data_len;
  int bytes_received = 0;

  if(num_files != 0){

    // make calls to recv until all bytes have been read in
  	while (bytes_received < bytes_expected){
  		int new_bytes = recv(sock, &dataBuffer[bytes_received + 4], bytes_expected - bytes_received, 0);

	  	if(new_bytes < 0) {
	  		printf("recv() failed or connection closed prematurely.\n");
    		exit(1);
  		}

  		bytes_received += new_bytes;
  	}
  }
  
  return dataBuffer;
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Print out a list of valid instructions that the user can enter
void printInstructions(){
  printf("Enter \'list\' to list all the files the server has.\n");
  printf("Enter \'diff\' to show a \"diff\" of the files you have in comparison to the server \n");
  printf("Enter \'sync\' to sync to your device files on the server and vice versa.\n");
  printf("Enter \'bye!\' to leave the program.\n");
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Requests a list of files on the server and prints this for the user,
// with a note for files that have the same content as files in musicDir.
void list(int sock){

  // get list of files from server
	unsigned char* dataBuffer = getFilenamesOnServer(sock);

  // get number of included files from header
	int num_files;
	memcpy(&num_files, dataBuffer, 4);

  // copy file names
	struct file_name* files = (struct file_name*) malloc( sizeof(file_name)*num_files );
	memcpy(&files, &dataBuffer[4], sizeof(file_name)*num_files);
	
  
  // If the server sends no files
  if(num_files == 0){
    printf("Server has no file. \n");
    return;
  }
  
  // Retrieve a hashmap of <hash, filename> for files on the client side
  std::unordered_map<std::string, std::vector<std::string>> filesOnClient = getFilesOnClient();
	
	printf("\n");
  printf("+ List of files on server:\n");
  for(int i = 0; i < num_files; i++){

    // Store appropriate data into a file_name struct for each file name
    struct file_name file = files[i];
    
    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator search
     = filesOnClient.find(hashStr);

    // print file name, adding a note if it is a duplicate of a server file
    if(search == filesOnClient.end()) {
      printf("File: %s \n", file.filename);
    }
    else{
    	printf("File: %s (duplicate content with ", file.filename);
      for(auto& name : search->second)
      	printf(" %s ", name.c_str());
    	printf(")\n");
    }
  }

  printf("\n");

  free(dataBuffer);
  free(files);
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Displays a list of files that are on the server but not the client
// and files that are on the client but not the server.
void diff(int sock){

  // get list of files on server -- names and hashes of files
  unsigned char* dataBuffer = getFilenamesOnServer(sock);

  // get number of file names included from header
	int num_files;
	memcpy(&num_files, dataBuffer, 4);

  // copy file_name structs from packet
	struct file_name* files = (struct file_name*) malloc( sizeof(file_name)*num_files );
	memcpy(&files, &dataBuffer[4], sizeof(file_name)*num_files);

  free(dataBuffer);
  
  // Retrieve a hashmap of <hash, filename> for files on the client side
  std::unordered_map<std::string, std::vector<std::string>> filesOnClient = 
  	getFilesOnClient();

  // keeps track of the hashes of each duplicate file
	std::vector<std::string> sameHash;


  // files on the server but not on the client
  vector<string> diffFiles;

  for(int i = 0; i < num_files; i++){

    struct file_name file = files[i];

    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly

    std::string hashStr(file.hash);
    std::unordered_map<std::string,std::vector<std::string>>::const_iterator search = 
    	filesOnClient.find(hashStr);

    if(search == filesOnClient.end()) {
      // add file to list of files on the server
      string filename_str( file.filename );
      diffFiles.push_back( filename_str );
    }
    else {

      // remove leading ./ from directory path
      string relativePath( &musicDir[2] );

      // get file name without leading directory -- name and /
      int fileNameLen = search->second[0].length() - relativePath.length();
      string nameWithoutDir = search->second[0].substr( relativePath.length() + 1, fileNameLen );


      // if the contents match, but the names don't,
      // ask if the user wants to rename the local file
      // to be consistent with the server's file name
      if ( strncmp( file.filename, nameWithoutDir.c_str(), nameWithoutDir.length() ) != 0 )
      {
        cout << endl << "The following file(s) have the same content "
             << "as " << file.filename << " on the server:" << endl;
        for ( size_t i = 0; i < search->second.size(); ++i )
        {
          cout << "\t" << search->second[i] << endl;
        }

        cout << "Would you like to rename the local file(s) to match the server files? "
             << "(if there are multiple files, all but one will be deleted.)" 
             << " Please enter 'y' to rename and merge files, and 'n' to keep "
             << "duplicate files. \t";

        // get user response
        char response[2];
        cin.getline( response, 2 );

        if ( response[0] == 'y' || response[0] == 'Y' )
        {
           // rename all matching files to the server's filename
           for ( size_t i = 0; i < search->second.size(); ++i )
           {
             // prepend the client's directory path to the server's name
             string name_w_path( musicDir );
             name_w_path += "/";
             name_w_path += file.filename;

             rename( search->second[i].c_str(), name_w_path.c_str() );
           }
        }
        else
        {
          cout << endl << "Files were not renamed." << endl;
        }
      }

      // add hash of the file to the list
    	sameHash.push_back(search->first);
    }
  }


  // print names of files on the server that the client doesn't have, if any

  printf("\n\n");
  printf("+ List of files on server the client doesn't have:\n");
  
  if(sameHash.size() == (unsigned int)num_files) {
  	printf("No such file found.\n");
  }
  else {

    for ( size_t i = 0; i < diffFiles.size(); ++i )
    {
      cout << endl << "File: " << diffFiles[i] << endl;
    }
  }
  
  // print file names on client but not server
  printf("\n");
  printf("+ List of files on client that server does not have:\n");
  int numFilesClientDiff = 0;
  for(auto& pair : filesOnClient){

  	if(std::find(sameHash.begin(), sameHash.end(), pair.first) == sameHash.end()){

  		for(auto& name : pair.second){
      	printf("File: %s \n", name.c_str());
  			numFilesClientDiff++;
  		}
  	}
  }

  if(numFilesClientDiff == 0) {
  	printf("No such file found.\n");
  }

  printf("\n");
  free(files);
}

std::unordered_map<std::string, std::vector<std::string>> getFilesOnClient(){
  std::unordered_map<std::string, std::vector<std::string>> hashToName;

  // use ls to get all music files in current directory
  char command[strlen("find  -maxdepth 1 -iname '*.mp3'") + strlen(musicDir)];
  sprintf(command, "find %s -maxdepth 1 -iname '*.mp3'", musicDir);
  FILE* pipe = popen(command, "r" );
  // get first filename
  char file[FILE_NAME_MAX];
  char* success = fgets( file, FILE_NAME_MAX, pipe );

  while( success != 0 )
  {
    char filename[FILE_NAME_MAX];
    
    // copy all but leading "./" and ending newline character into std::string
    strncpy(filename, &file[2], strlen( file ) - 3);
    filename[strlen( file ) - 3] = '\0';
    std::string filenameStr(filename);
    // compute hash of file
    char* hashResult = computeHash(filename);
    std::string hash(hashResult);
    free(hashResult);

    // Add both to the hashmap
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator it = 
    	hashToName.find(hash);
    if(it == hashToName.end()){
    	std::vector<std::string> vec;
    	vec.push_back(filenameStr);
    	hashToName.insert(make_pair(hash,vec));
    }
    else {
    	std::vector<std::string> vec = it->second;
    	vec.push_back(filenameStr);
    	hashToName[hash] = vec;
    }
    // get next file name
    success = fgets( file, FILE_NAME_MAX, pipe );
  }
  pclose( pipe );
  return(hashToName);
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Sync all files on the client and server side -- sends to the server
// all files on the client that it doesn't have and retrieves from the server
// all files that were included in the LIST that weren't found in musicDir.
void syncFiles(int sock){

	unsigned char* dataBuffer = getFilenamesOnServer(sock);
	int num_files;
	memcpy(&num_files, dataBuffer, 4);
	struct file_name files[sizeof(file_name)*num_files];
	memcpy(&files, &dataBuffer[4], sizeof(file_name)*num_files);
  free(dataBuffer);
  
  // Retrieve a hashmap of <hash, filename> for files on the client side
  std::unordered_map<std::string, std::vector<std::string>> filesOnClient = getFilesOnClient();
  std::vector<std::string> sameHash;
  std::vector<std::string> filesToSend;
  std::vector<std::string> filesToRequest;


  for(int i = 0; i < num_files; i++){
    struct file_name file = files[i];
    
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator search =
    filesOnClient.find(hashStr);
    if(search == filesOnClient.end()) {
    	string filenameStr(file.filename);
    	filesToRequest.push_back(filenameStr);
    }
    else {
      // remove leading ./ from directory path
      string relativePath( &musicDir[2] );

      // get file name without leading directory -- name and /
      int fileNameLen = search->second[0].length() - relativePath.length();
      string nameWithoutDir = search->second[0].substr( relativePath.length() + 1, fileNameLen );


      // if the contents match, but the names don't,
      // ask if the user wants to rename the local file
      // to be consistent with the server's file name
      if ( strncmp( file.filename, nameWithoutDir.c_str(), nameWithoutDir.length() ) != 0 )
      {
        cout << endl << "The following file(s) have the same content "
        << "as " << file.filename << " on the server:" << endl;
        for ( size_t i = 0; i < search->second.size(); ++i )
        {
          cout << "\t" << search->second[i] << endl;
        }

        cout << "Would you like to rename the local file(s) to match the server files? "
        << "(if there are multiple files, all but one will be deleted.)" 
        << " Please enter 'y' to rename and merge files, and 'n' to keep "
        << "duplicate files. \t";

        // get user response
        char response[2];
        cin.getline( response, 2 );

        if ( response[0] == 'y' || response[0] == 'Y' )
        {
           // rename all matching files to the server's filename
         for ( size_t i = 0; i < search->second.size(); ++i )
         {
             // prepend the client's directory path to the server's name
           string name_w_path( musicDir );
           name_w_path += "/";
           name_w_path += file.filename;

           rename( search->second[i].c_str(), name_w_path.c_str() );
         }
       }
       else
       {
        cout << endl << "Files were not renamed." << endl;
      }
    }


    sameHash.push_back(search->first);
  }
}

  printf("\n");
  printf("+ List of files server needs to send to client: \n");

  if(sameHash.size() == (unsigned int)num_files) {
   printf("No such file found.\n");
  } else {

    for ( size_t i = 0; i < filesToRequest.size(); ++i )
    {
      printf( "File: %s", filesToRequest[i].c_str() );
    }

    getFiles(sock, filesToRequest);
    printf("\nComplete sending files to the client.\n");
  }
  printf("\n");


  printf("\n");
  printf("+ List of files client needs to send to server:\n");

  int numFilesClientDiff = 0;

  for(auto& pair : filesOnClient){
   if(std::find(sameHash.begin(), sameHash.end(), pair.first) == sameHash.end()){
    for(auto& name : pair.second){
     printf("File %s \n", name.c_str());
     filesToSend.push_back(name);
   }
   numFilesClientDiff++;
  }

  }
  if(numFilesClientDiff == 0) {
   printf("No such file found.\n");
  }

  else {
   sendFiles(sock, filesToSend);
   printf("\nComplete sending files to the server.\n");
  }

  printf("\n");
}


//////////////////////////////////////////////////////////////////////////////
// @brief: Construct a "PULL" message with a list of struct pull_file (along with
// the byte content of each file) for files that are on server but not on client
void getFiles(int sock, std::vector<std::string> filesToRequest) {
  int length = filesToRequest.size();
  if(length == 0) {
    return;
  }
  // struct for each file

  size_t header_len = sizeof(struct header);
  size_t data_len = length*(sizeof(struct file_name));
  int packet_size = header_len + data_len;

  unsigned char* packet = (unsigned char*)malloc(packet_size);
  memset( packet, 0, packet_size );
  
  // create header and copy to message
  struct header pull_header;
  memcpy( pull_header.type, "PULL", 4 );
  
  pull_header.length = length;
  memcpy( packet, &pull_header, sizeof( struct header ) );

  int current_index = sizeof( struct header );


  for(int i = 0; i < length; i++) {
    memcpy( &packet[ current_index ], filesToRequest[i].c_str(), filesToRequest[i].length() );
    current_index += sizeof( struct file_name );
  }

  // send message to server
  if(send(sock, packet, packet_size, 0 ) != packet_size ){
    fprintf( stderr, "send() failed\n" );
    close(sock);
    exit(1);
  }

  free(packet);

  //size_t header_len = sizeof( struct header );
  unsigned char buffer[ header_len ];

  // first just read in message header to check message type
  size_t bytes_received = 0;
  size_t bytes_expected = sizeof( struct header );

  // make sure all bytes of header get read in correctly
  while( bytes_received < bytes_expected )
  {
    // read in up to the full header
    int new_bytes = recv( sock, buffer, bytes_expected - bytes_received, 0 );

    // if there's an error, print and exit
    if ( new_bytes < 0 )
    {
      fprintf( stderr, "recv() failed: %s\n", strerror( errno ) );
      close( sock );
      exit(1);
    }

    // update received count based on the last call
    bytes_received += new_bytes;
  }


  // copy header into struct for parsing
  struct header respond_header;
  memcpy( &respond_header, buffer, header_len );

  int i;
  length = respond_header.length;


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
      ssize_t new_bytes = recv( sock, &buffer[ bytes_received ], bytes_expected - bytes_received, 0 );

      if ( new_bytes < 0 )
      {
        fprintf( stderr, "recv() failed\n" );
        close( sock );
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

    unsigned char* file = (unsigned char*) malloc(prefix.size);

    // read in bytes until the entire file has been received
    while ( bytes_received < bytes_expected )
    {
      // read in more data, up to the amount remaining in the file
      ssize_t new_bytes = recv( sock, &file[ bytes_received ], bytes_expected - bytes_received, 0 );

      // the loop will continue until all bytes of the file have been read in (but no more)
      bytes_received += new_bytes;

    }

    // create file using filename
    // adding .part suffix at first
    // to avoid including incomplete files in a LIST request

    size_t file_name_len = strlen(prefix.name) + strlen(musicDir) - 1;
    char nameWithDir[ file_name_len + 6];
    const char slash[1] = {'/'};

    memcpy(nameWithDir, &musicDir[2], strlen(musicDir)-2);
    memcpy(&nameWithDir[strlen(musicDir)-2], slash, strlen(slash));
    memcpy(&nameWithDir[strlen(musicDir)-1], prefix.name, strlen(prefix.name));

    strncpy( &nameWithDir[file_name_len], ".part", 5);
    nameWithDir[file_name_len + 5] = '\0';

    // create file
    FILE* new_file = fopen( nameWithDir, "w" );

    // write bytes to file
    fwrite( file, sizeof(char), prefix.size, new_file );
    fclose( new_file );

    char newName[file_name_len + 1];
    strncpy(newName, nameWithDir, file_name_len);
    newName[file_name_len] = '\0';
    
    // rename to remove .part once the entire file is written
    rename( nameWithDir, newName);

    printf( "complete\n" );

    free(file);
  }

  // prints a fun message
  cout << "\n" << "                 ________________\n                |                |_____    __\n                | Files arrived! |     |__|  |_________\n                |________________|     |::|  |        /\n   /\\**/\\       |                \\.____|::|__|      <\n  ( o_o  )_     |                      \\::/  \\._______\\\n   (u--u   \\_)  |\n    (||___   )==\\\n  ,dP\"/b/=( /P\"/b\\\n  |8 || 8\\=== || 8\n  `b,  ,P  `b,  ,P\n    \"\"\"`     \"\"\"`" << endl;

}


//////////////////////////////////////////////////////////////////////////////
// @brief: Construct a "PUSH" message with a list of struct push_file (along with
// the byte content of each file) for files that are on client but not on server
void sendFiles(int sock, std::vector<std::string> filesToSend){
	int length = filesToSend.size();
  if(length == 0){
    return;
  }
	// struct for each file
  struct push_file* file_sizes = (struct push_file*) malloc( length * sizeof( struct push_file) );

  // keeps track of total size of packet
  int packet_size = sizeof( struct header );
	// get the size of each requested file that is found
  int file_found_count = 0;

  int i;
  for ( i = 0; i < length; ++i )
  {	
    // gets size of file (in bytes)
    struct stat file_size;
    int success = stat( filesToSend[i].c_str(), &file_size );

    unsigned int size = file_size.st_size;

    // strip path to get just file name
    size_t dir_len = strlen(musicDir) - 1;
    size_t name_len = filesToSend[i].length() - dir_len;
    char nameWithoutDir[name_len + 1];
    strncpy(nameWithoutDir, &(filesToSend[i].c_str()[dir_len]), name_len);
    nameWithoutDir[name_len] = '\0';
		
    // stat() returns 0 to to signal it successfully found the file
    if ( success == 0 ){
      // copy file name (without path) and size
      memset( &file_sizes[file_found_count], 0, sizeof( struct push_file ) );
      strncpy(file_sizes[file_found_count].name, nameWithoutDir, name_len);
    	file_sizes[file_found_count].name[ name_len ] = '\0';
      file_sizes[ file_found_count ].size = size;
  
      // update packet size to account for this file
      packet_size += sizeof( struct push_file );
      packet_size += file_sizes[file_found_count].size;

      ++file_found_count;
    }
  }
  
  // construct response message
  unsigned char* packet = (unsigned char*) malloc(packet_size);
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
    // Add back the path to be able to open the file

    char nameWithDir[strlen(file_sizes[i].name) + strlen(musicDir)];
    const char slash[1] = {'/'};
    memcpy(nameWithDir, &musicDir[2], strlen(musicDir)-2);
    memcpy(&nameWithDir[strlen(musicDir)-2], slash, strlen(slash));
    memcpy(&nameWithDir[strlen(musicDir)-1], file_sizes[i].name, strlen(file_sizes[i].name));
    nameWithDir[strlen(file_sizes[i].name) + strlen(musicDir)-1] = '\0';
  	
    FILE* file = fopen( nameWithDir, "r" );    
		unsigned char* buffer = (unsigned char*) malloc(file_sizes[i].size);
		fread( buffer, 1, file_sizes[i].size, file );
		fclose( file );
		
		// copy contents of file to message
    memcpy( &packet[ current_index ], buffer, file_sizes[i].size );
		current_index += file_sizes[i].size;
		free(buffer);
	}

	// send message to server
  if(send(sock, packet, packet_size, 0 ) != packet_size ){
    fprintf( stderr, "send() failed\n" );
    close(sock);
    exit(1);
  }

  free(packet);
  free( file_sizes );

  // prints a fun message
  cout << "\n" << "       ___\n      (  /\n      / |\n     /  \\ \n    |   \\\\ \n    |   |\\\\            //\\\\    /\\_     //\\     /\\_ \n     \\\\\\\\ |           // _____/  _/   // _____/  _/\n      \\\\\\\\|____       |     |   /     |     |   /\n      ////|    | ==== ( ==__|==/ ==== ( ==__|==/\n     //// |    |       \\ \\   \\ \\       \\ \\   \\ \\ \n=================/     / /    \\ \\      / /    \\ \\ \n" << endl;
  cout << "Delivering your files!" << endl;

}


//////////////////////////////////////////////////////////////////////////////
// @brief: Sends BYE message to server and terminate the connection.
void leave(int sock){

  // create BYE! message to send to server
  struct header bye_header;
  strncpy( bye_header.type, "BYE!", 4 );
  bye_header.length = 0;

  // create message
  unsigned char buffer[ sizeof( struct header ) ];
  memcpy( buffer, &bye_header, sizeof( struct header ) );

  // send message to server and end connection
  send( sock, buffer, sizeof( struct header ), 0 );

  close(sock); 
}

