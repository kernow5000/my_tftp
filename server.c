// Shaun Bradley
// 
// File transfer server, CNET312
// Bsc Multimedia computing Yr 3


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "my_tftp.h"                    // our header file


// globals
struct mypacket_t outpacket;            // packet struct for outgoing
struct mypacket_t inpacket;             // packet struct for incoming

int sockfd;                             // socket descriptor  
int infile = 0;                         // file handle for served files
int len;                                // stores sizeof() results
int waiting = 1;                        // server waiting flag
int quit = 0;                           // quit flag

struct sockaddr_in svr_address, from;   // for us, and incoming
// end of globals


// prototypes

// in my header
extern void send_error(int id, char *description, int sock, struct sockaddr_in address);
extern void server_usage();  // usage information

// in this module
void server_init(int port);     // initialise socket + bind etc
void server_run();      // the serving engine
// end prototypes



// signal handler, for cleaning up on control-c
void ctrl_c(int sig) 
{
  
  //printf("waiting = %d\n", waiting);    // debug
  if((waiting == 1) && (infile < 0))      // if waiting for packet AND file is open
    recvfrom (sockfd, (struct mypacket_t *)&inpacket, sizeof(inpacket),0, (struct sockaddr *)&from, &len);
  // discard this packet, its useless
  
  // then carry on dying
  printf("<s> Ctrl-C (SIGINT) encountered - Cleaning up and terminating\n");
  //printf("Got signal %d\n", sig);         // debug
  (void) signal(SIGINT, SIG_DFL);           // reset to default for signal
  
  quit = 1;                                 // flag
  
  // send an error to the client
  //printf("<*> Sending ERR packet to server\n"); // debug
  send_error(0,"Server encountered SIGINT and quit.", sockfd, from);
  
  // now cleanup
  close(infile);       // close current file being served
  close(sockfd);       // close the server socket
  
  printf("<*> Quitting\n");
  
  close(sockfd);
  // now DIE
  exit(0);
  
  // server is now dead
}


// gets socket, binds, or dies on error
void server_init(int port)
{
  
  printf("<*> Initialising socket\n");
  
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);   // UDP datagrams
  if(sockfd == -1) {
    printf("<x> Unable to create Socket.\n");
    exit(1);
  } // socket now exists, continue
  

  // get port from args, or resort to default
  // check port is good choice
  if(port <=0 || port >= 5000) 
    {
      port = DEFAULT_PORT;
      printf("<!> Bad port, defaulting to %d\n", DEFAULT_PORT);
    }
  // find out custom port values

  svr_address.sin_family = AF_INET;              // internet family
  svr_address.sin_addr.s_addr = INADDR_ANY;
  svr_address.sin_port = htons(port);            // port specified in header
  len = sizeof(from);
  

  // bind to port.. or exit gracefully with error
  printf("<*> Binding to port %d.\n", port);
  
  if (bind(sockfd,(struct sockaddr *) &svr_address, sizeof(svr_address)) )
    {
      printf("<!> Error - Unable to Bind\n");  // Unable to bind
      exit(1);                         // tell the shell
    }
  
  // socket is now bound to port
  printf("<*> Server is running.\n");
  
}



// the main serving loop, and packet passing algorithm
void server_run()
{
  
  int id;     // packet id counter/setter
    
  while(!quit) {
    
    
    // wait for clients to connect and request a file
    
    printf("<*> Waiting for client connections\n");
    
    waiting = 1;      // set waiting
    recvfrom (sockfd, (struct mypacket_t *)&inpacket, sizeof(inpacket),0, (struct sockaddr *)&from, &len);
    waiting = 0;      // unset 
    
    // we have a packet, unpackage the packet.. is it a file request?
    printf("<*> File request: %s\n", inpacket.data);
    
    // cleanse the outpacket
    id = inpacket.id;     // setup ID counter, usually 0
    outpacket.header = 0; // empty header
    
    
    // REQ handler
    if(inpacket.header == REQ) {
      
      // try and open the filename in data field 
      if ( (infile = open(inpacket.data, O_RDONLY)) == -1)     // open in binary reading mode
	{
	  printf("<!> Error opening %s\n", inpacket.data);     // cant open file
	  outpacket.id = inpacket.id;                          // for the current packet
	  outpacket.header = ERR;                              // set header to ERR
	  
	  //send ERR to client, tell it to quit
	  send_error(0, "Server can't open file", sockfd, from);
	  
	  close(infile); 
	}
      else 
	printf("<*> %s opened for reading.\n", inpacket.data);  // or success!
    }  
    
    
    // pre-negotiation complete, carry on serving
    
    // as long as there are no errors.. serve
    if(outpacket.header != ERR)
      {
	
	// main client serving loop
	do {
	  
	  // get a new chunk
	  outpacket.size = read(infile,outpacket.data,CHUNK_SIZE);  // read a chunk 
	  
	  id++;               // increment id counter    
	  outpacket.id = id;  // update
	  
	  //printf("outpacket.id = %d, outpacket.size = %d\n", outpacket.id, outpacket.size);   // debug
	  
	  if(outpacket.size == -1) // read error
	    {
	      printf("<!> Error reading file\n");  // Cannot read for some reason
	      
	      //tell the client
	      send_error(0,"Server could not read from file.\n", sockfd, from);   // send error
	      close(infile);
	      // client has been kicked by its own error handler
	    }
	  
	  // find out if its an END packet
	  if((outpacket.size < CHUNK_SIZE) || (outpacket.size == 0)) 
	    { // if chunk is less than chunk_size
	      outpacket.header = END;                       
	      // it must be the last chunk or ERR, so set header to END
	      printf("<*> File served.\n");
	    }
	  else outpacket.header = DATA;   // else just set it to DATA
	  
	  
	  // send the packet, maybe its a resend of the last packet because the client never got it.. 
	  sendto (sockfd, (struct mypacket_t *)&outpacket, sizeof(outpacket),0,(struct sockaddr *)&from, sizeof(from));
	  
	  //wait for an ACKnowledgment from the client
	  waiting = 1;
	  recvfrom (sockfd, (struct mypacket_t *)&inpacket, sizeof(inpacket),0, (struct sockaddr *)&from, &len);
	  waiting = 0;
	  //printf("Ack from client : ID: %d, Packet Header : %s\n", inpacket.id, inpacket.header); // debug
	  
	  
	  //if header is error then say so and reset to waiting state, if ACK then.. carry on
	  if(inpacket.header == ERR && inpacket.id == 0) {
	    printf("<!> Error encountered from client\n");     // got ERR from client :(
	    printf("<!> Error is: %s", inpacket.data);         // display error text from client
	    close(infile);                                     // close the file being served
	    waiting = 1;
	  }
	  // this error handler deals with "normal" client errors.. not resends, just kick the client
	  
	  
	  // or it must be a re-request for a packet
	  if(( inpacket.header == ERR) && (inpacket.id >= 0) )  {
	    printf("<r> Resending ID: %d to client\n", inpacket.id);
	    outpacket.header = DATA;
	    outpacket.id = inpacket.id;
	    
	    // send using our normal send
	    
	  }
	  
	  
	} while(outpacket.header != END || (inpacket.header != ERR && inpacket.id == 0)); // end of file, or (no err and id = 0)
	
	
	infile = close(infile);  // close the served file
	
	
	// another file served
	
      }
    
  }
  close(sockfd);  // close the socket
}





int main(int argc, char *argv[])
{
  
  // chain signal to ctrl_c() function
  (void) signal(SIGINT, ctrl_c);
  
  if(argc == 2) 
    {
      server_init(atoi(argv[1]) );      // initialise/bind to port
      server_run();                     // start running, until ctrl-c or ?
    } 
  else server_usage();  // show usage info 
  
  
  return 0;
  
}
