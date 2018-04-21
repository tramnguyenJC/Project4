
#include "Project4Header.h"


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
// and returns a hashmap of  <hash string, file name>
// Choose structure hashmap for easier look up (O(1) instead of O(n))
std::unordered_map<std::string, std::string> getFilesOnClient();

//////////////////////////////////////////////////////////////////////////////
// @brief: Retrieves all file names currently on server
// @return: a char buffer of size[sizeof(file_name)*file_counts + 4], with the 
// first 4 bytes as the number of file names returned.
unsigned char* getFilenamesOnServer(int sock);

char *musicDir;													//< directory path that contains music files

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

	if ( argv[1][0] == '-' && argv[1][1] == 'd' )
    musicDir = argv[2];	
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
    if(strncmp(input, "list", 4) == 0)
      list(sock);
    else if(strncmp(input, "diff", 4) == 0)
      diff(sock);
    else if(strncmp(input, "sync", 4) == 0)
      syncFiles(sock);
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
  if((recv(sock, &dataBuffer[4], data_len, 0)) <= 0) {
    printf("recv() failed or connection closed prematurely.\n");
    exit(1);
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
  std::unordered_map<std::string, std::string> filesOnClient = getFilesOnClient();

  printf("List of files on server:\n");
  for(int i = 0; i < num_files; i++){

    // Store appropriate data into a file_name struct for each file name
    struct file_name file = files[i];
    
    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::string>::const_iterator search = filesOnClient.find(hashStr);
    if(search == filesOnClient.end())
      printf("File: %s \n", file.filename);
    else
      printf("File: %s (duplicate content with %s) \n", file.filename, search->second.c_str());
  }
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
  std::unordered_map<std::string, std::string> filesOnClient = getFilesOnClient();
	std::vector<std::string> sameHash;
  printf("List of files on server the client doesn't have:\n");
  for(int i = 0; i < num_files; i++){
    struct file_name file = files[i];

    // Look up if the client has a file with the same hash std::string (same file content)
    // and print information accordingly
    std::string hashStr(file.hash);
    std::unordered_map<std::string, std::string>::const_iterator search = filesOnClient.find(hashStr);
    if(search == filesOnClient.end())
      printf("File: %s \n", file.filename);
    else
    	sameHash.push_back(search->first);
  }
  
  printf("\n");
  printf("List of files on client that server does not have:\n");
  int numFilesClientDiff = 0;
  for(auto& pair : filesOnClient){
  	if(std::find(sameHash.begin(), sameHash.end(), pair.first) == sameHash.end()){
  		printf("File %s \n", pair.second.c_str());
  		numFilesClientDiff++;
  	}
  }
  if(numFilesClientDiff == 0)
  	printf("No such file found.\n");
}

std::unordered_map<std::string, std::string> getFilesOnClient(){
  std::unordered_map<std::string, std::string> hashToName;

  // use ls to get all music files in current directory
  char command[strlen("find  -maxdepth 1 -iname '*.mp3") + strlen(musicDir)];
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
    std::string filenameStr(filename);
    // compute hash of file
    char* hashResult = computeHash(filename);
    std::string hash(hashResult);
    free(hashResult);

    // Add both to the hashmap
    hashToName.insert(make_pair(hash, filenameStr));
    // get next file name
    success = fgets( file, FILE_NAME_MAX, pipe );
  }
  pclose( pipe );
  return(hashToName);
}

void syncFiles(int sock){
 
}

void leave(int sock){
  close(sock); 
}

