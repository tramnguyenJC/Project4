
#include "Project4Header.h"


//////////////////////////////////////////////////////////////////////////////
// @brief:Take name of host and return its internet address
// @param name the name of the host
// @return the host's internet address
unsigned long ResolveName(char name[]);

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

int main (int argc, char *argv[]) {

  char *serverHost = SERVER_HOST;   //< Default server host
  unsigned short serverPort = atoi(SERVER_PORT); // Default server port
  char *serverPortString;           //< String representing server port
  struct sockaddr_in serverAddr;    //< Echo server address 
  //int bytesRcvd;                    //< Bytes read in single recv() and total byte reads

  // Test for correct number of arguments
  if (argc != 3 && argc != 1) {
    printf("Error: Usage ./Project4Client [-s <server IP>[:<port>]] \n");
    exit(1);
  }

  // If user specifies IP Address/name and port number
  if(argc == 3){
    if ( argv[1][0] == '-' && argv[1][1] == 's' ){
      serverHost = strtok(argv[2],":");
      if ((serverPortString = strtok(NULL, ":")) != NULL) {
        serverPort = atoi(serverPortString);
      }
    }
    else{
      fprintf( stderr, "Error: Usage ./Project4Client [-s <server IP>[:<port>]]\n" );
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
  
  // Establish the connection to the echo server 
  if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0){
    fprintf( stderr, "connect() failed\n" );
    exit(1);
  }

  printf("Welcome Frosty! Please select any functions below:\n");
  printInstructions();

  for(;;){
    // allocates space for the string user enters
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
      break;
    }
    else {
      printf("You entered an invalid command. Here are the instructions:\n");
      printInstructions();
    }
  }

  close(sock); 

}

unsigned long ResolveName(char name[]) {
  struct hostent *host; // Structure containing host information
  if((host = gethostbyname(name)) == NULL){
    fprintf(stderr, "gethostbyname() failed");
    exit(1);
  }
  // Return the binary, network-byte-ordered address
  return *((unsigned long *) host->h_addr_list[0]);
}

void printInstructions(){
  printf("Enter \'list\' to list all the files the server has.\n");
  printf("Enter \'diff\' to show a \"diff\" of the files you have in comparison to the server \n");
  printf("Enter \'sync\' to sync to your device files on the server and vice versa.\n");
  printf("Enter \'bye!\' to leave the program.\n");
}

void list(int sock){
  
}

void diff(int sock){
 
}

void syncFiles(int sock){
 
}

void leave(int sock){
 
}

