// Shaun Bradley
// 
// File Transfer client, CNET312
// Bsc Multmedia Computing Yr 3



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>    // for gethostbyname()
#include <netdb.h>        // for gethostbyname()

#include <errno.h>
#include <signal.h>      // for alarm signal

#include "my_tftp.h"     // our header, defining packet type and constants


// globals
struct sockaddr_in svr_address,from;   
int sockfd;                            // Socket descriptor 
int len;                               // for storing sizeof() results
struct mypacket_t inpacket;            // for incoming packets
// end of globals


// function prototypes

// in our header
extern void send_error(int id, char *description, int sock, struct sockaddr_in address);    // from our header
extern void client_usage();

// in this module
static void sig_alrm(int sig);    // SIGALRM handler
void client_connect(char *host, int port);  // initialise client, connect to server
void client_get(char *file);      // get a file from server, once connected
// end prototypes



// This function is called when a SIGALRM is raised, when TIMEOUT expires
// it asks for the packet to be resent, because it didnt get it.
static void sig_alrm(int sig) 
{
  
  //printf("<!> SIGALRM encountered\n");
  printf("<r> Asking server to resend ID: %d\n", inpacket.id);
  
  errno = EINTR;   // setup errno?
  
  // send the resend error
  send_error(inpacket.id, "Client recieve timeout\n", sockfd, from);   // send error that we lost packet
  
  //printf("Got signal %d\n", sig);     // debug
  //(void) signal(SIGALRM, SIG_DFL);     // reset to default for signal
  
  //setup another alarm
  alarm(TIMEOUT);
  
}


// this function grabs a socket and connects 
void client_connect(char *server, int port)
{
  
  char ourhostname[256];                 // ourhostname
  char *host;                            // server hostname
  char **addrs;                          // stores the IP's of a host
  struct hostent *hostinfo;              // data struct for host info
  
  
  // Grab a socket from the OS
  printf("<*> Initialising socket\n");
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // setup for UDP datagrams
  if(sockfd == -1) {
    printf("<!> Unable to create Socket.\n");
    exit(1);   // tell the shell
  }                                         // socket now exists, continue
  
  svr_address.sin_family = AF_INET;         // internet family
  
  // get our host name and display 
  gethostname(ourhostname, 255);            // find out our host name
  host = ourhostname;                       // copy into host
  printf("<*> Client running on %s\n", host);
  
  // replace with their hostname
  host = server;    // get hostname to connect to
  
  // get the servers host info
  hostinfo = gethostbyname(host);
  if(!hostinfo) {
    printf("<!> Cannot get host information for %s\n", host);
    exit(1); // tell the shell
  }
  
  // get the port, from cmd args
  // or resort to default port
  if(port <=0 || port >= 5000)
    {
      port = DEFAULT_PORT;
      printf("<!> Bad port, defaulting to %d\n", DEFAULT_PORT);
    }
  // find out values for custom ports

  // get the IP from the hostinfo struct
  addrs = hostinfo->h_addr_list;                  // get IP list from hostinfo struct
  host = inet_ntoa(*(struct in_addr *)*addrs);    // convert
  //  printf("<*> server IP is: %s\n", host);     // debug
  
  // now put  that ip into here
  svr_address.sin_addr.s_addr = inet_addr(host);  // connect to IP specified above
  svr_address.sin_port = htons(port);             // port specified in my header
  len = sizeof(svr_address);                      // get size of struct
  
  // try to connect to server
  printf("<*> Connecting to server %s \n", host);
  
  // connect
  if(connect(sockfd, (struct sockaddr *)&svr_address, len) != 0)  // error
    {
      printf("<!> Error, unable to connect. \n");  // failure
      close(sockfd);
      exit(1);  // tell the shell
    }
  else 
    printf("<*> Connected to %s :port %d.\n", host, port);  // show IP and port of server
  
}



// once connected, this function gets the file
// using our packet passing design
void client_get(char *file)
{
  
  struct mypacket_t outpacket;           // for outgoing packets   
  
  int outfile;                           // output file handle
  char temp[40] = "";                    // for creating new filename
  
  
  //  The "pre-negotiation" stage
  //  now send a request packet with the filename in the data field of the packet
  //  we'll have to wait for an ERR from the server to say the file isnt available
  //  or DATA saying it is, and the file has been chunked
  
  // setup a REQuest packet
  outpacket.id = 0;                    // first packet;
  outpacket.header = REQ;              // header ID
  strcpy(outpacket.data, file); // insert name of REQuested file
  
  // send it
  // printf("<*> Sending packet to server\n"); // debug
  sendto (sockfd, (struct mypacket_t *)&outpacket, sizeof(outpacket),0,(struct sockaddr *)&svr_address, sizeof(svr_address));
  printf("<*> Sending request for %s\n", file);
  
  // wait for the reply from the server
  recvfrom (sockfd, (struct mypacket_t *)&inpacket, sizeof(inpacket),0, (struct sockaddr *)&from, &len);
  // this is the first recv = pre-negotiation
  
  //printf("<*> Packet Header is: %d\n", inpacket.header);   // debug
  
  // ERRor handler
  if(inpacket.header == ERR) {
    close(outfile);                    // close file handle
    close(sockfd);                     // close socket handle
    printf("<!> %s\n",inpacket.data);  // print error description
    exit(1);                           // tell shell
  }
  
  // test if the packet header is data
  if(inpacket.header == DATA) {
    
    // build a filename for the destination file
    strcat(temp, file);         // build filename for the output file
    strcat(temp, ".copy");             // add a ".copy" extension
    
    // try to open destination file for writing
    if ( (outfile = open (temp, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
      {
	printf("<!> Error creating %s\n", temp);         // Client unable to create output file
	
	// tell the server an ERR occurred
	send_error(0,"Could not open file\n", sockfd, from);
	exit(1);                       // Tell the shell
      }
    //printf("%s created.\n", temp);   // File Opened.. Debug
    
    
    // write data chunk to disk, making sure its the correct amount
    if( write(outfile,inpacket.data,inpacket.size) <= 0)
      {
	printf("<!> Error writing to %s\n", temp);      // Cannot write for some reason
	
	//tell the server
	send_error(0,"Could not write to file.\n", sockfd, from);
	exit(1);                                        // Tell the shell
      } 
    
    //printf("writing Packet ID: %d, Packet Size: %d to disk\n", inpacket.id, inpacket.size); // debug
    
    
    // it was data, we have written it, send ACK back
    outpacket.id = inpacket.id;       // we want to send the ID of the packet just written
    outpacket.header = ACK;           // set header to acknowledge
    strcpy(outpacket.data,"");        // empty data buffer
    //printf("Packet ID: %d, Packet Header: %d\n", inpacket.id, inpacket.header);  // Debug
    
    // send ACK to server
    sendto (sockfd, (struct mypacket_t *)&outpacket, sizeof(outpacket),0,(struct sockaddr *)&svr_address, sizeof(svr_address));
    
  } // First REQ-DATA sequence complete
  
  printf("<*> Request permitted - transferring %s\n", file);
  // pre-negotiation finished
  
  
  // pre-negotiation successful, continue into client loop
  // loop now, start sending chunks of the file
  do {
    
    // get a chunk from server
    // set a timeout
    alarm(TIMEOUT);
    recvfrom (sockfd, (struct mypacket_t *)&inpacket, sizeof(inpacket),0, (struct sockaddr *)&from, &len);
    
    // ERRor handler.. just dies on error :(
    // e.g server cannot read file anymore so sends error.. client dies
    // or server got SIGINT, tells client to die
    if(inpacket.header == ERR) {
      close(outfile);                    // close file handle
      close(sockfd);                     // close socket handle
      printf("<!> %s\n",inpacket.data);  // print error description
      exit(1);                           // tell shell 
    }
    
    // try to write it to disk
    if((inpacket.size == 0) && (inpacket.header == END));     // end packet with 0 size, do nothing
    else
      if( write(outfile,inpacket.data,inpacket.size) <= 0)    // write chunk to disk
	{
	  // An error occurred, send an ERR packet to server
	  printf("<!> Error writing to %s\n", temp); 
	  
	  send_error(0,"Error writing to file\n", sockfd, from);
	  
	  // kick the client
	  close(outfile);    // close file handle
	  close(sockfd);     // close socket handle
	  exit(1);
	}
    
    // ACKnowledge we have written that packet
    outpacket.id = inpacket.id;                   // for this packet ID
    outpacket.header = ACK;                       // set header to acknowledged
    strcpy(outpacket.data,"");                    // empty data buffer
    //printf("Packet ID: %d, Packet Header: %d\n", inpacket.id, inpacket.header);    // debug 
    
    
    // send ACK to server
    sendto (sockfd, (struct mypacket_t *)&outpacket, sizeof(outpacket),0,(struct sockaddr *)&svr_address, sizeof(svr_address));
    
    
  } while(inpacket.header != END);  // until END of file packet is served
  // end of file serving loop
  
  printf("<*> File transfer complete.\n");        // signal file transfer done
  
  alarm(0);                                       // zero alarm
  close(outfile);                                 // close output file
  close(sockfd);                                  // clean up after
  
}




int main(int argc, char *argv[])
{
  
  // chain signal to handler function
  signal(SIGALRM, sig_alrm);
  
  
  // 3 args, host/port/file
  if (argc == 4) 
    {
      // e.g client foo.com 8000 bar.mpg

      client_connect(argv[1], atoi(argv[2]) );   // connect to hostname, port
      client_get(argv[3]);                       // get the file, or try

    }
  else client_usage();
  
  
  // end
  return 0;
  
}
