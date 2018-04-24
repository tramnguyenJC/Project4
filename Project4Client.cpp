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
// @brief: Print out instructions for user
void printInstructions();

//////////////////////////////////////////////////////////////////////////////
// @brief: Print out instructions for user
void list(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Show list of different files
void diff(int sock);

//////////////////////////////////////////////////////////////////////////////
// @brief: Sync all files on the client and server side
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
  if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0){
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
    if(strncmp(input, "list", 4) == 0){
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

unsigned long ResolveName(const char name[]) {
  struct hostent *host; // Structure containing host information
  if((host = gethostbyname(name)) == NULL){
    fprintf(stderr, "gethostbyname() failed");
    exit(1);
  }
  // Return the binary, network-byte-ordered address
  return *((unsigned long *) host->h_addr_list[0]);
}

unsigned char* getFilenamesOnServer(int sock){
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
  if(num_files != 0){
	  if((recv(sock, &dataBuffer[4], data_len, 0)) < 0) {
	  	printf("recv() failed or connection closed prematurely.\n");
    	exit(1);
  	}
  }
  
  return dataBuffer;
}

void printInstructions(){
  printf("Enter \'list\' to list all the files the server has.\n");
  printf("Enter \'diff\' to show a \"diff\" of the files you have in comparison to the server \n");
  printf("Enter \'sync\' to sync to your device files on the server and vice versa.\n");
  printf("Enter \'bye!\' to leave the program.\n");
}

void list(int sock){
	unsigned char* dataBuffer = getFilenamesOnServer(sock);
	int num_files;
	memcpy(&num_files, dataBuffer, 4);
	struct file_name files[sizeof(file_name)*num_files];
	memcpy(&files, &dataBuffer[4], sizeof(file_name)*num_files);

  // If the server sends no files
  if(num_files == 0){
    printf("Server has no file. \n");
    return;
  }
  
  // Retrieve a hashmap of <hash, filename> for files on the client side
  std::unordered_map<std::string, std::vector<std::string>> filesOnClient = getFilesOnClient();

  printf("+ List of files on server:\n");
  for(int i = 0; i < num_files; i++){

    // Store appropriate data into a file_name struct for each file name
    struct file_name file = files[i];
    
    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator search
     = filesOnClient.find(hashStr);
    if(search == filesOnClient.end())
      printf("File: %s \n", file.filename);
    else{
      for(auto& name : search->second)
      	printf("File: %s (duplicate content with %s) \n", file.filename, name.c_str());
    }
  }
  printf("\n");
  free(dataBuffer);
}

void diff(int sock){
  unsigned char* dataBuffer = getFilenamesOnServer(sock);
	int num_files;
	memcpy(&num_files, dataBuffer, 4);
	struct file_name files[sizeof(file_name)*num_files];
	memcpy(&files, &dataBuffer[4], sizeof(file_name)*num_files);
  free(dataBuffer);
  
  // Retrieve a hashmap of <hash, filename> for files on the client side
  std::unordered_map<std::string, std::vector<std::string>> filesOnClient = 
  	getFilesOnClient();
	std::vector<std::string> sameHash;
  printf("+ List of files on server the client doesn't have:\n");
  for(int i = 0; i < num_files; i++){
    struct file_name file = files[i];
    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly
    std::string hashStr(file.hash);
    std::unordered_map<std::string,std::vector<std::string>>::const_iterator search = 
    	filesOnClient.find(hashStr);
    if(search == filesOnClient.end())
      printf("File: %s \n", file.filename);
    else
    	sameHash.push_back(search->first);
  }
   if(sameHash.size() == (unsigned int)num_files)
  	printf("No such file found.\n");
  
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
  if(numFilesClientDiff == 0)
  	printf("No such file found.\n");
  printf("\n");
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
	
	printf("+ List of files server needs to send to client: \n");
  for(int i = 0; i < num_files; i++){
    struct file_name file = files[i];
    
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator search =
     filesOnClient.find(hashStr);
    if(search == filesOnClient.end()){
    	string filenameStr(file.filename);
    	filesToRequest.push_back(filenameStr);
    	printf("File %s \n", file.filename);
    }
    else
    	sameHash.push_back(search->first);
  }
  if(sameHash.size() == (unsigned int)num_files)
  	printf("No such file found.\n");
  
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
  if(numFilesClientDiff == 0)
  	printf("No such file found.\n");
  sendFiles(sock, filesToSend);
  printf("\nComplete sending files to the server.\n");
  printf("\n");
}

void sendFiles(int sock, std::vector<std::string> filesToSend){
	int length = filesToSend.size();
	// struct for each file
  struct push_file file_sizes[ length ];

  // keeps track of total size of packet
  int packet_size = sizeof( struct header );
	// get the size of each requested file that is found
  int file_found_count = 0;

  int i;
  for ( i = 0; i < length; ++i )
  {
    // get the size of this file
    // command: find -iname '$[filename]' -printf '%s\n'
    // [filename] must not contain slashes in the find command, therefore we
    // strip off the directory path string in the filename
    
    // Size of music directory string (after stripped off 2 chars './' at the front,
    // but add the '/' in between the directory and fileName
   
    size_t dir_len = strlen(musicDir) - 1;
		size_t name_len = filesToSend[i].length() - dir_len;
		char nameWithoutDir[name_len + 1];
		strncpy(nameWithoutDir, &(filesToSend[i].c_str()[dir_len]), name_len);
		nameWithoutDir[name_len] = '\0';
		
    size_t command_len = strlen( "find -iname '" );
    size_t options_len = strlen( " -printf '%s'\n" );
		size_t len = command_len + name_len + options_len;

    // copy command and filename for call
    char command[ len + 1 ];
		strncpy( command, "find -iname '", command_len );
    strncpy( &command[command_len], nameWithoutDir, name_len );
    strncpy( &command[name_len + command_len], "' -printf '%s'\n", options_len );
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
    if ( size != 0 ){
      // copy file name (include path directory) and size
      memset( file_sizes[file_found_count].name, 0, strlen(nameWithoutDir));
      strncpy(file_sizes[file_found_count].name, nameWithoutDir, name_len);
    	file_sizes[file_found_count].name[strlen(nameWithoutDir)] = '\0';
      file_sizes[ file_found_count ].size = size;
  
      // update packet size to account for this file
      packet_size += sizeof( struct push_file );
      packet_size += file_sizes[file_found_count].size;

      ++file_found_count;
    }
  }
  
  // construct response message
  unsigned char* packet = (unsigned char*)malloc(packet_size);
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
}

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

