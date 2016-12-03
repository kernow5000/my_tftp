/* my file transfer app defines and packet format
 * 
 * S.Bradley 2003
 *
 */



// Headers
#define REQ   100
#define ACK   101
#define ERR   102
#define END   103
#define DATA  104



#define DEFAULT_PORT 2008        // port number to use for communication

#define TIMEOUT  3               // recvfrom timeout value
#define CHUNK_SIZE 1024          // 1kilobyte chunks

// if I had time, these could be set at the command line
// for varying connections, reliability.



// my packet structure design
struct mypacket_t {
  int id;                    // packet ID
  int header;                // packet Header
  int size;                  // size of the data field below
  
  char data[CHUNK_SIZE];     // data buffer
};



// send an ERRor packet with a description, down a sock
void send_error(int id, char *description, int sock, struct sockaddr_in address) 
{
  struct mypacket_t p;                 // a packet

  p.header = ERR;                      // set ERR in header
  p.id = id;                           // init  packet ID
  p.size = sizeof(description);        // find size of text
  strcpy(p.data, description);         // copy error description into data buffer
  
  // send it
  sendto (sock, (struct mypacket_t *)&p, sizeof(p),0,(struct sockaddr *)&address, sizeof(address));

  return;

}


// display usage information for client program
void client_usage()
{
  // printf("bleh");
  printf("S.Bradley's File transfer client\n");
  printf("Usage:\n");

  printf("client <host> <port> <file to get>\n");
  printf("x.4/2003\n\n");
 
}


// display usage information for server program
void server_usage()
{
  // printf("bleh");
  printf("S.Bradley's File transfer server\n");
  printf("Usage:\n");

  printf("server <port>\n");
  printf("x/4/2003\n\n");

}



