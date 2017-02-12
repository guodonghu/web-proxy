#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

typedef struct sockaddr SA;

#define BUFFER_SIZE 8192

typedef struct {
  int rio_fd;                 //client or server socket fd
  ssize_t rio_cnt;            //unread bytes
  char *rio_bufptr;           //next unread byte
  char rio_buf[BUFFER_SIZE]; 
} rio_t;



#define LISTENQ  1024  //max length for listen socket queue

//network connection functions
void *forwarder(void* args);
void *httpConnection(void* args);
void httpsConnection(int clientfd, rio_t client, char *inHost, int serverPort);

//io fuctions
ssize_t rio_readp(int fd, void *ptr, ssize_t nbytes);
ssize_t rio_writep(int fd, void *ptr, ssize_t nbytes);
ssize_t rio_read(rio_t *rp, char *usrbuf, ssize_t n);
ssize_t rio_readn(int fd, void *usrbuf, ssize_t n);
ssize_t rio_writen(int fd, const void *usrbuf, ssize_t n);
void rio_readinitb(rio_t *rp, int fd); 
ssize_t	rio_readlineb(rio_t *rp, void *usrbuf, ssize_t maxlen);

/* Client/server helper functions */
int open_clientfd(char *hostname, int port);
int open_listenfd(int port);

#endif
