# CNET File transfer application
# 
# Shaun Bradley
# Bsc Multimedia Computing yr 3

CC = gcc

CLIENT_SRC = client.c
SERVER_SRC = server.c

CLIENT_BIN = client
SERVER_BIN = server

ARCHIVE = my_tftp-FINAL


all: client server

server:      
	      $(CC) $(SERVER_SRC) -o $(SERVER_BIN)
    
client:	      
	      $(CC) $(CLIENT_SRC) -o $(CLIENT_BIN)
   
clean:	      
	      rm -Rf *.o *~ *.copy $(SERVER_BIN) $(CLIENT_BIN)
	      

backup:
	      tar cf $(ARCHIVE).tar *
	      gzip $(ARCHIVE).tar
	      mv $(ARCHIVE).tar.gz $(HOME)
	      
	      
	     
      	       
